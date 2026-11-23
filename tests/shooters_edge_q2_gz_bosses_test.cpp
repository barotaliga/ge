// Tests: Ground Zero's bosses (games/shooters_edge/server/q2_gz_bosses.cpp and their roster
// rows): health and power shields by skill, the carrier's attacks and the fliers it launches
// within its slots, the widow's stalkers, her answer to a quad, her beam-out and her legs, the
// second stage killing every stalker at her death and settling into a body that can be gibbed,
// lasers passing the bosses by.
//
// ge:summary Tests: Ground Zero's carrier and black widow (both stages), by hand and by skill
// ge:keywords test q2 ground-zero rogue boss carrier black-widow widow widow2 stalker legs
// ge:keywords spawn-slots power-up-answer beam-out
// ge:skill shooter testing
// ge:test tests/shooters_edge_q2_gz_bosses_test.cpp
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/server/q2_combat.hpp"
#include "shooters_edge/server/q2_map.hpp"
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

// A long hall floored at 0 and roofed at 256 with a wall at x 512, the player's start at its west
// end, and `things`.
std::shared_ptr<const br::Map> hall(const std::vector<Pairs>& things) {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-1024, -256, -16}, hi[3] = {1024, 256, 0};
  const float wall_lo[3] = {512, -256, 0}, wall_hi[3] = {528, 256, 256};
  const float roof_lo[3] = {-1024, -256, 256}, roof_hi[3] = {1024, 256, 272};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  w.models[0].push_back(br::box_brush(roof_lo, roof_hi, sh::kSqSolid, "world/roof"));
  w.models[0].push_back(br::box_brush(wall_lo, wall_hi, sh::kSqSolid, "world/wall"));
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
  sv::Q2Monster& m(int i = 0) const { return s().monsters[size_t(i)]; }
  sv::Q2Entity& e(int i = 0) const { return s().entities[size_t(m(i).entity)]; }
};

std::unique_ptr<Hall> open_hall(const std::vector<Pairs>& things, const char* skill = "1") {
  auto h = std::make_unique<Hall>();
  sv::Q2ServerOptions o;
  o.pack = sv::Q2Pack::ground_zero;
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

// Host ticks of 13 ms, Ana standing.
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

constexpr int kSecond = 77;  // host ticks of 13 ms in a second

Pairs thing(const char* classname, const char* origin, const char* angle) {
  return {{"classname", classname}, {"origin", origin}, {"angle", angle}};
}

int find(const Hall& h, const char* classname) {
  for (size_t i = 0; i < h.s().monsters.size(); i++)
    if (h.s().entities[size_t(h.s().monsters[i].entity)].classname == classname &&
        !h.s().monsters[i].spare)
      return static_cast<int>(i);
  return -1;
}

int missiles(const Hall& h, sv::SqShot kind, int from = -1) {
  int n = 0;
  for (const sv::Q2Missile& m : h.s().missiles)
    n += m.live && m.kind == kind && (from < 0 || m.owner == from) ? 1 : 0;
  return n;
}

// Runs `seconds`, Ana kept alive and in place; returns the most of `count` seen at once.
template <class F>
int watch(Hall& h, int seconds, F count) {
  int most = 0;
  for (int i = 0; i < seconds * kSecond; i++) {
    h.s().players[0].health = 999;
    run(h, 1);
    most = std::max(most, count());
  }
  return most;
}

int brood_out(const Hall& h, int boss) {
  int n = 0;
  for (const sv::Q2Monster& m : h.s().monsters)
    n += m.commander == boss && !m.spare && !m.dead ? 1 : 0;
  return n;
}

}  // namespace

TEST(the_bosses_health_and_shields_go_by_skill) {
  {
    auto h = open_hall(
        {thing("monster_carrier", "0 0 128", "180"), thing("monster_widow", "300 -120 24", "180"),
         thing("monster_widow2", "300 120 24", "180")},
        "1");
    run(*h, 2);
    CHECK(h->e(find(*h, "monster_carrier")).health == 2000);
    CHECK(h->e(find(*h, "monster_widow")).health == 3000);
    CHECK(h->e(find(*h, "monster_widow2")).health == 3800);
    CHECK(h->m(find(*h, "monster_widow")).shield == 0);
    CHECK(h->s().monsters_total == 3);  // their spares are not the level's
  }
  auto h = open_hall(
      {thing("monster_carrier", "0 0 128", "180"), thing("monster_widow", "300 -120 24", "180"),
       thing("monster_widow2", "300 120 24", "180")},
      "3");
  run(*h, 2);
  CHECK(h->e(find(*h, "monster_carrier")).health == 4000);
  CHECK(h->e(find(*h, "monster_widow")).health == 5000);
  CHECK(h->m(find(*h, "monster_widow")).shield == 500);
  CHECK(h->m(find(*h, "monster_widow2")).shield == 750);
}

