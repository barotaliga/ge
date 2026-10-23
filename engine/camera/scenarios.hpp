// THE CAMERA'S SCRIPTED SUBJECTS: a body moving through a small world by a script, sampled at any
// display rate, so the camera's labs and tests run every rig through the same walks, runs,
// strafes, jumps, stairs, falls, carves, skids, woods, hits and teleports without a game. The body
// is stepped at a fixed 960 Hz and read between two steps by its time, so a run sampled at 30 Hz
// and one at 240 Hz follow the same path; the moments (landings, shots, hits, teleports) are
// delivered on the frame whose interval holds them.
//
//   walk       on flat ground, from 1.5 m/s up to 4 m/s, with a slow look round
//   strafe     running at 6 m/s with a 4 m/s strafe right, then left, then right
//   jump       running at 6 m/s, two jumps
//   stairs     walking up a flight of 12 steps 0.25 m high
//   crouch     walking, crouched from 1.5 s to 3.5 s
//   fire       standing, turning: three shots, a hit from the right, one from the front, a blast
//   cliff      running off a 25 m drop at 9 m/s, landing and running on
//   carve      S-turns down a 25 degree face with rolls, building to 22 m/s, the body banked
//   skid       down the face at 18 m/s, then the body swung 70 degrees across its way and braked
//   trees      weaving at 10 m/s between trunks either side, one standing on the line
//   teleport   running at 8 m/s, put 40 m to the side at 3 s
//
// The world they run in answers the camera's three queries (world.hpp): its ground, the line
// against its trunks, and the trunks near a point.
//
// ge:summary Scripted camera subjects (walk, strafe, jump, stairs, cliff, carve, skid, trees...)
// ge:keywords camera scenario scripted subject walk strafe jump stairs crouch fire cliff fall
// ge:keywords carve skid trees teleport world ground obstacles camera_scenarios script_run
// ge:keywords advance_to scenario_world
// ge:skill camera labs
// ge:test tests/camera_scenarios_test.cpp
#pragma once

#include <span>
#include <string_view>

#include "camera/subject.hpp"
#include "camera/world.hpp"

namespace ge::camera {

enum class Ground : uint8_t { flat, stairs, cliff, slope };

struct Scenario {
  std::string_view id;
  std::string_view what;
  double seconds = 6;
  Ground ground = Ground::flat;
  bool trees = false;
};

std::span<const Scenario> camera_scenarios();
const Scenario* find_camera_scenario(std::string_view id);

// The world a scenario runs in (its ground, its trunks).
World scenario_world(const Scenario& sc);
double scenario_ground(const Scenario& sc, double x, double z);

// A scenario being run: the body at the last two fixed steps and the moments still to deliver.
struct ScriptRun {
  const Scenario* sc = nullptr;
  double t = 0;  // the newest step's time
  struct Body {
    Vec3 pos{0, 0, 0}, vel{0, 0, 0};
    double heading = 0, roll = 0, pitch = 0, look = 0, speed = 0, side = 0, crouch = 0;
    bool grounded = true;
  } prev{}, cur{};
  double read_to = 0;  // the time the last sample was read at
  // Moments found by the steps since the last sample.
  int pending = 0;
  Event events[kMaxEvents]{};
};

ScriptRun start_script(const Scenario& sc);
// The subject at time `t_s` (non-decreasing), with the moments since the last call.
Subject advance_to(ScriptRun& run, double t_s);

}  // namespace ge::camera
