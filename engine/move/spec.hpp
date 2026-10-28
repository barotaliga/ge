// THE KNOBS OF A MOVING CHARACTER: every number that decides how a body walks, runs, stops,
// jumps, falls, steps, crouches, slides and steers in the air, in one flat struct a game fills
// (or starts from a preset) and the console binds field by field (knobs.hpp names each one).
// Nothing here is a ski, a gun or a platform: a shooter's player, a platformer's hero, a
// third-person walker and a spectator's camera are each a row of these numbers.
//
// UNITS. The struct is unit-free: L is the game's length unit (a metre, or a map unit), every
// speed L/s, every acceleration L/s^2. `scale_lengths` converts a row between units; the factors
// (`accelerate`, `friction`, ...) are per second and do not scale.
//
// THE MODEL each knob feeds is docs/movement.md § The model; in short:
//   - on the ground, FRICTION takes `max(speed, stop_speed) * friction * dt` off the speed (times
//     `edge_friction` near a drop), then ACCELERATION adds up to `accelerate * wish * dt` along
//     the wish direction, but never past `wish` along it;
//   - in the air the same acceleration with `air_accelerate`, where the speed along the wish
//     direction is held to `air_speed_cap` instead of the wish speed: a small cap is what lets a
//     player gain speed by turning while strafing (strafe jumping, bunny hopping); 0 is no cap
//     (full air control, a platformer's);
//   - `air_control` turns the velocity toward the view while only forward is held, and
//     `air_strafe_*` take over while only sideways is held (the strafe-and-turn style);
//   - JUMPING sets (or adds) `jump_speed` upward; `jump_hold` lets a held button jump again on
//     landing (an automatic bunny hop), `jump_buffer` keeps an early press, `coyote_time` allows
//     a jump just after walking off a ledge, `jump_cut` shortens a jump let go early, and
//     `air_jumps` are double jumps;
//   - STEPS up to `step_height` are climbed, slopes steeper than `walk_normal` are slid down
//     (a surf ramp), `ground_snap` keeps a fast walker on a ramp's crest and down stairs;
//   - CROUCHING lowers the hull to `crouch_height` (pulling the feet up in the air when
//     `crouch_jump`), and above `slide_min_speed` it starts a SLIDE on `slide_friction`.
//
// ge:summary Character movement's knobs (MoveSpec): ground, air, jump, step, slope, crouch, slide
// ge:keywords move movement character controller player knobs tuning feel spec movespec units
// ge:keywords accelerate friction stop-speed gravity jump air-accelerate air-control strafe
// ge:keywords strafe-jump bunny-hop bhop coyote-time jump-buffer jump-cut double-jump air-jumps
// ge:keywords step-height walk-normal slope crouch slide ground-snap edge-friction scale_lengths
// ge:skill move
// ge:test tests/move_step_test.cpp
#pragma once

#include <cstdint>

namespace ge::move {

struct MoveSpec {
  double unit = 32;  // L in a metre: what the labs and a camera convert by (not a knob of feel)

  // ─── the body ───────────────────────────────────────────────────────────────────────────
  double radius = 16;         // L: half the hull's width (a square box in plan)
  double height = 56;         // L: the hull standing, feet to the top of the head
  double crouch_height = 32;  // L: the hull crouched
  double eye_height = 46;     // L: the eyes above the feet, standing (for a camera)
  double crouch_eye = 22;     // L: the eyes above the feet, crouched

  // ─── on the ground ──────────────────────────────────────────────────────────────────────
  double max_speed = 320;       // L/s: the wish speed with the stick (or a key) fully over
  double back_scale = 1;        // share of `max_speed` going backwards
  double side_scale = 1;        // share of `max_speed` straight sideways
  double walk_scale = 0.5;      // share while the walk button is held
  double sprint_scale = 1;      // share while sprint is held (1: no sprint)
  double crouch_scale = 0.333;  // share while crouched (not sliding)
  double accelerate = 10;       // 1/s: the ground's acceleration factor
  double friction = 4;          // 1/s: the ground's friction factor
  double stop_speed = 100;      // L/s: friction acts as if the speed were at least this
  double edge_friction = 2;     // friction's factor where the ground ahead drops away (1: none)
  double edge_probe = 16;       // L: how far ahead of the feet the drop is looked for
  double walk_normal = 0.7;     // the least up part of a ground's normal that is walkable
  double step_height = 18;      // L: the tallest step walked up without a jump
  double ground_snap = 0;       // L: a drop this small under a walker is followed, not fallen off
  double contact = 0.25;        // L: a gap under the feet this small still counts as standing
  double unground_speed = 180;  // L/s: rising faster than this leaves the ground

