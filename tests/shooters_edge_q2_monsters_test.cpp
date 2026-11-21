// Tests: Quake II's monsters (games/shooters_edge/server/q2_monsters.cpp) in a hall built
// here: removed in deathmatch; a guard facing a player sees him, turns on him and shoots him; one
// facing away does not, until the player's weapon is heard; shot dead it leaves a corpse shots
// pass, counts as killed, fires its targets and drops its item; a big blow throws it apart; one
// with a path walks it; one waiting hidden appears on a use and turns on the user. The full game's:
// a medic raises a corpse, a brain's screen, a technician's and a super tank's ends, Jorg's death
// bringing out the Makron, a gladiator's rail, the commander's body. A monster facing the shooter
// ducks his bolts and rises again; one with no duck, or facing away, never does.
//
// ge:summary Tests: Quake II's monsters: sight, noise, attack, death, corpse, gibs, path, waiting
// ge:keywords test q2 monsters monster-ai sight noise corpse gib path-corner triggered-spawn
// ge:keywords medic power-screen boss makron dodge duck
// ge:skill shooter testing
// ge:test tests/shooters_edge_q2_monsters_test.cpp
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/q2_move.hpp"
#include "shooters_edge/server/q2_combat.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monster_kinds.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

using Pairs = std::vector<std::pair<std::string, std::string>>;