TEST(lasers_pass_the_bosses_by) {
  auto h = open_hall({thing("monster_carrier", "0 0 128", "180")});
  run(*h, 2);
  CHECK(sv::monster_laser_proof(h->s(), h->m(find(*h, "monster_carrier")).entity));
}

TEST(the_carrier_fights_and_launches_fliers_within_its_slots) {
  auto h = open_hall({thing("monster_carrier", "-350 0 128", "180")}, "1");
  run(*h, 2);
  const int c = find(*h, "monster_carrier");
  CHECK(c >= 0);
  h->m(c).enemy = 0;
  int shots = 0;
  const int most = watch(*h, 30, [&] {
    shots = std::max(shots, missiles(*h, sv::SqShot::grenade) + missiles(*h, sv::SqShot::rocket));
    return brood_out(*h, c);
  });
  CHECK_MSG(most >= 1 && most <= 6, std::to_string(most));  // 6 slots at skill 1
  CHECK(h->m(c).called <= 6);
  CHECK(shots > 0);  // grenades or rockets: Ana is within 600
  CHECK(h->s().monsters_total == 1);
}

TEST(the_widow_brings_stalkers_within_her_slots) {
  auto h = open_hall({thing("monster_widow", "200 0 24", "180")}, "1");
  run(*h, 2);
  const int w = find(*h, "monster_widow");
  h->m(w).enemy = 0;
  const int most = watch(*h, 30, [&] { return brood_out(*h, w); });
  CHECK_MSG(most >= 1 && most <= 3, std::to_string(most));  // 3 slots at skill 1
  // One stalker killed: her slot back, no kill counted.
  for (sv::Q2Monster& m : h->s().monsters) {
    if (m.commander != w || m.spare || m.dead) continue;
    const int before = h->m(w).called;
    CHECK(sv::monster_damage(h->s(), m.entity, -1, 100000, {}));
    CHECK(h->m(w).called == before - 1);
    break;
  }
  CHECK(h->s().monsters_killed == 0);
}

TEST(the_widow_answers_a_quad_with_her_own_multiplier) {
  auto h = open_hall({thing("monster_widow", "200 0 24", "180")}, "2");
  run(*h, 2);
  const int w = find(*h, "monster_widow");
  h->m(w).enemy = 0;
  h->s().players[0].quad_until = 1e9;
  watch(*h, 3, [] { return 0; });
  CHECK_MSG(h->m(w).boss.multiplier == 4, std::to_string(h->m(w).boss.multiplier));
  h->s().players[0].quad_until = 0;
  watch(*h, 3, [] { return 0; });
  CHECK(h->m(w).boss.multiplier == 1);
}

TEST(the_widow_beams_out_leaves_her_legs_and_they_burn_down) {
  auto h = open_hall({thing("monster_widow", "200 0 24", "180")}, "1");
  run(*h, 2);
  const int w = find(*h, "monster_widow");
  CHECK(sv::monster_damage(h->s(), h->m(w).entity, -1, 100000, {}));
  CHECK(h->m(w).dead && h->s().monsters_killed == 1);
  CHECK(sv::monster_damage(h->s(), h->m(w).entity, -1, 100, {}));  // taken, but untouched
  CHECK(!h->e(w).gone);
  watch(*h, 4, [] { return 0; });  // 3.1 s of dying: her legs now
  CHECK(h->m(w).boss.legs >= 0 && !h->e(w).box && !h->e(w).gone);
  watch(*h, 4, [] { return 0; });
  CHECK(h->e(w).gone);
}

TEST(the_second_stage_kills_every_stalker_and_settles_into_a_body) {
  auto h = open_hall(
      {thing("monster_widow2", "200 0 24", "180"), thing("monster_stalker", "-200 150 24", "0")},
      "1");
  run(*h, 2);
  const int w = find(*h, "monster_widow2"), k = find(*h, "monster_stalker");
  CHECK(w >= 0 && k >= 0 && !h->m(k).dead);
  CHECK(sv::monster_damage(h->s(), h->m(w).entity, -1, h->e(w).health + 1, {}));
  CHECK(h->m(w).dead && h->m(k).dead);
  CHECK(h->s().monsters_killed == 2);
  const int health = h->e(w).health;
  CHECK(sv::monster_damage(h->s(), h->m(w).entity, -1, 5000, {}));  // dying: untouched
  CHECK(!h->e(w).gone && h->e(w).health == health);
  watch(*h, 14, [] { return 0; });  // the blasts, the chain and the dead search: settled
  CHECK_MSG(h->e(w).maxs.z == 80, std::to_string(h->e(w).maxs.z));
  CHECK(sv::monster_damage(h->s(), h->m(w).entity, -1, 1000, {}));
  CHECK(h->e(w).gone);  // thrown apart below -900
}
