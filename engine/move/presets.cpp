// THE PRESETS: a starting row of MoveSpec for each kind of game, chosen so the `move-strafe` and
// `move-trace` labs show the kinds apart. A game copies the nearest and tunes it (by cvars, by a
// lab's `--set`); the rows here are starting points, measured, not anyone's final feel.
//
// ge:summary Movement presets: classic arena, strafe arena, modern shooter, platformer, walker
// ge:keywords move presets preset classic_arena strafe_arena modern_shooter platformer walker
// ge:keywords find_preset preset_names arena shooter fps third-person
// ge:skill move
// ge:test tests/move_step_test.cpp
#include <cstring>

#include "move/spec.hpp"

namespace ge::move {

// The classic arena shooter, in map units (a 56-unit player): the struct's defaults. A low air
// cap (30) with a small air factor is what makes strafe jumping: a player gains speed only by
// turning the view with the strafe while airborne, and jumping must be pressed again each time.
MoveSpec classic_arena() { return MoveSpec{}; }

// The classic row with the strafe-and-turn style's air: no air cap and a factor of 1 (speed is
// gained turning with forward and a strafe held), air control while only forward is held, a
// sharp strafe-only acceleration held to a small wish (circling tight turns), and a strong brake
// against the velocity.
MoveSpec strafe_arena() {
  MoveSpec s;
  s.air_accelerate = 1;
  s.air_speed_cap = 0;
  s.air_control = 4800;
  s.air_strafe_accelerate = 70;
  s.air_strafe_cap = 30;
  s.air_stop_accelerate = 2.5;
  return s;
}

// A modern shooter in metres: a 1.8 m soldier who sprints, slides out of a sprint, jumps a little
// late off a ledge and a little early before landing, and steers in the air, but strafing there
// never takes him past a sprint.
MoveSpec modern_shooter() {
  MoveSpec s;
  s.unit = 1;
  s.radius = 0.4;
  s.height = 1.8;
  s.crouch_height = 1.2;
  s.eye_height = 1.65;
  s.crouch_eye = 1.05;
  s.max_speed = 5.5;
  s.back_scale = 0.8;
  s.side_scale = 0.9;
  s.walk_scale = 0.5;
  s.sprint_scale = 1.45;
  s.crouch_scale = 0.45;
  s.accelerate = 10;
  s.friction = 6;
  s.stop_speed = 2.5;
  s.edge_friction = 1;
  s.edge_probe = 0.5;
  s.walk_normal = 0.7;
  s.step_height = 0.45;
  s.ground_snap = 0.5;
  s.contact = 0.02;
  s.unground_speed = 4;
  s.gravity = 16;
  s.terminal_speed = 55;
  s.air_accelerate = 2;
  s.air_speed_cap = 0.9;
  s.air_max_speed = 8;  // a sprint's speed: strafing in the air never takes him past it
  s.jump_speed = 6.2;
  s.jump_buffer = 0.12;
  s.coyote_time = 0.1;
  s.crouch_time = 0.12;
  s.slide_min_speed = 6.5;
  s.slide_friction = 0.6;
  s.slide_boost = 1.5;
  s.slide_cooldown = 1.2;
  s.slide_steer = 0.15;
  return s;
}

// A platformer's hero in metres: snappy on the ground, full steering in the air held to the run
// speed, a jump that is as high as the button is held, a heavier fall than rise, generous coyote
// time and buffer, and a double jump.
MoveSpec platformer() {
  MoveSpec s;
  s.unit = 1;
  s.radius = 0.35;
  s.height = 1.6;
  s.crouch_height = 0.9;
  s.eye_height = 1.45;
  s.crouch_eye = 0.8;
  s.max_speed = 8;
  s.accelerate = 15;
  s.friction = 12;
  s.stop_speed = 3;
  s.edge_friction = 1;
  s.edge_probe = 0.4;
  s.walk_normal = 0.6;
  s.step_height = 0.3;
  s.ground_snap = 0.4;
  s.contact = 0.02;
  s.unground_speed = 3;
  s.gravity = 30;
  s.fall_gravity_scale = 1.8;
  s.terminal_speed = 25;
  s.air_accelerate = 8;
  s.air_speed_cap = 0;
  s.air_max_speed = 8;
  s.jump_speed = 12;
  s.jump_additive = false;
  s.jump_buffer = 0.15;
  s.coyote_time = 0.12;
  s.jump_cut = 0.45;
  s.air_jumps = 1;
  s.air_jump_speed = 10;
  s.crouch_time = 0.08;
  return s;
}

// A person in a third-person game, in metres: walks at 1.6 m/s, runs at 5.4 m/s on sprint, jumps
// half a metre under the Earth's gravity, and can barely steer once off the ground (and never
// gains by it).
MoveSpec walker() {
  MoveSpec s;
  s.unit = 1;
  s.radius = 0.3;
  s.height = 1.75;
  s.crouch_height = 1.1;
  s.eye_height = 1.62;
  s.crouch_eye = 1.0;
  s.max_speed = 1.6;
  s.back_scale = 0.6;
  s.side_scale = 0.8;
  s.walk_scale = 0.5;
  s.sprint_scale = 3.4;
  s.crouch_scale = 0.5;
  s.accelerate = 8;
  s.friction = 8;
  s.stop_speed = 1;
  s.edge_friction = 1;
  s.edge_probe = 0.3;
  s.walk_normal = 0.75;
  s.step_height = 0.4;
  s.ground_snap = 0.4;
  s.contact = 0.02;
  s.unground_speed = 2.5;
  s.gravity = 9.81;
  s.terminal_speed = 55;
  s.air_accelerate = 0.5;
  s.air_speed_cap = 0.3;
  s.air_max_speed = 1.6;  // no faster for jumping; a running jump keeps its run
  s.jump_speed = 3.3;
  s.coyote_time = 0.08;
  s.crouch_time = 0.25;
  return s;
}

namespace {

struct Named {
  const char* name;
  MoveSpec spec;
};

const Named* table() {
  static const Named kTable[] = {{"classic_arena", classic_arena()},
                                 {"strafe_arena", strafe_arena()},
                                 {"modern_shooter", modern_shooter()},
                                 {"platformer", platformer()},
                                 {"walker", walker()}};
  return kTable;
}
constexpr int kPresets = 5;

}  // namespace

const MoveSpec* find_preset(const char* name) {
  for (int i = 0; i < kPresets; i++) {
    if (std::strcmp(table()[i].name, name) == 0) return &table()[i].spec;
  }
  return nullptr;
}

const char* const* preset_names() {
  static const char* const kNames[] = {"classic_arena", "strafe_arena", "modern_shooter",
                                       "platformer",    "walker",       nullptr};
  return kNames;
}

}  // namespace ge::move
