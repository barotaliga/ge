// Tests: The Reckoning's monsters (games/shooters_edge/server/q2_reckoning_monsters.cpp and the
// pack's rows in q2_monster_kinds.hpp) in a hall built here, under the pack's switch: the new
// kinds only under the pack; the three guards' guns (ion ripper discs, blue bolts, a laser that
// burns through); the gekk's green blood, its chant and its heal; the power shields of the beta
// gladiator, the beta super tank and the super tank with spawn flag 8; the repair bot roaming,
// welding a repair object and raising the dead with its beam; the brain's eyes; the pack's words.
//
// ge:summary Tests: The Reckoning's monsters: guards, gekk, repair bot, shields, brain, words
// ge:keywords test q2 reckoning mission-pack xatrix gekk fixbot repair-bot gladb boss5 chick-heat
// ge:keywords soldier-ripper soldier-hypergun soldier-lasergun power-shield laser-beam obituary
// ge:skill shooter testing
// ge:test tests/shooters_edge_q2_reckoning_monsters_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
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
namespace ro = shooters_edge::server::roster;
using sv::Q2Pack;

using Pairs = std::vector<std::pair<std::string, std::string>>;

br::Entity entity(Pairs pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A long floored hall, the player's start at its west end, and `things` placed in it.
std::shared_ptr<const br::Map> hall(const std::vector<Pairs>& things) {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-1024, -256, -16}, hi[3] = {1024, 256, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-600 0 24"}, {"angle", "0"}}));
  for (const Pairs& m : things) w.entities.push_back(entity(m));
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

std::unique_ptr<Hall> open_hall(const std::vector<Pairs>& things, Q2Pack pack = Q2Pack::reckoning,
                                const char* skill = "1") {
  auto h = std::make_unique<Hall>();
  sv::Q2ServerOptions o;
  o.pack = pack;
  h->r = sv::q2_runner(sv::make_q2_server(o));
  h->r.console("deathmatch 0");
  h->r.console(std::string("skill ") + skill);
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall(things);
  std::string error;
  CHECK_MSG(h->r.start(map, error), error);
  h->r.connect(0, "Ana", false, false);
  return h;
}

// Host ticks of 13 ms, Ana standing still looking east.
void run(Hall& h, int ticks) {
  for (int i = 0; i < ticks; i++) {
    sh::Packet pk;
    pk.client = 0;
    pk.sequence = ++h.sequence;
    sh::MoveCmd c;
    c.msec = 13;
    pk.cmds[0] = pk.cmds[1] = pk.cmds[2] = c;
    h.r.frame(&pk, 1);
  }
}

Pairs monster(const char* classname, const char* origin, const char* angle,
              const char* spawnflags = "0") {
  return {
      {"classname", classname}, {"origin", origin}, {"angle", angle}, {"spawnflags", spawnflags}};
}

constexpr int kSecond = 77;  // host ticks of 13 ms in a second

// Whether a shot of `kind` is in flight.
bool flying(const Hall& h, sv::SqShot kind) {
  for (const sv::Q2Missile& m : h.s().missiles)
    if (m.live && m.kind == kind) return true;
  return false;
}

// Runs until a shot of `kind` flies (at most `seconds`).
bool fires(Hall& h, sv::SqShot kind, int seconds) {
  for (int i = 0; i < seconds * kSecond; i++) {
    run(h, 1);
    if (flying(h, kind)) return true;
  }
  return false;
}

}  // namespace

TEST(the_packs_kinds_play_only_under_it) {
  for (const char* c :
       {"monster_gekk", "monster_fixbot", "monster_gladb", "monster_chick_heat", "monster_boss5",
        "monster_soldier_ripper", "monster_soldier_hypergun", "monster_soldier_lasergun"}) {
    CHECK_MSG(ro::kind_named(c, Q2Pack::base) < 0, c);
    CHECK_MSG(ro::kind_named(c, Q2Pack::reckoning) >= 0, c);
    CHECK_MSG(ro::kind_named(c, Q2Pack::ground_zero) < 0, c);
  }
  // The base classes the pack changes: its own rows under it, the base game's elsewhere.
  const ro::Kind& tank = ro::kKinds[ro::kind_named("monster_supertank", Q2Pack::reckoning)];
  CHECK(tank.shield == 400 && tank.shield_flag == 8);
  CHECK(ro::kKinds[ro::kind_named("monster_supertank", Q2Pack::base)].shield == 0);
  CHECK(ro::kKinds[ro::kind_named("monster_brain", Q2Pack::reckoning)].missile[1].fire ==
        ro::Fire::laser);
  CHECK(ro::kKinds[ro::kind_named("monster_brain", Q2Pack::base)].missile[0].fire ==
        ro::Fire::none);
  // A beta plays its base kind's frames.
  CHECK(
      std::string(ro::looks_like(ro::kKinds[ro::kind_named("monster_gladb", Q2Pack::reckoning)])) ==
      "monster_gladiator");
  // In the base game a gekk placed on a map is no monster at all.
  auto h = open_hall({monster("monster_gekk", "0 0 24", "180")}, Q2Pack::base);
  run(*h, 2);
  CHECK(h->s().monsters.empty());
}

TEST(the_three_guards_numbers_and_guns) {
  auto h = open_hall({monster("monster_soldier_ripper", "-200 0 24", "180")});
  run(*h, 2);
  CHECK(h->e().health == 50 && h->e().max_health == 50);
  CHECK(fires(*h, sv::SqShot::ripper, 10));  // ion ripper discs
  auto g = open_hall({monster("monster_soldier_hypergun", "-200 0 24", "180")});
  run(*g, 2);
  CHECK(g->e().health == 60);
  CHECK(fires(*g, sv::SqShot::blue_bolt, 10));  // blue bolts
  CHECK(!flying(*g, sv::SqShot::bolt));
}

TEST(the_laser_guard_burns_her_with_a_beam_it_draws) {
  auto h = open_hall({monster("monster_soldier_lasergun", "-200 0 24", "180")});
  run(*h, 2);
  CHECK(h->e().health == 70);
  CHECK(h->m().beam >= 0);  // its beams' rows
  bool drawn = false;
  for (int i = 0; i < 15 * kSecond && h->p().health == 100; i++) {
    run(*h, 1);
    const sh::WorldEntity& r = h->s().world.entities[size_t(sv::map_row(h->m().beam))];
    if (!r.free && (r.render_fx & sv::kSqBeamRender) != 0) drawn = true;
  }
  CHECK_MSG(h->p().health < 100, std::to_string(h->p().health));
  CHECK(drawn);
  CHECK(!flying(*h, sv::SqShot::bolt));  // no bolts: a beam
}

TEST(the_gekk_bleeds_green_heals_and_chants) {
  auto h = open_hall({monster("monster_gekk", "300 0 24", "0", "8")});
  run(*h, 2);
  CHECK(h->e().health == 125);
  CHECK(sv::monster_green(h->s(), h->m().entity));
  CHECK(h->m().chant);
  // Chanting it looks for no one, even facing her.
  h->e().angles.y = 180;
  run(*h, 3 * kSecond);
  CHECK(h->m().enemy == -1);
  // Its first blow only wakes it: no flinch.
  CHECK(sv::monster_damage(h->s(), h->m().entity, 0, 30, {-1, 0, 0}));
  CHECK(!h->m().chant);
  CHECK(h->e().health == 65);  // a blow on one that saw no one: twice its points
  // Its idle heals it 10 to 20 (here with no enemy: a fresh, uncounted one).
  auto g = open_hall({monster("monster_gekk", "300 0 24", "0")});
  run(*g, 2);
  g->e().health = 50;
  for (int i = 0; i < 40 && g->e().health == 50; i++) run(*g, kSecond);
  CHECK_MSG(g->e().health >= 60 && g->e().health <= 70, std::to_string(g->e().health));
  // A guard bleeds red.
  auto r = open_hall({monster("monster_soldier_ripper", "300 0 24", "0")});
  run(*r, 2);
  CHECK(!sv::monster_green(r->s(), r->m().entity));
}

TEST(a_gekk_facing_her_spits_or_leaps) {
  auto h = open_hall({monster("monster_gekk", "-200 0 24", "180")});
  run(*h, 2);
  bool spat = false, leapt = false;
  for (int i = 0; i < 15 * kSecond && !(spat && leapt) && h->p().alive; i++) {
    run(*h, 1);
    spat = spat || flying(*h, sv::SqShot::spit);
    leapt = leapt || h->m().leaping;
  }
  CHECK(spat || leapt);
}

TEST(power_shields_take_two_thirds_from_every_side) {
  auto h = open_hall({monster("monster_gladb", "0 0 24", "180"),
                      monster("monster_boss5", "300 0 24", "180"),
                      monster("monster_supertank", "0 200 24", "180", "8"),
                      monster("monster_supertank", "300 -200 24", "180")});
  run(*h, 2);
  CHECK(h->e(0).health == 800);
  CHECK(h->m(0).shield == 400 && h->m(1).shield == 400 && h->m(2).shield == 400);
  CHECK(h->m(3).shield == 0);  // no spawn flag 8: none
  // 90 into its back (a blast: no surprise's double): 60 taken by the shield (30 points).
  CHECK(sv::monster_damage(h->s(), h->m(0).entity, -1, 90, {1, 0, 0}, true));
  CHECK_MSG(h->e(0).health == 770, std::to_string(h->e(0).health));
  CHECK(h->m(0).shield == 370);
  CHECK(sv::monster_damage(h->s(), h->m(1).entity, -1, 90, {-1, 0, 0}, true));
  CHECK(h->e(1).health == 1470 && h->m(1).shield == 370);
  CHECK(sv::monster_damage(h->s(), h->m(3).entity, -1, 90, {-1, 0, 0}, true));
  CHECK(h->e(3).health == 1410);
  run(*h, 1);  // the shell shown on the struck
  const sh::WorldEntity& r = h->s().world.entities[size_t(sv::map_row(h->m(0).entity))];
  CHECK((r.render_fx & sv::kSqShieldShellRender) != 0);
  // Worn out: 400 points take 800, then nothing.
  CHECK(sv::monster_damage(h->s(), h->m(1).entity, -1, 2000, {-1, 0, 0}, true));
  CHECK(h->m(1).shield == 0);
}

TEST(the_repair_bot_raises_a_dead_guard_with_its_beam) {
  auto h = open_hall({monster("monster_soldier_ripper", "100 0 24", "0"),
                      monster("monster_fixbot", "250 0 60", "0", "4")});
  run(*h, 10);
  CHECK(h->s().monsters_total == 2);
  CHECK(sv::map_damage(h->s(), h->m(0).entity, 0, 35));  // twice over: dead at -20, whole
  CHECK(h->m(0).dead && !h->e(0).gone);
  bool beamed = false;
  for (int i = 0; i < 30 * kSecond && h->m(0).dead; i++) {
    run(*h, 1);
    const sh::WorldEntity& r = h->s().world.entities[size_t(sv::map_row(h->m(1).beam))];
    beamed = beamed || (!r.free && static_cast<uint32_t>(r.skin) == 0xF3F3F1F1u);
  }
  CHECK_MSG(!h->m(0).dead, "the repair bot never raised the guard");
  CHECK(beamed);  // the blue healing beam
  CHECK(h->e(0).health == 50);
  CHECK(h->m(0).enemy == -1);  // up with no enemy
}

TEST(the_repair_bot_roams_and_welds_a_repair_object) {
  auto h = open_hall({monster("monster_fixbot", "0 0 60", "0", "4"),
                      {{"classname", "func_object_repair"}, {"origin", "200 0 40"}}});
  run(*h, 2);
  int object = -1;
  for (size_t i = 0; i < h->s().entities.size(); i++)
    if (h->s().entities[i].classname == "object_repair") object = static_cast<int>(i);
  CHECK(object >= 0);
  if (object < 0) return;
  for (int i = 0; i < 40 * kSecond && h->s().entities[size_t(object)].health >= 0; i++) run(*h, 1);
  CHECK_MSG(h->s().entities[size_t(object)].health < 0,
            std::to_string(h->s().entities[size_t(object)].health));
  CHECK(h->m().enemy == -1);  // working, it looked for no one
}

TEST(a_repair_bot_with_no_flags_stands_and_looks) {
  auto h = open_hall({monster("monster_fixbot", "-300 0 60", "180")});
  run(*h, 2);
  CHECK(h->m().job == 0);
  run(*h, 2 * kSecond);
  CHECK(h->m().enemy == 0);
  // Dead, it bursts and leaves nothing.
  CHECK(sv::map_damage(h->s(), h->m().entity, 0, 500));
  CHECK(h->m().dead && h->e().gone);
}

TEST(the_packs_brain_burns_her_with_its_eyes_from_afar) {
  auto h = open_hall({monster("monster_brain", "200 0 24", "180")});
  run(*h, 2);
  CHECK(h->m().beam >= 0);
  for (int i = 0; i < 40 * kSecond && h->p().health == 100 && h->p().alive; i++) run(*h, 1);
  CHECK_MSG(h->p().health < 100, std::to_string(h->p().health));
  // The base game's brain has no ranged attack and no beams.
  auto b = open_hall({monster("monster_brain", "200 0 24", "180")}, Q2Pack::base);
  run(*b, 2);
  CHECK(b->m().beam == -1);
}

TEST(the_packs_words_for_a_death_by_a_monster) {
  CHECK(sv::sq_obituary("Ana", "", false, sv::kSqByGotta, 0, "female", nullptr) ==
        "Ana that's gotta hurt.");
  CHECK(sv::sq_obituary("Ana", "", false, sv::kSqByLaser, 0, "female", nullptr) ==
        "Ana saw the light.");
  CHECK(sv::sq_obituary("Ana", "", false, sv::kSqByMonster + 3, 0, "female", nullptr) ==
        "Ana died.");
}
