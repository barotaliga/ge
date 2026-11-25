// Tests: Ground Zero's trains and fans (games/shooters_edge/server/q2_gz_trains.cpp): a corner's
// own speed taken by the train leaving it, a train's team riding with it, a fan that ramps up and
// down and fires its targets at full speed and at rest; the base game's trains and fans unchanged
// outside the pack.
//
// ge:summary Tests: Ground Zero's trains and fans: corner speeds, a train's riders, a fan's ramp
// ge:keywords test q2 ground-zero func-train path-corner corner-speed train-team func-rotating
// ge:keywords accel fan-ramp
// ge:skill shooter testing
// ge:test tests/shooters_edge_q2_gz_trains_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/server/q2_map.hpp"
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

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
}

// A floored hall far from its walls: a train (*1) on two corners 512 apart along x, the first
// carrying `corner` keys; a wall (*2) sharing the train's team; a fan (*3) with ACCEL that
// targets a relay; `pack` the Ground Zero switch.
struct Hall {
  sv::RulesRunner r;
  uint32_t sequence = 0;
  sv::Q2Server& s() const { return *sv::q2_of(r); }
  sv::Q2Entity& named(const char* classname) const {
    for (sv::Q2Entity& e : s().entities)
      if (e.classname == classname) return e;
    return s().entities[0];
  }
};

