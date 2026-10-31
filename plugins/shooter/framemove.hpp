// THE ORIGINAL PLAYER MOVE: the server's own step for a walking player, run once a step of length
// `dt` with the player's latest command (the other half of the movement setting beside the
// predicted move, pmove.hpp). It works on the loop's entity over the loop's traces, so a campaign
// host and a match host can both run it.
//
// ONE STEP, in order:
//  1. `frame_think`: the punch decays by 10 degrees a second of its length; a dead player
//     (health 0 or less) is done; the body's angles from the view (pitch minus a third of the
//     view's, yaw the view's, roll four times the roll the velocity gives along the body's right,
//     scaled by `rollspeed` up to `rollangle`), unless the program fixes the view; the wish
//     velocity from the body's forward and right (its z 0 when walking; a negative forward move
//     counts 0 while `teleport_time` is ahead), capped at `maxspeed`; then one of: noclip (the
//     velocity is the wish), a water jump (the horizontal velocity is `movedir`'s until the time
//     passes `teleport_time` or the player leaves the water), swimming (water level 2 or more:
//     the wish from the VIEW's axes with the up move added, or a drift of 60 down with no input,
//     0.7 of the capped speed; water friction, then acceleration), the ground (friction, the edge
//     friction where the floor 16 ahead falls away more than 34, then acceleration) or the air
//     (acceleration with the wish speed capped at 30 for the gain, never for the step).
//  2. the program's pre-think (`frame_program_move` when the engine moves for a multiplayer
//     program, or a test: the water drag, the jump out of water, the jump or the swim up).
//  3. `frame_move`: the water level; gravity unless swimming or water-jumping; each velocity
//     component bounded; unstick (back to `oldorigin`, then a nudge of up to one unit across and
//     17 up); the walk move: slide, and after a wall, the step up 18 and slide again (unsticking
//     when that one did not move, with wall friction against a wall faced), the step down, kept
//     only onto a floor (normal z above 0.7). Returns the slide's blocked bits, + 256 when the step
//     up was tried.
//
// ARITHMETIC: floats stay floats, the step length and every literal like 0.7 are doubles, so a
// product with them is a double rounded to float where it is stored; square roots and sines in
// double. The test vectors (tests/golden/shooter_framemove.txt) hold every step.
//
// ge:summary The original player move: the think, the walk move with its step and unstick, swim
// ge:keywords framemove original-move walk-move step-up step-down unstick wall-friction
// ge:keywords edge-friction air-accelerate swim water-jump water-level roll punch frame_think
// ge:keywords frame_move frame_program_move sv_playermove
// ge:skill shooter
// ge:test tests/shooter_framemove_test.cpp
#pragma once

#include "shooter/server.hpp"

namespace ge::shooter {

constexpr float kStepHeight = 18;

// The original move's settings (the loop's gravity and maxvelocity are `ServerSettings`').
struct FrameMoveSettings {
  float friction = 4;      // sv_friction
  float edgefriction = 2;  // edgefriction: the factor where the ground ahead falls away
  float stopspeed = 100;   // sv_stopspeed
  float accelerate = 10;   // sv_accelerate: ground, air and water
  float maxspeed = 320;    // sv_maxspeed
  bool nostep = false;     // sv_nostep
  float rollangle = 2;     // cl_rollangle, degrees
  float rollspeed = 200;   // cl_rollspeed, units/s
};

// A command as the step reads it: the view and the three movement axes.
struct FrameCommand {
  Vec3f view;  // pitch, yaw, roll in degrees
  float forward = 0, side = 0, up = 0;
};

// The settings a server's original move runs with, and a command as the step reads it.
FrameMoveSettings frame_settings(const ServerSettings& set);
FrameCommand frame_command(const MoveCmd& cmd);

// Step 1 for player `e`.
void frame_think(Server& sv, int e, const FrameCommand& cmd, const FrameMoveSettings& set,
                 double dt);

// The program's movement for player `e`, as the classic single-player program does it in its
// pre-think: the water drag, the jump out of water at level 2, then the jump (or the swim up) with
// the button held, else the jump marked released.
void frame_program_move(Server& sv, int e, bool jump, double dt);

// Step 3 for player `e`: returns the walk move's clip bits (+ 256: the step up was tried). A
// noclip player flies by its velocity; any other movetype but walk does not move here.
int frame_move(Server& sv, int e, const FrameMoveSettings& set, double dt);

// The water level and type at the player's feet, middle and eye (`view_ofs`).
void frame_water_level(Server& sv, int e);

}  // namespace ge::shooter
