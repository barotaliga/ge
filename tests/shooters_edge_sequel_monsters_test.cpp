// Tests: the sequel's monsters (games/shooters_edge/server/sequel_monsters.cpp) in a hall built
// here: removed in deathmatch; a guard facing a player sees him, turns on him and shoots him; one
// facing away does not, until the player's weapon is heard; shot dead it leaves a corpse shots
// pass, counts as killed, fires its targets and drops its item; a big blow throws it apart; one
// with a path walks it; one waiting hidden appears on a use and turns on the user. The full game's:
// a medic raises a corpse, a brain's screen, a technician's and a super tank's ends, Jorg's death
// bringing out the Makron, a gladiator's rail, the commander's body.
//
// ge:summary Tests: the sequel's monsters: sight, noise, attack, death, corpse, gibs, path, waiting
// ge:keywords test sequel monsters monster-ai sight noise corpse gib path-corner triggered-spawn
// ge:keywords medic power-screen boss makron
// ge:skill shooter testing
// ge:test tests/shooters_edge_sequel_monsters_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/server/sequel_combat.hpp"
#include "shooters_edge/server/sequel_items.hpp"
#include "shooters_edge/server/sequel_map.hpp"
#include "shooters_edge/server/sequel_monsters.hpp"
#include "shooters_edge/server/sequel_server.hpp"

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
  sv::SequelServer& s() const { return *sv::sequel_of(r); }
  sv::SequelPlayer& p() const { return s().players[0]; }
  sv::SequelMonster& m(int i = 0) const { return s().monsters[size_t(i)]; }
  sv::SequelEntity& e(int i = 0) const { return s().entities[size_t(m(i).entity)]; }
};

std::unique_ptr<Hall> open_hall(const std::vector<Pairs>& monsters, const char* dm = "0") {
  auto h = std::make_unique<Hall>();
  h->r = sv::sequel_runner(sv::make_sequel_server({}));
  h->r.console(std::string("deathmatch ") + dm);
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
  run(*h, kSecond / 2, sh::kButtonAttack);  // the blaster: heard
  CHECK(h->m().enemy == 0);
}

TEST(a_guard_shot_dead_leaves_a_corpse_counts_and_drops_its_item) {
  Pairs g = guard("0 0 24", "0");
  g.push_back({"item", "ammo_shells"});
  auto h = open_hall({g});
  run(*h, 10);
  // Ana's blaster at it down the hall until it falls.
  for (int i = 0; i < 20 && !h->m().dead; i++) run(*h, kSecond / 2, sh::kButtonAttack);
  CHECK(h->m().dead && !h->e().box && !h->e().gone);
  CHECK(h->s().monsters_killed == 1);
  bool dropped = false;
  for (const sv::SequelDrop& d : h->s().drops)
    if (d.live && d.item == sv::sq_item("ammo_shells")) dropped = true;
  CHECK(dropped);
  // A line passes the corpse.
  const sh::SequelTrace t =
      sv::map_trace(h->s(), {-100, 0, 4}, {}, {}, {100, 0, 4}, sh::kSqSolid | sh::kSqMonster);
  CHECK(t.fraction == 1);
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

// ---- The full game's (sequel-monsters-full.md) ----

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
  CHECK(sv::map_damage(h->s(), h->m(0).entity, 0, 25));  // dead, not thrown apart
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
  CHECK(sv::map_damage(m->s(), m->m(0).entity, 0, 25));
  CHECK(has(lines_sent(*m, 6), "te_medic_cable_attack"));
}