std::unique_ptr<Hall> open_hall(bool pack, const Pairs& corner) {
  br::BrushWorld w;
  w.models.resize(4);
  w.models[0].push_back(box(-2048, -1024, -16, 2048, 1024, 0));
  w.models[1].push_back(box(-32, -32, 200, 32, 32, 216));  // the train, high above the floor
  w.models[2].push_back(box(-32, 64, 200, 32, 128, 216));  // its rider, beside it
  w.models[3].push_back(box(-32, -600, 0, 32, -500, 64));  // the fan
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-1500 0 24"}, {"angle", "0"}}));
  w.entities.push_back(entity({{"classname", "func_train"},
                               {"model", "*1"},
                               {"target", "c1"},
                               {"team", "lift"},
                               {"spawnflags", "1"},
                               {"speed", "100"}}));
  w.entities.push_back(entity({{"classname", "func_wall"}, {"model", "*2"}, {"team", "lift"}}));
  Pairs c1 = {{"classname", "path_corner"},
              {"targetname", "c1"},
              {"target", "c2"},
              {"origin", "-32 -32 200"}};
  for (const auto& kv : corner) c1.push_back(kv);
  w.entities.push_back(entity(c1));
  w.entities.push_back(entity({{"classname", "path_corner"},
                               {"targetname", "c2"},
                               {"target", "c1"},
                               {"origin", "480 -32 200"}}));
  w.entities.push_back(entity({{"classname", "func_rotating"},
                               {"model", "*3"},
                               {"targetname", "fan"},
                               {"target", "fanned"},
                               {"spawnflags", "8196"},  // ACCEL, about z
                               {"speed", "100"},
                               {"accel", "20"},
                               {"decel", "50"}}));
  // What the fan fires: a wall that shows and hides by each use (TRIGGER_SPAWN, TOGGLE).
  w.models.push_back({box(-600, 500, 0, -500, 600, 64)});
  w.entities.push_back(entity({{"classname", "func_wall"},
                               {"model", "*4"},
                               {"targetname", "fanned"},
                               {"spawnflags", "3"}}));
  w.entities.push_back(
      entity({{"classname", "trigger_relay"}, {"targetname", "go"}, {"target", "fan"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  auto h = std::make_unique<Hall>();
  sv::Q2ServerOptions o;
  if (pack) o.pack = sv::Q2Pack::ground_zero;
  h->r = sv::q2_runner(sv::make_q2_server(o));
  h->r.console("deathmatch 0");
  sv::MapSpec spec;
  spec.name = "hall";
  spec.brushes = map;
  CHECK_MSG(h->r.start(spec, error), error);
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

}  // namespace

TEST(in_the_pack_a_train_leaving_a_corner_takes_the_corner_s_speed) {
  auto h = open_hall(true, {{"speed", "400"}});
  run(*h, kSecond);
  const sv::Q2Entity& train = h->named("func_train");
  CHECK_MSG(train.speed == 400 && train.accel == 400 && train.decel == 400,
            std::to_string(train.speed));
  CHECK_MSG(train.origin.x > 150, std::to_string(train.origin.x));  // 400 a second, not 100
  auto base = open_hall(false, {{"speed", "400"}});
  run(*base, kSecond);
  CHECK(base->named("func_train").speed == 100);
  CHECK_MSG(base->named("func_train").origin.x < 100,
            std::to_string(base->named("func_train").origin.x));
}

TEST(in_the_pack_a_corner_with_its_own_accel_makes_the_leg_an_accelerated_move) {
  auto h = open_hall(true, {{"speed", "40"}, {"accel", "4"}, {"decel", "4"}});
  run(*h, kSecond / 4);  // found its first corner and set off
  const sv::Q2Entity& train = h->named("func_train");
  CHECK(train.speed == 40 && train.accel == 4 && train.decel == 4);
  CHECK(train.accelerated);
  // Units per 0.1 s frame: from rest, 4 more each frame, so slow at first and then fast.
  const float early = train.origin.x;
  run(*h, kSecond);  // 4, 8, 12 ... a frame: past 150 in a second, where a plain 40 a second is 40
  CHECK_MSG(h->named("func_train").origin.x - early > 150,
            std::to_string(h->named("func_train").origin.x - early));
}

TEST(in_the_pack_a_train_s_team_rides_with_it_and_outside_it_stays) {
  auto h = open_hall(true, {});
  const float wall0 = h->named("func_wall").origin.x, train0 = h->named("func_train").origin.x;
  run(*h, kSecond * 2);
  const float moved = h->named("func_train").origin.x - train0;
  CHECK_MSG(moved > 100, std::to_string(moved));
  CHECK_MSG(
      h->named("func_wall").origin.x - wall0 == moved,
      std::to_string(h->named("func_wall").origin.x - wall0) + " vs " + std::to_string(moved));
  auto base = open_hall(false, {});
  const float still = base->named("func_wall").origin.x;
  run(*base, kSecond * 2);
  CHECK(base->named("func_wall").origin.x == still);
}

TEST(in_the_pack_an_accel_fan_spins_up_fires_its_targets_and_slows_to_rest) {
  auto h = open_hall(true, {});
  run(*h, 2);
  const auto index = [&](const char* name) {
    for (size_t i = 0; i < h->s().entities.size(); i++)
      if (h->s().entities[i].targetname == name) return static_cast<int>(i);
    return -1;
  };
  const auto fan = [&]() -> sv::Q2Entity& { return h->s().entities[size_t(index("fan"))]; };
  const auto shown = [&] { return h->s().entities[size_t(index("fanned"))].solid; };
  CHECK(fan().avelocity.z == 0 && !shown());
  sv::map_use_targets(h->s(), index("go"), -1);
  run(*h, kSecond / 10 + 2);  // a step or two: 20 degrees a second more each 0.1 s
  CHECK_MSG(fan().avelocity.z > 0 && fan().avelocity.z < 100, std::to_string(fan().avelocity.z));
  CHECK(!shown());
  run(*h, kSecond);  // full speed (100) after half a second: its targets fire once
  CHECK_MSG(fan().avelocity.z == 100, std::to_string(fan().avelocity.z));
  CHECK(shown());
  sv::map_use_targets(h->s(), index("go"), -1);  // stopped: 50 less each 0.1 s, then they fire
  run(*h, kSecond / 10 + 2);
  CHECK_MSG(fan().avelocity.z > 0 && fan().avelocity.z < 100, std::to_string(fan().avelocity.z));
  run(*h, kSecond);
  CHECK(fan().avelocity.z == 0 && !shown());
  // Outside the pack a use starts it at full speed at once and fires nothing.
  auto base = open_hall(false, {});
  run(*base, 2);
  int go = -1, wall = -1;
  for (size_t i = 0; i < base->s().entities.size(); i++) {
    if (base->s().entities[i].targetname == "go") go = static_cast<int>(i);
    if (base->s().entities[i].targetname == "fanned") wall = static_cast<int>(i);
  }
  sv::map_use_targets(base->s(), go, -1);
  run(*base, 2);
  CHECK(!base->s().entities[size_t(wall)].solid);
}