  // ─── in the air ─────────────────────────────────────────────────────────────────────────
  double gravity = 800;              // L/s^2: down
  double fall_gravity_scale = 1;     // gravity's factor while falling (a platformer's heavier fall)
  double terminal_speed = 0;         // L/s: the fastest fall (0: none)
  double air_accelerate = 0.7;       // 1/s: the air's acceleration factor
  double air_speed_cap = 30;         // L/s: the speed along the wish held to this (0: no cap)
  double air_control = 0;            // L/s^2: forward-only steering of the velocity (0: none)
  double air_strafe_accelerate = 0;  // 1/s: the factor while only sideways is held (0: as air)
  double air_strafe_cap = 0;         // L/s: the wish speed while only sideways is held (0: as air)
  double air_stop_accelerate = 0;    // 1/s: the factor while wishing against the velocity (0: air)
  double air_max_speed = 0;  // L/s: air acceleration lifts the plan speed no higher (0: no cap)

  // ─── jumping ────────────────────────────────────────────────────────────────────────────
  double jump_speed = 270;    // L/s: upward speed a jump gives
  bool jump_additive = true;  // a jump adds to a rising speed (a ramp's) instead of setting it
  bool jump_hold = false;     // a held button jumps again on landing (an automatic bunny hop)
  double jump_buffer = 0;     // s: a press this early before landing still jumps
  double coyote_time = 0;     // s: a jump this late after leaving the ground still counts
  double jump_cut = 1;        // the rising speed's factor when the button is let go (1: none)
  int32_t air_jumps = 0;      // jumps in the air before landing again (double jumps)
  double air_jump_speed = 0;  // L/s: upward speed an air jump sets (0: `jump_speed`)

  // ─── crouching and sliding ──────────────────────────────────────────────────────────────
  double crouch_time = 0;       // s: the eyes' travel between standing and crouched (0: at once)
  bool crouch_jump = true;      // crouching in the air pulls the feet up, the head staying put
  double slide_min_speed = 0;   // L/s: crouching faster than this slides (0: no slide)
  double slide_friction = 0.5;  // 1/s: friction's factor in a slide
  double slide_boost = 0;       // L/s: speed added as a slide starts
  double slide_cooldown = 1;    // s: before the boost is given again
  double slide_steer = 0.2;     // share of the ground's acceleration kept in a slide

  // ─── free flight (a spectator, a noclip developer) ──────────────────────────────────────
  double fly_scale = 1;  // share of `max_speed` flying

  // ─── ladders (a surface flagged kLadder, world.hpp) ─────────────────────────────────────
  double ladder_speed = 200;  // L/s: climbing, descending and sideways on the rungs
  double ladder_jump = 270;   // L/s: the push off the rungs a jump gives, out along their normal
  double ladder_probe = 1;    // L: how far ahead of the hull a ladder is found
};

// THE PRESETS, each a starting row for one kind of game (presets.cpp says what each is for).
// `classic_arena` and `strafe_arena` are in map units (about 32 to the metre); the others in
// metres.
MoveSpec classic_arena();   // the classic arena shooter: strafe jumping by turning in the air
MoveSpec strafe_arena();    // classic plus air control and the strafe-only air acceleration
MoveSpec modern_shooter();  // metres: sprint, crouch slide, coyote time and a jump buffer
MoveSpec platformer();      // metres: full air control, a jump cut, a heavier fall, a double jump
MoveSpec walker();          // metres: a person in a third-person game; no air steering to speak of

// A preset by name (`classic_arena`, `strafe_arena`, `modern_shooter`, `platformer`, `walker`);
// null when there is none. `preset_names` lists them in that order.
const MoveSpec* find_preset(const char* name);
const char* const* preset_names();  // null-terminated

// The row in another length unit: every L, L/s and L/s^2 times `k` (a map unit to metres:
// k = metres per unit), `unit` with them; factors, shares and times kept.
MoveSpec scale_lengths(const MoveSpec& spec, double k);

}  // namespace ge::move