br::Entity entity(Pairs pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A long floored hall, the player's start at its west end, and `monsters` placed in it.
std::shared_ptr<const br::Map> hall(const std::vector<Pairs>& monsters) {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-1024, -256, -16}, hi[3] = {1024, 256, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-600 0 24"}, {"angle", "0"}}));
  for (const Pairs& m : monsters) w.entities.push_back(entity(m));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

struct Hall {
  sv::RulesRunner r;
  uint32_t sequence = 0;
  sv::Q2Server& s() const { return *sv::q2_of(r); }
  sv::Q2Player& p() const { return s().players[0]; }
  sv::Q2Monster& m(int i = 0) const { return s().monsters[size_t(i)]; }
  sv::Q2Entity& e(int i = 0) const { return s().entities[size_t(m(i).entity)]; }
};

std::unique_ptr<Hall> open_hall(const std::vector<Pairs>& monsters, const char* dm = "0",
                                const char* setting = nullptr) {
  auto h = std::make_unique<Hall>();
  h->r = sv::q2_runner(sv::make_q2_server({}));
  h->r.console(std::string("deathmatch ") + dm);
  if (setting != nullptr) h->r.console(setting);
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall(monsters);
  std::string error;
  CHECK_MSG(h->r.start(map, error), error);
  h->r.connect(0, "Ana", false, false);
  return h;
}

// Host ticks of 13 ms, Ana looking along `yaw` with `buttons`.
void run(Hall& h, int ticks, uint8_t buttons = 0, float yaw = 0) {
  for (int i = 0; i < ticks; i++) {
    sh::Packet pk;
    pk.client = 0;
    pk.sequence = ++h.sequence;
    sh::MoveCmd c;
    c.msec = 13;
    c.buttons = buttons;
    c.angles = {0, yaw, 0};
    pk.cmds[0] = pk.cmds[1] = pk.cmds[2] = c;
    h.r.frame(&pk, 1);
  }
}

Pairs guard(const char* origin, const char* angle) {
  return {{"classname", "monster_soldier_light"}, {"origin", origin}, {"angle", angle}};
}

constexpr int kSecond = 77;  // host ticks of 13 ms in a second

}  // namespace

TEST(monsters_are_not_spawned_in_deathmatch) {
  auto h = open_hall({guard("0 0 24", "180")}, "1");
  run(*h, 10);
  CHECK(h->s().monsters.empty());
  CHECK(h->s().monsters_total == 0);
}

TEST(a_guard_facing_the_player_sees_him_and_shoots_him) {
  auto h = open_hall({guard("0 0 24", "180")});
  run(*h, 10);
  CHECK(h->s().monsters_total == 1);
  CHECK_MSG(h->e().origin.z >= 24 && h->e().origin.z < 24.1f,
            std::to_string(h->e().origin.z));  // on the floor
  run(*h, kSecond);
  CHECK(h->m().enemy == 0);
  for (int i = 0; i < 30 && h->p().alive && h->p().health == 100; i++) run(*h, kSecond);
  CHECK_MSG(h->p().health < 100, std::to_string(h->p().health));
  CHECK(h->e().origin.x < 0);  // it came on toward him
}

TEST(a_guard_facing_away_is_deaf_to_nothing_and_hears_a_shot) {
  auto h = open_hall({guard("0 0 24", "0")});
  run(*h, 3 * kSecond);
  CHECK(h->m().enemy == -1);
  run(*h, kSecond / 2, sh::kButtonAttack, 180);  // the blaster, fired away: heard
  CHECK(h->m().sound_target || h->m().enemy == 0);
  for (int i = 0; i < 5 * kSecond && h->m().enemy != 0; i++) run(*h, 1);
  CHECK(h->m().enemy == 0);  // turned to it, it sees her
}

TEST(a_guard_shot_dead_leaves_a_corpse_counts_and_drops_its_item) {
  Pairs g = guard("0 0 24", "0");
  g.push_back({"item", "ammo_shells"});
  auto h = open_hall({g});
  run(*h, 10);
  // Ana's blaster at it down the hall until it falls.
  for (int i = 0; i < 20 && !h->m().dead; i++) run(*h, kSecond / 2, sh::kButtonAttack);
  CHECK(h->m().dead && h->e().corpse && !h->e().gone);
  CHECK(h->s().monsters_killed == 1);
  bool dropped = false;
  for (const sv::Q2Drop& d : h->s().drops)
    if (d.live && d.item == sv::sq_item("ammo_shells")) dropped = true;
  CHECK(dropped);
  // A move passes the corpse; a shot meets it, and enough of them throw it apart.
  const ge::classic::Q2Trace t =
      sv::map_trace(h->s(), {-100, 0, 4}, {}, {}, {100, 0, 4}, sh::kSqSolid | sh::kSqMonster);
  CHECK(t.fraction == 1);
  const ge::classic::Q2Trace shot =
      sv::map_trace(h->s(), {-100, 0, 4}, {}, {}, {100, 0, 4}, sh::kSqSolid | sh::kSqDeadMonster);
  CHECK(shot.fraction < 1);
  sv::fight_blast(h->s(), h->e().origin, 0, 100, 140, int(sv::SqWeapon::rocket_launcher));
  CHECK_MSG(h->e().gone, "a blast beside the corpse did not throw it apart");
}

TEST(a_big_blow_throws_a_monster_apart) {
  auto h = open_hall({guard("0 0 24", "0")});
  run(*h, 10);
  CHECK(sv::map_damage(h->s(), h->m().entity, 0, 200));
  CHECK(h->m().dead && h->e().gone);
  run(*h, 1);
  CHECK(h->r->world.entities[size_t(sv::map_row(h->m().entity))].free);
}

TEST(a_monster_with_a_path_walks_it_until_it_finds_an_enemy) {
  auto h = open_hall({{{"classname", "monster_infantry"},
                       {"origin", "800 0 24"},
                       {"angle", "0"},
                       {"target", "c1"}},
                      {{"classname", "path_corner"}, {"origin", "500 0 8"}, {"targetname", "c1"}}});
  run(*h, 10);
  const float x0 = h->e().origin.x;
  run(*h, 3 * kSecond);  // 50 a second toward the corner, Ana beyond its sight
  CHECK_MSG(h->e().origin.x < x0 - 100, std::to_string(h->e().origin.x));
  CHECK(h->m().enemy == -1);
}

TEST(a_waiting_monster_appears_on_a_use_and_turns_on_the_user) {
  Pairs g = guard("0 0 24", "0");
  g.push_back({"spawnflags", "2"});
  g.push_back({"targetname", "ambush"});
  auto h = open_hall({g});
  run(*h, 10);
  CHECK(h->m().waiting && h->e().hidden && !h->e().box);
  CHECK(h->r->world.entities[size_t(sv::map_row(h->m().entity))].free);
  sv::monster_use(h->s(), h->m().entity, 0);  // used by Ana, as her trigger would
  CHECK(!h->m().waiting && h->e().box && h->m().enemy == 0);
  run(*h, 1);
  CHECK(!h->r->world.entities[size_t(sv::map_row(h->m().entity))].free);
}

// ---- The full game's (q2-monsters-full.md) ----

namespace {

Pairs monster(const char* classname, const char* origin, const char* angle) {
  return {{"classname", classname}, {"origin", origin}, {"angle", angle}};
}

int find(const Hall& h, const char* classname) {
  for (size_t i = 0; i < h.s().monsters.size(); i++)
    if (h.s().entities[size_t(h.s().monsters[i].entity)].classname == classname)
      return static_cast<int>(i);
  return -1;
}

}  // namespace

TEST(a_medic_raises_a_dead_guard_who_counts_again) {
  // Both face away from Ana, 800 off: neither sees her.
  auto h = open_hall({guard("100 0 24", "0"), monster("monster_medic", "250 0 24", "0")});
  run(*h, 10);
  CHECK(h->s().monsters_total == 2);
  // 12, twice over on a guard that has seen no one: dead, not thrown apart.
  CHECK(sv::map_damage(h->s(), h->m(0).entity, 0, 12));
  CHECK(h->m(0).dead && !h->e(0).gone);
  for (int i = 0; i < 10 && h->m(0).dead; i++) run(*h, kSecond);
  CHECK_MSG(!h->m(0).dead, "the medic never raised the guard");
  CHECK(h->e(0).health == 20 && h->e(0).box);
  CHECK(h->s().monsters_total == 3 && h->s().monsters_killed == 1);
}

TEST(a_brains_screen_takes_a_third_of_a_blow_from_in_front) {
  auto h = open_hall({monster("monster_brain", "0 0 24", "180")});
  run(*h, 2);
  CHECK(sv::monster_damage(h->s(), h->m().entity, -1, 30, {1, 0, 0}));  // into its face
  CHECK_MSG(h->e().health == 280, std::to_string(h->e().health));
  CHECK(sv::monster_damage(h->s(), h->m().entity, -1, 30, {-1, 0, 0}));  // into its back
  CHECK_MSG(h->e().health == 250, std::to_string(h->e().health));
}

TEST(a_technician_explodes_and_a_super_tank_burns_down_to_a_wreck) {
  auto h = open_hall(
      {monster("monster_floater", "0 100 60", "0"), monster("monster_supertank", "300 0 24", "0")});
  run(*h, 2);
  CHECK(sv::map_damage(h->s(), h->m(0).entity, 0, 500));
  CHECK(h->m(0).dead && h->e(0).gone);  // nothing left
  CHECK(sv::map_damage(h->s(), h->m(1).entity, 0, 5000));
  CHECK(h->m(1).dead && !h->e(1).gone);  // dying
  run(*h, 2 * kSecond);
  CHECK(!h->e(1).gone);
  run(*h, 2 * kSecond);
  CHECK(h->e(1).gone);  // blown apart after its death and eight blasts
}

TEST(jorgs_death_brings_out_the_makron) {
  auto h = open_hall({monster("monster_jorg", "300 0 24", "0")});
  run(*h, 2);
  CHECK(h->s().monsters_total == 1);
  const int makron = find(*h, "monster_makron");
  CHECK(makron >= 0 && h->m(makron).waiting && h->e(makron).hidden);
  CHECK(sv::map_damage(h->s(), h->m(0).entity, 0, 5000));
  run(*h, 4 * kSecond);
  CHECK(h->m(makron).waiting);
  run(*h, 2 * kSecond);
  CHECK(!h->m(makron).waiting && !h->e(makron).hidden && h->e(0).gone);
  CHECK(h->s().monsters_total == 2);
}

TEST(a_gladiators_rail_hits_for_fifty) {
  auto h = open_hall({monster("monster_gladiator", "-300 0 24", "180")});
  for (int i = 0; i < 30 && h->p().alive && h->p().health == 100; i++) run(*h, kSecond / 2);
  CHECK_MSG(h->p().health == 50 || !h->p().alive, std::to_string(h->p().health));
}

TEST(the_commanders_body_stays_in_deathmatch_uncounted_and_slumps_when_used) {
  auto h = open_hall({monster("monster_commander_body", "0 0 24", "0")}, "1");
  run(*h, 10);
  CHECK(h->s().monsters.size() == 1 && h->s().monsters_total == 0 && !h->e().gone);
  sv::monster_use(h->s(), h->m().entity, 0);
  run(*h, 3 * kSecond);
  CHECK(h->r->world.entities[size_t(sv::map_row(h->m().entity))].frame == 24);
  CHECK(sv::map_damage(h->s(), h->m().entity, 0, 500));  // a figure: never hurt
  CHECK(!h->e().gone && !h->m().dead);
}

namespace {

// The named line effects the server sent over `seconds`, by name.
std::vector<std::string> lines_sent(Hall& h, int seconds) {
  std::vector<std::string> names;
  for (int i = 0; i < seconds * kSecond; i++) {
    h.s().out.events.clear();
    run(h, 1);
    for (const sv::NetEvent& ev : h.s().out.events)
      if (ev.kind == sv::EventKind::trail_effect) names.push_back(ev.text);
  }
  return names;
}

bool has(const std::vector<std::string>& v, const char* name) {
  for (const std::string& s : v)
    if (s == name) return true;
  return false;
}

}  // namespace

TEST(a_guards_shot_leaves_with_a_blaster_flash_the_window_draws) {
  auto h = open_hall({guard("0 0 24", "180")});
  const std::vector<std::string> sent = lines_sent(*h, 8);
  CHECK_MSG(has(sent, "te_monster_flash.blaster"), sent.empty() ? "none" : sent[0]);
}

TEST(a_parasites_drain_and_a_medics_cable_are_sent_as_lines) {
  auto p = open_hall({monster("monster_parasite", "-450 0 24", "180")});
  CHECK(has(lines_sent(*p, 8), "te_parasite_attack"));
  auto m = open_hall({guard("100 0 24", "0"), monster("monster_medic", "250 0 24", "0")});
  run(*m, 10);
  CHECK(sv::map_damage(m->s(), m->m(0).entity, 0, 12));  // twice over: dead, whole
  CHECK(has(lines_sent(*m, 6), "te_medic_cable_attack"));
}

// Classic remix's `cr_monsters q1` under Quake II's rules: Quake's monsters are rows of
// the roster with the classic campaign's numbers, so a rifleman, a brute and a fiend spawn with
// its health and hurt the player before them.
TEST(the_classic_monsters_run_by_q2_rules) {
  auto h = open_hall({monster("monster_army", "-300 -96 24", "180"),
                      monster("monster_ogre", "-300 96 40", "180"),
                      monster("monster_demon1", "-420 0 40", "180")});
  run(*h, 10);
  CHECK(h->s().monsters_total == 3);
  const int army = find(*h, "monster_army"), ogre = find(*h, "monster_ogre"),
            fiend = find(*h, "monster_demon1");
  CHECK(army >= 0 && ogre >= 0 && fiend >= 0);
  if (army < 0 || ogre < 0 || fiend < 0) return;
  CHECK(h->e(army).health == 30 && h->e(ogre).health == 200 && h->e(fiend).health == 300);
  for (int i = 0; i < 30 && h->p().alive && h->p().health == 100; i++) run(*h, kSecond);
  CHECK_MSG(h->p().health < 100 || !h->p().alive, std::to_string(h->p().health));
  CHECK(h->m(army).pose.span.last > 0 && h->m(ogre).pose.span.last > 0);  // their own runs
}

TEST(the_classic_spawn_bursts_on_whoever_is_near_under_q2_rules) {
  auto h = open_hall({monster("monster_tarbaby", "-560 0 40", "180")});
  run(*h, 2);
  const int spawn = find(*h, "monster_tarbaby");
  CHECK(spawn >= 0);
  if (spawn < 0) return;
  const float before = h->p().health;
  CHECK(sv::map_damage(h->s(), h->m(spawn).entity, 0, 200));  // its death, here
  run(*h, 2);
  CHECK(h->m(spawn).dead && h->e(spawn).gone);
  CHECK_MSG(h->p().health < before, std::to_string(h->p().health));  // caught by the burst
}

TEST(a_monster_facing_the_shooter_ducks_his_bolts_and_rises_again) {
  auto h = open_hall({{{"classname", "monster_infantry"}, {"origin", "0 0 24"}, {"angle", "180"}}});
  run(*h, 10);
  const float top = h->e().maxs.z;
  bool ducked = false;
  for (int i = 0; i < 6 * kSecond && !ducked && h->p().alive; i++) {
    run(*h, 1, sh::kButtonAttack);  // the blaster, straight at it
    ducked = h->m().ducked;
  }
  CHECK(ducked);
  CHECK_MSG(h->e().maxs.z == top - 32, std::to_string(h->e().maxs.z));  // its top lowered
  CHECK(h->m().enemy == 0);
  run(*h, 2 * kSecond);  // no more bolts: up again after its hold
  CHECK(!h->m().ducked);
  CHECK(h->e().maxs.z == top);
}

TEST(a_monster_without_a_duck_or_facing_away_never_ducks) {
  // A berserker faces her but has no duck; an infantryman alone faces away (none sees her to tell
  // him).
  const char* kinds[2][2] = {{"monster_berserk", "180"}, {"monster_infantry", "0"}};
  for (const auto& k : kinds) {
    auto h = open_hall({{{"classname", k[0]}, {"origin", "0 0 24"}, {"angle", k[1]}}});
    run(*h, 10);
    for (int i = 0; i < 2 * kSecond; i++) {
      sv::monster_dodge(h->s(), h->m().entity, 0, 600, 1000);  // every offer taken by its roll
      run(*h, 1);
      CHECK_MSG(!h->m().ducked && h->m().duck_frame < 0, k[0]);
    }
  }
}

// A monster set to wait for a use with no name to be used by is said on the console, and stands
// from the start with g_fix_triggered 1.
TEST(g_fix_triggered_stands_a_waiting_monster_nothing_can_wake) {
  Pairs g = guard("0 0 24", "0");
  g.push_back({"spawnflags", "2"});
  auto as_made = open_hall({g});
  run(*as_made, 10);
  CHECK(as_made->m().waiting && as_made->e().hidden);
  bool said = false;
  for (const std::string& l : as_made->s().console)
    said = said || l == "triggered monster_soldier_light at (0 0 24) has no targetname";
  CHECK(said);
  auto fixed = open_hall({g}, "0", "g_fix_triggered 1");
  run(*fixed, 10);
  CHECK(!fixed->m().waiting && !fixed->e().hidden && fixed->e().box);
}

// g_commanderbody_nogod 1: the first blow throws the commander's body apart.
TEST(g_commanderbody_nogod_lets_the_first_blow_throw_the_body_apart) {
  auto h =
      open_hall({monster("monster_commander_body", "0 0 24", "0")}, "0", "g_commanderbody_nogod 1");
  run(*h, 10);
  CHECK(sv::map_damage(h->s(), h->m().entity, 0, 10));
  CHECK(h->e().gone);
  run(*h, 1);
  CHECK(h->r->world.entities[size_t(sv::map_row(h->m().entity))].free);
}

namespace {

// The step sounds a walking `classname` on its path made over three seconds.
std::vector<std::string> walker_sounds(const char* setting,
                                       const char* classname = "monster_infantry") {
  auto h = open_hall(
      {{{"classname", classname}, {"origin", "800 0 24"}, {"angle", "0"}, {"target", "c1"}},
       {{"classname", "path_corner"}, {"origin", "500 0 8"}, {"targetname", "c1"}}},
      "0", setting);
  std::vector<std::string> names;
  for (int i = 0; i < 3 * kSecond; i++) {
    h->s().out.events.clear();
    run(*h, 1);
    for (const sv::NetEvent& ev : h->s().out.events)
      if (ev.kind == sv::EventKind::sound && ev.text.find("step") != std::string::npos)
        names.push_back(ev.text);
  }
  return names;
}

}  // namespace

TEST(g_monsterfootsteps_lets_a_walking_infantry_step_on_its_walks_frames_and_is_off_by_default) {
  CHECK(walker_sounds(nullptr).empty());
  const std::vector<std::string> steps = walker_sounds("g_monsterfootsteps 1");
  // Two step frames in each round of its walk's frames: a few in three seconds.
  CHECK_MSG(steps.size() >= 2 && steps.size() <= 6, std::to_string(steps.size()));
  for (const std::string& name : steps)
    CHECK_MSG(name == "infantry/step1.wav" || name == "infantry/step2.wav", name);
}

TEST(a_walking_tank_steps_whatever_g_monsterfootsteps_says) {
  const std::vector<std::string> steps = walker_sounds(nullptr, "monster_tank");
  CHECK_MSG(steps.size() >= 2, std::to_string(steps.size()));
  for (const std::string& name : steps) CHECK_MSG(name == "tank/step.wav", name);
}

namespace {

// Ana's hand on weapon `w` with `ammo`, raised and ready.
void arm(Hall& h, sv::SqWeapon w, int ammo) {
  const sv::SqWeaponRow& r = sv::kSqWeapons[int(w)];
  h.p().body.counts[sv::sq_item(r.item)] = 1;
  h.p().body.ammo[int(r.ammo)] = ammo;
  h.r.client_command(0, std::string("use ") + r.name);
  run(h, 100);
}

}  // namespace

TEST(a_first_blow_on_a_monster_that_has_seen_no_one_counts_twice) {
  auto h = open_hall({monster("monster_gunner", "0 0 24", "0")});
  run(*h, 10);
  const int full = h->e().health;
  CHECK(sv::map_damage(h->s(), h->m().entity, 0, 10));
  CHECK_MSG(h->e().health == full - 20, std::to_string(h->e().health));
  CHECK(h->m().enemy == 0);  // now it has seen her: the next blow counts once
  CHECK(sv::map_damage(h->s(), h->m().entity, 0, 10));
  CHECK(h->e().health == full - 30);
  // A blast's blow never counts twice.
  auto b = open_hall({monster("monster_gunner", "0 0 24", "0")});
  run(*b, 10);
  CHECK(sv::map_damage(b->s(), b->m().entity, 0, 10, {1, 0, 0}, true));
  CHECK(b->e().health == full - 10);
}

TEST(the_railgun_passes_through_every_monster_in_line) {
  auto h = open_hall({guard("0 0 24", "0"), guard("200 0 24", "0")});
  arm(*h, sv::SqWeapon::railgun, 10);
  run(*h, 8, sh::kButtonAttack);
  CHECK_MSG(h->m(0).dead && h->m(1).dead, "one slug drops both guards in its line");
}

TEST(a_rocket_on_a_monster_hurts_it_once_and_its_blast_leaves_it_out) {
  auto h = open_hall({monster("monster_tank", "0 0 24", "0")});
  run(*h, 10);
  h->m().enemy = 0;  // it has seen her: no first blow's double
  const int full = h->e().health;
  arm(*h, sv::SqWeapon::rocket_launcher, 10);
  h->m().enemy = 0;
  run(*h, 8, sh::kButtonAttack);
  run(*h, 2 * kSecond);
  const int lost = full - h->e().health;
  CHECK_MSG(lost >= 100 && lost <= 120, std::to_string(lost));  // 100 to 120 direct, no blast
}

TEST(a_monsters_death_fires_its_deathtarget_in_place_of_its_target) {
  Pairs g = guard("0 0 24", "0");
  g.push_back({"deathtarget", "won"});
  auto h =
      open_hall({g, {{"classname", "target_goal"}, {"origin", "0 100 24"}, {"targetname", "won"}}});
  run(*h, 10);
  CHECK(h->s().goals == 1 && h->s().goals_found == 0);
  CHECK(sv::map_damage(h->s(), h->m().entity, 0, 15));
  run(*h, 2);
  CHECK(h->m().dead && h->s().goals_found == 1);
}

TEST(a_flyer_bursts_at_death_and_leaves_nothing) {
  auto h = open_hall({monster("monster_flyer", "0 0 64", "0")});
  run(*h, 10);
  CHECK(sv::map_damage(h->s(), h->m().entity, 0, 40));
  CHECK(h->m().dead && h->e().gone);
}

TEST(a_monster_hurt_by_another_kind_turns_on_it_and_back_to_the_player_after) {
  auto h = open_hall(
      {monster("monster_gunner", "0 0 24", "0"), monster("monster_infantry", "200 0 24", "180")});
  run(*h, 10);
  // The gunner hunts Ana; the enforcer is hit by the gunner: they walk alike and differ.
  sv::map_damage(h->s(), h->m(0).entity, 0, 1);
  CHECK(h->m(0).enemy == 0);
  const int gunner = -2 - h->m(0).entity;
  CHECK(sv::map_damage(h->s(), h->m(1).entity, gunner, 5));
  CHECK_MSG(h->m(1).enemy == gunner, std::to_string(h->m(1).enemy));
  // The gunner, hit back by the one it was not aiming at, turns on it too.
  CHECK(sv::map_damage(h->s(), h->m(0).entity, -2 - h->m(1).entity, 5));
  CHECK(h->m(0).enemy == -2 - h->m(1).entity && h->m(0).old_enemy == 0);
  // Its foe dead, it goes back to Ana.
  sv::map_damage(h->s(), h->m(1).entity, 0, 100000);
  run(*h, 2 * kSecond / 10 + 10);
  CHECK_MSG(h->m(0).enemy == 0, std::to_string(h->m(0).enemy));
}

TEST(the_same_kind_or_a_heavy_kind_does_not_start_a_fight_but_its_target_is_joined) {
  auto h = open_hall({monster("monster_gunner", "0 0 24", "0"),
                      monster("monster_gunner", "200 0 24", "180"),
                      monster("monster_tank", "-300 0 24", "0")});
  run(*h, 10);
  sv::map_damage(h->s(), h->m(0).entity, 0, 1);  // the first gunner hunts Ana
  CHECK(sv::map_damage(h->s(), h->m(1).entity, -2 - h->m(0).entity, 5));
  CHECK_MSG(h->m(1).enemy == 0, std::to_string(h->m(1).enemy));           // joins it against Ana
  CHECK(sv::map_damage(h->s(), h->m(0).entity, -2 - h->m(2).entity, 5));  // the tank's splash
  CHECK(h->m(0).enemy == 0);  // a heavy kind's blow is let pass
}

TEST(a_monsters_shot_that_hits_another_monster_counts_as_its_blow) {
  auto h = open_hall(
      {monster("monster_gunner", "0 0 24", "0"), monster("monster_infantry", "200 0 24", "180")});
  run(*h, 10);
  sv::fight_monster_bullet(h->s(), h->m(0).entity, h->e(0).origin + sh::Vec3f{20, 0, 0},
                           h->e(1).origin, 3, 0, 0);
  CHECK_MSG(h->m(1).enemy == -2 - h->m(0).entity, std::to_string(h->m(1).enemy));
}

// ---- Monsters rank 2 (q2-sp-monsters.md 23, 24, 25, 27, 33, 35, 37, 43, 44) ----

namespace {

// The hall with a pool of `contents` 128 wide and 80 deep round the hall's middle.
std::unique_ptr<Hall> open_pool(const std::vector<Pairs>& monsters, int32_t contents) {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-1024, -256, -16}, hi[3] = {1024, 256, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  const float plo[3] = {-64, -64, 0}, phi[3] = {64, 64, 80};
  w.models[0].push_back(br::box_brush(plo, phi, contents, "world/liquid"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-600 0 24"}, {"angle", "0"}}));
  for (const Pairs& m : monsters) w.entities.push_back(entity(m));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  auto h = std::make_unique<Hall>();
  h->r = sv::q2_runner(sv::make_q2_server({}));
  h->r.console("deathmatch 0");
  sv::MapSpec spec;
  spec.name = "pool";
  spec.brushes = map;
  CHECK_MSG(h->r.start(spec, error), error);
  h->r.connect(0, "Ana", false, false);
  return h;
}

}  // namespace

TEST(a_monster_in_lava_burns_ten_a_level_five_times_a_second) {
  auto h = open_pool({monster("monster_gunner", "0 0 24", "0")}, sh::kSqLava);
  run(*h, 10);
  const int full = h->e().health;
  run(*h, kSecond);
  CHECK(h->m().water_level == 3 && h->m().in_water);
  const int lost = full - h->e().health;
  CHECK_MSG(lost % 30 == 0 && lost >= 120 && lost <= 180, std::to_string(lost));
}

TEST(a_monster_in_slime_burns_four_a_level_each_second_and_in_water_drowns_after_twelve) {
  auto h = open_pool({monster("monster_gunner", "0 0 24", "0")}, sh::kSqSlime);
  run(*h, 10);
  const int full = h->e().health;
  run(*h, 2 * kSecond);
  const int lost = full - h->e().health;
  CHECK_MSG(lost % 12 == 0 && lost >= 24 && lost <= 36, std::to_string(lost));
  auto w = open_pool({monster("monster_gunner", "0 0 24", "0")}, sh::kSqWater);
  run(*w, 11 * kSecond);
  const int before = w->e().health;
  CHECK(before == w->s().entities[size_t(w->m().entity)].health);
  CHECK_MSG(before > 170, std::to_string(before));  // air for 12 s: unhurt
  run(*w, 3 * kSecond);
  const int drowned = before - w->e().health;
  CHECK_MSG(drowned >= 2 && drowned <= 12, std::to_string(drowned));  // 2, then 4, then 6
}

TEST(using_an_awake_monster_with_no_enemy_angers_it_and_an_ambusher_appears_without_one) {
  auto h = open_hall({guard("0 0 24", "0")});
  run(*h, 10);
  CHECK(h->m().enemy == -1);
  sv::monster_use(h->s(), h->m().entity, 0);
  CHECK(h->m().enemy == 0);
  Pairs g = guard("0 0 24", "0");
  g.push_back({"spawnflags", "3"});  // triggered, ambush
  g.push_back({"targetname", "ambush"});
  auto a = open_hall({g});
  run(*a, 10);
  sv::monster_use(a->s(), a->m().entity, 0);
  CHECK(!a->m().waiting && a->m().enemy == -1);
}

TEST(an_impact_is_heard_by_a_monster_with_no_enemy_but_not_by_an_ambusher) {
  auto h = open_hall({guard("0 0 24", "0")});
  run(*h, 10);
  sv::monster_noise(h->s(), 0, {200, 0, 24}, true);
  run(*h, 10);
  CHECK(h->m().sound_target && h->m().enemy == -1);  // a noise is a place to go, not an enemy
  Pairs g = guard("0 0 24", "0");
  g.push_back({"spawnflags", "1"});  // ambush
  auto a = open_hall({g});
  run(*a, 10);
  sv::monster_noise(a->s(), 0, {200, 0, 24}, true);
  run(*a, 10);
  CHECK(!a->m().sound_target);
  sv::monster_noise(a->s(), 0, {200, 0, 24}, false);  // her own noise, where it sees: heard
  run(*a, 10);
  CHECK(a->m().sound_target);
}

TEST(the_first_grenades_held_are_taken_in_hand_in_single_player) {
  auto h = open_hall({});
  run(*h, 10);
  const int box = sv::sq_item("ammo_grenades");
  arm(*h, sv::SqWeapon::shotgun, 10);
  CHECK(sv::sq_pick_up(box, h->p().body, false, 1, 0));
  sv::fight_took(h->s(), 0, box);
  CHECK(h->p().hand.pending == int(sv::SqWeapon::hand_grenade));
  h->p().hand.pending = -1;
  CHECK(sv::sq_pick_up(box, h->p().body, false, 1, 0));  // more: no switch
  sv::fight_took(h->s(), 0, box);
  CHECK(h->p().hand.pending == -1);
}

TEST(a_berserker_has_two_blows_and_a_tank_three_volleys_by_range) {
  namespace ro = sv::roster;
  for (const ro::Kind& k : ro::kKinds) {
    const std::string_view name = k.classname;
    if (name == "monster_berserk") {
      CHECK(k.melee[0].damage == 15 && k.melee[1].damage == 5 && k.melee[1].damage_max == 10);
    }
    if (name == "monster_tank") {
      CHECK(k.by_range && k.missile[0].shots == 3 && k.missile[1].shots == 3);
      CHECK(k.missile[0].again_skill == 2 && k.missile[2].burst_min == 1.9f);
    }
    if (name == "monster_soldier_light") CHECK(k.missile[0].aim_h == 1000);
    if (name == "monster_soldier_ss") CHECK(k.missile[0].h == 300 && k.missile[0].aim_v == 500);
  }
}

TEST(a_path_corner_holds_its_wait_fires_its_pathtarget_and_a_flagged_next_is_a_jump) {
  Pairs g = guard("0 0 24", "0");
  g.push_back({"target", "c1"});
  Pairs hidden = guard("400 200 24", "0");
  hidden.push_back({"spawnflags", "2"});
  hidden.push_back({"targetname", "late"});
  auto h =
      open_hall({g,
                 hidden,
                 {{"classname", "path_corner"},
                  {"origin", "100 0 8"},
                  {"targetname", "c1"},
                  {"target", "c2"},
                  {"wait", "3"},
                  {"pathtarget", "late"}},
                 {{"classname", "path_corner"},
                  {"origin", "500 -100 8"},
                  {"targetname", "c2"},
                  {"spawnflags", "1"},
                  {"target", "c3"}},
                 {{"classname", "path_corner"}, {"origin", "700 -100 8"}, {"targetname", "c3"}}});
  const int walker = find(*h, "monster_soldier_light");
  run(*h, 10);
  for (int i = 0; i < 6 * kSecond && h->m(walker).corner_wait == 0; i++) run(*h, 1);
  CHECK(h->m(walker).corner_wait > 0);
  CHECK(!h->m(1).waiting);  // the corner's pathtarget brought it out
  const sv::Q2Entity& e = h->e(walker);
  const float x = e.origin.x;
  run(*h, 2 * kSecond);
  CHECK_MSG(std::abs(h->e(walker).origin.x - x) < 1, std::to_string(h->e(walker).origin.x));
  run(*h, 2 * kSecond);  // the wait over: c2 is reached at once, on toward c3
  CHECK_MSG(h->e(walker).origin.x > 490, std::to_string(h->e(walker).origin.x));
}

TEST(a_monster_waits_a_second_after_finding_him_before_its_first_shot) {
  auto h = open_hall({guard("-200 0 24", "180")});
  run(*h, 10);
  sv::monster_use(h->s(), h->m().entity, 0);
  CHECK(h->m().attack_finished >= h->s().level_time + 0.9);
}

TEST(a_tank_and_a_chick_leave_their_own_corpse_boxes) {
  auto h = open_hall(
      {monster("monster_tank", "0 0 40", "0"), monster("monster_chick", "0 150 24", "0")});
  run(*h, 10);
  for (int i = 0; i < 2; i++) CHECK(sv::map_damage(h->s(), h->m(i).entity, -1, h->e(i).health));
  CHECK(h->m(0).dead && h->e(0).mins.z == -16 && h->e(0).maxs.z == 0);
  CHECK(h->m(1).dead && h->e(1).mins.z == 0 && h->e(1).maxs.z == 16);
}

TEST(the_trail_drops_a_mark_where_he_stood_once_he_cannot_see_the_newest) {
  // A block in the hall's middle hides the world's origin, where the empty trail's marks lie.
  auto h = open_pool({monster("monster_gunner", "600 0 24", "180")}, sh::kSqSolid);
  run(*h, 3 * kSecond);
  int marks = 0;
  for (double t : h->s().trail_time) marks += t > 0 ? 1 : 0;
  CHECK_MSG(marks == 1, std::to_string(marks));  // one, then he sees it: no more
  const int newest = (h->s().trail_head + 7) % 8;
  CHECK(std::abs(h->s().trail_at[newest].x + 600) < 1);
}

TEST(a_tank_says_its_idle_now_and_then_and_a_flyer_carries_its_engine) {
  auto h = open_hall({monster("monster_tank", "300 200 40", "90"),
                      monster("monster_flyer", "300 -200 64", "270")});
  int idles = 0;
  for (int i = 0; i < 40 * kSecond / 8; i++) {
    const int64_t before = h->s().out.events_total;
    run(*h, 8);
    const auto& ev = h->s().out.events;
    for (size_t k = 0; k < ev.size(); k++)
      if (h->s().out.events_total - int64_t(ev.size() - k) >= before &&
          ev[k].text == "tank/tnkidle1.wav")
        idles++;
  }
  CHECK_MSG(idles >= 1 && idles <= 3, std::to_string(idles));  // 15 to 30 s apart
  const int row = sv::map_row(h->m(1).entity);
  const int loop = h->r->world.entities[size_t(row)].loop;
  CHECK(loop > 0 && h->s().sounds[size_t(loop)] == "flyer/flyidle1.wav");
}

namespace {

// The sounds the server sent over `ticks` host ticks, by name.
std::vector<std::string> sounds_sent(Hall& h, int ticks) {
  std::vector<std::string> names;
  for (int i = 0; i < ticks; i++) {
    h.s().out.events.clear();
    run(h, 1);
    for (const sv::NetEvent& ev : h.s().out.events)
      if (ev.kind == sv::EventKind::sound) names.push_back(ev.text);
  }
  return names;
}

}  // namespace

TEST(a_soldier_far_off_fires_on_the_run_at_first_sight_above_the_easiest_skill) {
  for (const char* level : {"0", "1"}) {
    std::vector<Pairs> guards;
    for (int i = 0; i < 6; i++) {
      const std::string at = "100 " + std::to_string(-200 + 80 * i) + " 24";
      guards.push_back(guard(at.c_str(), "180"));
      guards.back().emplace_back("spawnflags", "1");  // each sees him for itself
    }
    auto h = open_hall(guards, "0", (std::string("skill ") + level).c_str());
    int ran = 0;
    for (int t = 0; t < 2 * kSecond; t++) {
      run(*h, 1);
      for (int i = 0; i < 6; i++) {
        const sv::SqMonsterPose& p = h->m(i).pose;
        if (h->m(i).run_shoot >= 0 || (p.frame >= 109 && p.frame <= 122)) ran |= 1 << i;
      }
    }
    if (level[0] == '0')
      CHECK_MSG(ran == 0, std::to_string(ran));
    else
      CHECK_MSG(ran != 0, "none ran and shot");  // half the time each: one in 64 for none
  }
}

TEST(a_soldier_shot_within_four_of_his_eye_plays_the_head_shot_death) {
  auto h = open_hall({guard("0 0 24", "180"), guard("0 150 24", "180")});
  run(*h, 10);
  const sh::Vec3f head = h->e(0).origin + sh::Vec3f{0, 0, 26};
  CHECK(sv::monster_damage(h->s(), h->m(0).entity, -1, h->e(0).health, {1, 0, 0}, false, &head));
  const sh::Vec3f chest = h->e(1).origin + sh::Vec3f{0, 0, 10};
  CHECK(sv::monster_damage(h->s(), h->m(1).entity, -1, h->e(1).health, {1, 0, 0}, false, &chest));
  run(*h, 10);
  CHECK(h->m(0).dead && h->m(0).pose.span.first == 343);
  CHECK(h->m(1).dead && h->m(1).pose.span.first != 343);
}

TEST(a_prisoner_hangs_on_his_cross_lies_down_or_stands_by_his_flags_and_cries_on_cue) {
  const auto insane = [](const char* origin, const char* flags) {
    return Pairs{{"classname", "misc_insane"}, {"origin", origin}, {"spawnflags", flags}};
  };
  auto h = open_hall({insane("0 0 24", "8"), insane("0 150 24", "20"), insane("0 -150 24", "32")});
  bool cross = true, down = true, upright = true;
  std::vector<std::string> heard;
  for (int t = 0; t < 10 * kSecond / 8; t++) {
    for (const std::string& n : sounds_sent(*h, 8)) heard.push_back(n);
    const int c = h->m(0).pose.frame, d = h->m(1).pose.frame, u = h->m(2).pose.frame;
    cross = cross && c >= 252 && c <= 281;
    down = down && d >= 99 && d <= 159;
    upright = upright && u >= 59 && u <= 93;
  }
  CHECK(cross);
  CHECK(down);
  CHECK(upright);
  CHECK(has(heard, "insane/insane7.wav"));   // the moan on the cross
  CHECK(has(heard, "insane/insane11.wav"));  // the fist lying down
  // Dead, the hung one hangs on where he was; the one lying down dies low.
  const int hung = h->m(0).pose.frame;
  for (int i = 0; i < 2; i++) CHECK(sv::map_damage(h->s(), h->m(i).entity, -1, h->e(i).health));
  run(*h, kSecond);
  CHECK(h->m(0).pose.frame == hung || h->m(0).pose.frame == hung + 1);
  CHECK(h->m(1).pose.span.first == 245);
}

namespace {

// The hall with every face's light one luxel of `level` (0..255).
std::unique_ptr<Hall> open_lit_hall(const std::vector<Pairs>& monsters, uint8_t level) {
  auto map = std::make_shared<br::Map>(*hall(monsters));
  map->lightmaps.push_back({1, 1, 0});
  map->light_rgb = {level, level, level};
  // The floor drawn: two triangles over the hall's top face, lit by the one luxel.
  const uint32_t at = static_cast<uint32_t>(map->vertices.size());
  for (float x : {-1024.0f, 1024.0f})
    for (float y : {-256.0f, 256.0f}) {
      br::Vertex v;
      v.pos[0] = x;
      v.pos[1] = y;
      map->vertices.push_back(v);
    }
  for (uint32_t i : {0u, 2u, 1u, 1u, 2u, 3u}) map->indices.push_back(at + i);
  if (map->textures.empty()) map->textures.emplace_back();
  br::Surface floor;
  floor.first = static_cast<uint32_t>(map->indices.size()) - 6;
  floor.count = 6;
  floor.lightmap = 0;
  map->models[0].first_surface = static_cast<uint32_t>(map->surfaces.size());
  map->models[0].surface_count = 1;
  map->surfaces.push_back(floor);
  auto h = std::make_unique<Hall>();
  h->r = sv::q2_runner(sv::make_q2_server({}));
  h->r.console("deathmatch 0");
  sv::MapSpec spec;
  spec.name = "hall";
  spec.brushes = map;
  std::string error;
  CHECK_MSG(h->r.start(spec, error), error);
  h->r.connect(0, "Ana", false, false);
  return h;
}

}  // namespace

TEST(a_player_in_the_dark_is_not_spotted_until_his_muzzle_flashes) {
  auto dark = open_lit_hall({guard("0 0 24", "180")}, 4);  // 150 × 4/255: 2.4
  run(*dark, 2 * kSecond);
  CHECK(dark->m().enemy == -1);
  run(*dark, 1, 1);  // a shot: his flash, and the noise
  run(*dark, kSecond);
  CHECK(dark->m().enemy == 0);
  auto lit = open_lit_hall({guard("0 0 24", "180")}, 20);  // 150 × 20/255: 11.8
  run(*lit, 2 * kSecond);
  CHECK(lit->m().enemy == 0);
}

TEST(a_gunner_flinches_by_the_blows_size_and_at_skill_3_only_cries) {
  auto h = open_hall({monster("monster_gunner", "300 200 24", "90"),
                      monster("monster_gunner", "300 -200 24", "90")});
  run(*h, 10);
  CHECK(sv::monster_damage(h->s(), h->m(0).entity, -1, 5, {1, 0, 0}));
  CHECK(sv::monster_damage(h->s(), h->m(1).entity, -1, 30, {1, 0, 0}));
  run(*h, 8);
  CHECK_MSG(h->m(0).pose.span.first == 185, std::to_string(h->m(0).pose.span.first));
  CHECK_MSG(h->m(1).pose.span.first == 159, std::to_string(h->m(1).pose.span.first));
  auto hard = open_hall({monster("monster_gunner", "300 200 24", "90")}, "0", "skill 3");
  run(*hard, 10);
  hard->s().out.events.clear();
  CHECK(sv::monster_damage(hard->s(), hard->m().entity, -1, 30, {1, 0, 0}));
  bool cried = false;
  for (const sv::NetEvent& ev : hard->s().out.events)
    cried = cried || ev.text.rfind("gunner/gunpain", 0) == 0;
  run(*hard, 8);
  CHECK(cried);
  CHECK(hard->m().pose.act != sv::SqMonsterPose::Act::pain);
}

TEST(a_tank_shrugs_off_a_small_blow_and_a_soldier_knocked_up_plays_its_air_pain) {
  auto h = open_hall({monster("monster_tank", "300 200 40", "90"), guard("300 -200 24", "90")});
  run(*h, 10);
  h->s().out.events.clear();
  CHECK(sv::monster_damage(h->s(), h->m(0).entity, -1, 8, {1, 0, 0}));
  for (const sv::NetEvent& ev : h->s().out.events) CHECK(ev.text != "tank/tnkpain2.wav");
  h->m(1).velocity.z = 200;
  CHECK(sv::monster_damage(h->s(), h->m(1).entity, -1, 2, {}));
  run(*h, 8);
  CHECK_MSG(h->m(1).pose.span.first == 80, std::to_string(h->m(1).pose.span.first));
}

TEST(a_berserker_falls_by_the_killing_blow_and_a_chick_cries_by_its_death) {
  auto h = open_hall({monster("monster_berserk", "300 200 24", "90"),
                      monster("monster_berserk", "300 -200 24", "90"),
                      monster("monster_chick", "300 0 24", "90")});
  run(*h, 10);
  CHECK(sv::monster_damage(h->s(), h->m(0).entity, -1, h->e(0).health + 60, {}));
  h->e(1).health = 10;
  CHECK(sv::monster_damage(h->s(), h->m(1).entity, -1, 10, {}));
  h->s().out.events.clear();
  CHECK(sv::monster_damage(h->s(), h->m(2).entity, -1, h->e(2).health, {}));
  std::string cry;
  for (const sv::NetEvent& ev : h->s().out.events)
    if (ev.text.rfind("chick/chkdeth", 0) == 0) cry = ev.text;
  run(*h, 8);
  CHECK(h->m(0).pose.span.first == 223);
  CHECK(h->m(1).pose.span.first == 236);
  const int run_first = h->m(2).pose.span.first;
  CHECK_MSG((run_first == 48 && cry == "chick/chkdeth1.wav") ||
                (run_first == 60 && cry == "chick/chkdeth2.wav"),
            cry + " " + std::to_string(run_first));
}

TEST(flies_come_to_some_infantry_corpses_and_buzz) {
  std::vector<Pairs> foot;
  foot.reserve(4);
  for (int i = 0; i < 4; i++)
    foot.push_back(monster("monster_infantry",
                           ("300 " + std::to_string(-200 + 130 * i) + " 24").c_str(), "90"));
  auto h = open_hall(foot);
  run(*h, 10);
  for (int i = 0; i < 4; i++) CHECK(sv::map_damage(h->s(), h->m(i).entity, -1, h->e(i).health));
  run(*h, 18 * kSecond);
  int buzzing = 0;
  for (int i = 0; i < 4; i++) {
    const sv::WorldEntity& r = h->r->world.entities[size_t(sv::map_row(h->m(i).entity))];
    if ((r.effects & sv::kSqFliesEffect) != 0) {
      buzzing++;
      CHECK(h->s().sounds[size_t(r.loop)] == "infantry/inflies1.wav");
    }
  }
  CHECK_MSG(buzzing >= 1, "none of four corpses drew flies");
}

TEST(a_gunner_standing_fidgets_now_and_then) {
  std::vector<Pairs> guns;
  guns.reserve(3);
  for (int i = 0; i < 3; i++)
    guns.push_back(
        monster("monster_gunner", ("400 " + std::to_string(-200 + 200 * i) + " 24").c_str(), "90"));
  auto h = open_hall(guns);
  bool fidgeted = false;
  for (int t = 0; t < 60 * kSecond / 8 && !fidgeted; t++) {
    run(*h, 8);
    for (int i = 0; i < 3; i++)
      fidgeted = fidgeted || (h->m(i).pose.frame >= 30 && h->m(i).pose.frame <= 69);
  }
  CHECK(fidgeted);
}

TEST(a_technician_stands_either_of_its_two_ways) {
  std::vector<Pairs> techs;
  techs.reserve(8);
  for (int i = 0; i < 8; i++)
    techs.push_back(monster(
        "monster_floater",
        (std::to_string(300 + 100 * (i / 4)) + " " + std::to_string(-210 + 140 * (i % 4)) + " 64")
            .c_str(),
        "90"));
  auto h = open_hall(techs);
  int first = 0, second = 0;
  for (int i = 0; i < 8; i++) {
    const int f = h->m(i).pose.frame;
    first += f >= 144 && f <= 195 ? 1 : 0;
    second += f >= 196 && f <= 247 ? 1 : 0;
  }
  CHECK_MSG(first > 0 && second > 0 && first + second == 8,
            std::to_string(first) + " " + std::to_string(second));
}

TEST(the_makron_falling_whole_leaves_his_torso_writhing) {
  auto h = open_hall({monster("monster_makron", "300 0 24", "180")});
  run(*h, 10);
  int torso = -1;
  for (size_t i = 0; i < h->s().entities.size(); i++)
    if (h->s().entities[i].classname == "makron_torso") torso = static_cast<int>(i);
  CHECK(torso >= 0 && h->s().entities[size_t(torso)].hidden);
  CHECK(sv::map_damage(h->s(), h->m().entity, -1, h->e().health));
  run(*h, kSecond);
  const sv::Q2Entity& t = h->s().entities[size_t(torso)];
  CHECK(!t.hidden);
  CHECK_MSG(t.origin.y == h->e().origin.y - 84, std::to_string(t.origin.y));
  const sv::WorldEntity& r = h->r->world.entities[size_t(sv::map_row(torso))];
  CHECK(!r.free);
  CHECK_MSG(r.frame >= 346 && r.frame <= 364, std::to_string(r.frame));
  CHECK(h->s().sounds[size_t(r.loop)] == "makron/spine.wav");
}

TEST(a_tank_stamps_on_a_fallen_player_then_forgets_him) {
  auto h = open_hall({monster("monster_tank", "-450 0 40", "180")});
  run(*h, 2 * kSecond);
  CHECK(h->m().enemy == 0);
  h->m().busy_until = 0;
  sv::fight_hurt(h->s(), 0, -1, {}, 110, 0, 0);
  CHECK(!h->p().alive && h->p().health > -80);
  bool stamped = false;
  for (int t = 0; t < 5 * kSecond && !stamped; t++) {
    run(*h, 8);
    stamped = h->m().pose.frame >= 77 && h->m().pose.frame <= 114;
  }
  CHECK(stamped);
  run(*h, 5 * kSecond);
  CHECK(h->m().enemy == -1);
}

TEST(a_maps_count_on_ammo_gives_that_many) {
  auto h = open_hall({{{"classname", "ammo_shells"}, {"origin", "-590 0 24"}, {"count", "3"}}});
  const int before = h->p().body.ammo[int(sv::SqAmmo::shells)];
  run(*h, kSecond);
  CHECK_MSG(h->p().body.ammo[int(sv::SqAmmo::shells)] == before + 3,
            std::to_string(h->p().body.ammo[int(sv::SqAmmo::shells)]));
}

TEST(the_chaingun_spins_up_and_down_and_the_hornet_is_proof_against_lasers) {
  auto h = open_hall({monster("monster_boss2", "600 0 200", "180")});
  CHECK(sv::monster_laser_proof(h->s(), h->m().entity));
  arm(*h, sv::SqWeapon::chaingun, 100);
  std::vector<std::string> heard;
  for (int i = 0; i < 2 * kSecond; i++) {  // held past the spin (let go by frame 14: no wind-down)
    h->s().out.events.clear();
    run(*h, 1, sh::kButtonAttack, 90);
    for (const sv::NetEvent& ev : h->s().out.events)
      if (ev.kind == sv::EventKind::sound) heard.push_back(ev.text);
  }
  for (const std::string& n : sounds_sent(*h, 2 * kSecond)) heard.push_back(n);
  CHECK(has(heard, "weapons/chngnu1a.wav"));
  CHECK(has(heard, "weapons/chngnd1a.wav"));
}

TEST(a_flyer_is_knocked_back_and_wears_the_push_off) {
  Pairs flyer = monster("monster_flyer", "300 0 120", "0");  // facing away, waiting unseen
  flyer.emplace_back("spawnflags", "1");
  auto h = open_hall({flyer});
  run(*h, 10);
  const sh::Vec3f was = h->e().origin;
  CHECK(sv::monster_damage(h->s(), h->m().entity, -1, 40, {1, 0, 0}));  // 400 u/s
  CHECK(h->m().velocity.x > 0);
  run(*h, 2 * kSecond);
  CHECK(h->e().origin.x > was.x + 1);
}
