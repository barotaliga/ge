// Tests: the classic games' teleporters read off a level (plugins/classic/teleporters.hpp).
// Quake's trigger_teleport: its brush model's box grown a unit, the destination 27 over its marker,
// facing its angle, 300 units/s that way; one with a name or a destination that is not a marker
// left out. Quake II's misc_teleporter: the 16 unit box over its disc, 10 over its destination,
// held 160 ms; its arrival on the packed state turns the view by the delta angles from the
// command that touched it. Push triggers: their way (up, down or the angle's) times their speed
// times ten, a one-shot left out, Quake II's each axis held to 4095 and off the ground.
//
// ge:summary Tests: the classic games' teleporters as a client foresees them
// ge:keywords test teleporters q1_teleporters q2_teleporters q2_teleport_state q1_pushers
// ge:keywords q2_pushers q2_push_state
// ge:skill classic testing
#include <cmath>

#include "brush/map.hpp"
#include "check.hpp"
#include "classic/teleporters.hpp"

namespace gc = ge::classic;
namespace gb = ge::brush;

namespace {

gb::Entity ent(std::vector<std::pair<std::string, std::string>> pairs) {
  return {std::move(pairs)};
}

}  // namespace

TEST(quakes_teleporters_are_read_off_the_trigger_and_its_marker) {
  gb::Map m;
  m.family = gb::Family::v29;
  m.models.resize(4);
  m.models[2].mins[0] = 100;
  m.models[2].mins[1] = -50;
  m.models[2].mins[2] = 0;
  m.models[2].maxs[0] = 140;
  m.models[2].maxs[1] = 50;
  m.models[2].maxs[2] = 100;
  m.entities = {
      ent({{"classname", "worldspawn"}}),
      ent({{"classname", "trigger_teleport"}, {"model", "*2"}, {"target", "t1"}}),
      ent({{"classname", "info_teleport_destination"},
           {"targetname", "t1"},
           {"origin", "500 600 24"},
           {"angle", "90"}}),
      // Switched on only by being used: never foreseen.
      ent({{"classname", "trigger_teleport"},
           {"model", "*3"},
           {"target", "t1"},
           {"targetname", "gate"}}),
      // A destination that is not a marker (a moving one): left out.
      ent({{"classname", "trigger_teleport"}, {"model", "*1"}, {"target", "train"}}),
      ent({{"classname", "misc_teleporttrain"}, {"targetname", "train"}}),
  };
  const auto t = gc::q1_teleporters(m);
  CHECK(t.size() == 1);
  CHECK(t[0].lo.x == 99 && t[0].hi.x == 141 && t[0].hi.z == 101);
  CHECK(t[0].dest.x == 500 && t[0].dest.y == 600 && t[0].dest.z == 51);
  CHECK(t[0].angles.y == 90 && t[0].hold_ms == 0);
  CHECK_MSG(t[0].velocity.y > 299.9f && t[0].velocity.x < 0.01f && t[0].velocity.x > -0.01f,
            std::to_string(t[0].velocity.x) + "," + std::to_string(t[0].velocity.y));
  CHECK(gc::level_teleporters(m).size() == 1);
}

TEST(quake_iis_teleporters_are_the_box_over_the_disc_and_the_held_arrival) {
  gb::Map m;
  m.family = gb::Family::v38;
  m.entities = {
      ent({{"classname", "worldspawn"}}),
      ent({{"classname", "misc_teleporter"}, {"origin", "64 0 16"}, {"target", "d"}}),
      ent({{"classname", "misc_teleporter_dest"},
           {"targetname", "d"},
           {"origin", "-300 200 40"},
           {"angles", "0 180 0"}}),
      ent({{"classname", "misc_teleporter"}, {"origin", "0 0 0"}}),  // no target: none
  };
  const auto t = gc::level_teleporters(m);
  CHECK(t.size() == 1);
  CHECK(t[0].lo.x == 55 && t[0].hi.x == 73 && t[0].lo.z == 23 && t[0].hi.z == 41);
  CHECK(t[0].dest.z == 50 && t[0].angles.y == 180 && t[0].hold_ms == 160);
  gc::Q2State s;
  s.velocity[0] = 2400;
  s.flags = gc::kSqOnGround;
  const int16_t cmd[3] = {0, 8192, 0};  // facing 45 when he touched it
  gc::q2_teleport_state(s, t[0], cmd);
  CHECK(s.origin[0] == -2400 && s.origin[1] == 1600 && s.origin[2] == 400);
  CHECK(s.velocity[0] == 0 && (s.flags & gc::kSqTimeTeleport) != 0 && s.time == 20);
  // The view: his 45 plus the delta is the destination's 180.
  CHECK(static_cast<int16_t>(cmd[1] + s.delta_angles[1]) == static_cast<int16_t>(32768));
}

TEST(push_triggers_set_the_speed_their_way_times_ten_and_quake_ii_holds_each_axis) {
  gb::Map m;
  m.models.resize(3);
  for (int k = 0; k < 3; k++) {
    m.models[1].mins[k] = -32;
    m.models[1].maxs[k] = 32;
  }
  m.entities = {
      ent({{"classname", "worldspawn"}}),
      ent({{"classname", "trigger_push"}, {"model", "*1"}, {"angle", "-1"}, {"speed", "50"}}),
      ent({{"classname", "trigger_push"}, {"model", "*1"}, {"angle", "90"}}),      // 1000 a default
      ent({{"classname", "trigger_push"}, {"model", "*1"}, {"spawnflags", "1"}}),  // once: gone
      ent({{"classname", "trigger_push"}, {"model", "*2"}}),  // no way: stops him
  };
  const auto q1 = gc::q1_pushers(m);
  CHECK(q1.size() == 3);
  CHECK(q1[0].push && q1[0].velocity.z == 500 && q1[0].lo.x == -33 && q1[0].hi.z == 33);
  CHECK(std::fabs(q1[1].velocity.y - 10000) < 0.5f && std::fabs(q1[1].velocity.x) < 0.5f);
  CHECK(q1[2].velocity.x == 0 && q1[2].velocity.y == 0 && q1[2].velocity.z == 0);
  // Quake II holds each axis to 4095 and lifts him off the ground.
  const auto q2 = gc::q2_pushers(m);
  CHECK(q2[1].velocity.y == gc::kQ2PushHeld);
  gc::Q2State s;
  s.flags = gc::kSqOnGround;
  gc::q2_push_state(s, q2[0]);
  CHECK(s.velocity[2] == 4000 && (s.flags & gc::kSqOnGround) == 0);
}
