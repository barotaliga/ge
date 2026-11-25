// Tests: Ground Zero's monsters (games/shooters_edge/server/q2_gz_monsters.cpp and their roster
// rows): the daedalus's green bolts and screen, the medic commander's call for reinforcements and
// its slots, the turret on a floor and in a wall, the hidden brain that fires a breach, and the
// stalker on its ceiling and in its false death.
//
// ge:summary Tests: Ground Zero's daedalus, medic commander, turret, turret brain and stalker
// ge:keywords test q2 ground-zero rogue monsters daedalus medic-commander reinforcements
// ge:keywords monster-turret turret-invisible-brain green-bolt stalker ceiling feign-death
// ge:skill shooter testing
// ge:test tests/shooters_edge_q2_gz_monsters_test.cpp
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

}  // namespace

TEST(a_daedalus_is_a_stronger_hover_firing_green_bolts_behind_a_screen) {
  auto h = open_hall({thing("monster_daedalus", "-300 0 96", "180")});
  run(*h, 2);
  const int d = find(*h, "monster_daedalus");
  CHECK(d >= 0);
  CHECK(h->e(d).health == 450 && h->m(d).screen == 100);
  CHECK(sv::monster_damage(h->s(), h->m(d).entity, -1, 30, {1, 0, 0}));  // into its face
  CHECK_MSG(h->e(d).health == 430, std::to_string(h->e(d).health));
  int green = 0;
  for (int i = 0; i < 6 * kSecond && green == 0; i++) {
    run(*h, 1);
    green = missiles(*h, sv::SqShot::green_bolt);
  }
  CHECK_MSG(green > 0, "the daedalus never fired a green bolt");
  CHECK(missiles(*h, sv::SqShot::bolt) == 0);
}

TEST(a_medic_commander_calls_guards_that_take_its_slots_and_are_not_counted) {
  // It faces away from Ana, 700 off: she is its enemy but out of its sight, so it calls.
  auto h = open_hall({thing("monster_medic_commander", "100 0 24", "0")});
  run(*h, 2);
  const int c = find(*h, "monster_medic_commander");
  CHECK(c >= 0);
  CHECK(h->e(c).health == 600 && h->s().monsters_total == 1);
  h->m(c).enemy = 0;
  int out = 0;
  for (int i = 0; i < 4 * kSecond && out == 0; i++) {
    run(*h, 1);
    out = 0;
    for (const sv::Q2Monster& m : h->s().monsters)
      out += m.commander == c && !m.spare && !m.dead ? 1 : 0;
  }
  CHECK_MSG(out == 1 || out == 3 || out == 5, std::to_string(out));
  CHECK(h->m(c).called == out);
  CHECK(h->s().monsters_total == 1);  // its guards are not the level's
  // One of them killed: its slot back, no kill counted.
  for (sv::Q2Monster& m : h->s().monsters) {
    if (m.commander != c || m.spare || m.dead) continue;
    CHECK(sv::monster_damage(h->s(), m.entity, -1, 100000, {}));
    break;
  }
  CHECK(h->m(c).called == out - 1);
  CHECK(h->s().monsters_killed == 0);
}

TEST(a_turret_on_the_floor_opens_aims_and_fires_its_rockets) {
  Pairs t = thing("monster_turret", "-900 0 12", "-1");  // behind Ana, facing her way
  t.push_back({"spawnflags", "32"});
  auto h = open_hall({t});
  run(*h, 2);
  const int u = find(*h, "monster_turret");
  CHECK(u >= 0);
  CHECK(h->e(u).health == 240 && h->e(u).angles.x == -90);
  int rockets = 0;
  for (int i = 0; i < 20 * kSecond && rockets == 0; i++) {
    run(*h, 1);
    rockets = missiles(*h, sv::SqShot::rocket);
  }
  CHECK_MSG(rockets > 0, "the turret never fired");
  CHECK(h->e(u).angles.x <= -5);  // held above the level
  // No pain: it fights on; at its death it blows up and is gone.
  CHECK(sv::monster_damage(h->s(), h->m(u).entity, -1, 1000, {}));
  CHECK(h->m(u).dead && h->e(u).gone);
}

TEST(a_wall_turret_is_untouchable_until_used_then_slides_out) {
  Pairs t = thing("monster_turret", "496 0 64", "180");
  t.push_back({"spawnflags", "136"});  // a wall unit with a blaster
  t.push_back({"targetname", "guns"});
  auto h = open_hall({t});
  run(*h, 2);
  const int u = find(*h, "monster_turret");
  CHECK(u >= 0);
  const float x0 = h->e(u).origin.x;
  CHECK(sv::monster_damage(h->s(), h->m(u).entity, -1, 50, {}));
  CHECK(h->e(u).health == 240);
  CHECK(sv::monster_use(h->s(), h->m(u).entity, 0));
  run(*h, 3 * kSecond);
  CHECK_MSG(h->e(u).origin.x <= x0 - 31.9f, std::to_string(h->e(u).origin.x));
  CHECK(sv::monster_damage(h->s(), h->m(u).entity, -1, 50, {}));
  CHECK(h->e(u).health == 190);
}

TEST(a_hidden_brain_fires_its_breach_once_it_has_seen_the_player_for_its_delay) {
  auto h = open_hall({{{"classname", "turret_breach"}, {"targetname", "gun"}, {"origin", "0 0 64"}},
                      {{"classname", "turret_invisible_brain"},
                       {"target", "gun"},
                       {"origin", "0 0 96"},
                       {"delay", "1"}}});
  run(*h, 2);
  int brain = -1;
  for (size_t i = 0; i < h->s().entities.size(); i++)
    if (h->s().entities[i].classname == "turret_invisible_brain") brain = static_cast<int>(i);
  CHECK(brain >= 0);
  run(*h, kSecond / 2);
  CHECK(missiles(*h, sv::SqShot::rocket) == 0);  // seen, but not yet for a second
  int rockets = 0;
  for (int i = 0; i < 2 * kSecond && rockets == 0; i++) {
    run(*h, 1);
    rockets = missiles(*h, sv::SqShot::rocket);
  }
  CHECK_MSG(rockets > 0, "the brain never fired its breach");
}

TEST(a_stalker_placed_on_the_roof_hangs_from_its_ceiling) {
  Pairs t = thing("monster_stalker", "200 0 200", "180");
  t.push_back({"spawnflags", "8"});
  auto h = open_hall({t});
  run(*h, kSecond);
  const int k = find(*h, "monster_stalker");
  CHECK(k >= 0);
  CHECK(h->m(k).gravity < 0);
  CHECK_MSG(h->e(k).origin.z > 237.9f && h->e(k).origin.z < 238.1f,
            std::to_string(h->e(k).origin.z));  // its top against the roof at 256
  // Killed, it falls to the floor.
  CHECK(sv::monster_damage(h->s(), h->m(k).entity, -1, 260, {}));
  run(*h, 2 * kSecond);
  CHECK_MSG(h->e(k).origin.z < 40, std::to_string(h->e(k).origin.z));
}

TEST(a_hurt_stalker_feigns_death_twitches_back_to_full_health_and_rises) {
  auto h = open_hall({thing("monster_stalker", "300 0 24", "0")}, "2");
  run(*h, 2);
  const int k = find(*h, "monster_stalker");
  CHECK(k >= 0);
  h->e(k).health = 40;  // below a quarter of 250
  for (int i = 0; i < 50 && h->m(k).stage != 2; i++)
    CHECK(sv::monster_damage(h->s(), h->m(k).entity, -1, 1, {}));
  CHECK_MSG(h->m(k).stage == 2, "it never feigned death");
  CHECK(!h->m(k).dead);
  run(*h, 4 * kSecond);  // two points a frame at skill 2
  CHECK_MSG(h->e(k).health > 60, std::to_string(h->e(k).health));
  run(*h, 10 * kSecond);
  CHECK(h->e(k).health == 250 && h->m(k).stage == 0);
}
