// QUAKE II'S PLAYER MOVE: one command moves one player through a world of brushes, as the
// Quake II's server and its predicting client both run it. Its state is integers (origin and
// velocity in eighths), so a client predicts it bit for bit; the floats in between follow fixed
// precision rules (32-bit float everywhere, doubles only where stated, rounded to float when
// stored). Unlike the classic move (pmove.hpp) it has no server settings but gravity and the air
// acceleration, never splits a command, sees a third of the pitch when walking, steps up 18
// units after sliding (keeping whichever of the two moves went farther), ducks to 100 units/s,
// climbs ladders, rides water currents and conveyors, and takes a landing pause after a long fall.
//
// One command, in order (`q2_move`):
//  1. the outputs zeroed; origin and velocity unpacked (× 0.125), the packed origin kept as the
//     previous origin; seconds = msec × 0.001 in double.
//  2. the view: each angle the command's plus the delta angle as a wrapping 16-bit integer, in
//     degrees (× 360 / 65536 in double); a pitch held at 89; a teleport hold sees the yaw alone.
//  3. a spectator flies through everything and is packed. The dead, the gibbed and the frozen
//     lose their moves; the frozen stop (state untouched).
//  4. the box and view height (standing, ducked, gib); ducking on the ground with up < 0, the
//     dead always ducked, standing up where the standing box fits.
//  5. a changed state (`changed`: a teleport, a spawn) searches 27 eighths for a free start.
//  6. categorise (the ground 0.25 below, the landing pause; the water level at three heights).
//  7. the dead slow by 20 a command on the ground; the ladder and the water jump are checked.
//  8. the timer; a teleport hold, a water jump (gravity, ends when falling), or the jump,
//     friction and the water or air move (accelerate, step slide).
//  9. categorise again; pack: velocity × 8 truncated, origin × 8 truncated and nudged an eighth
//     toward its sign on the first of eight axis sets whose box is free, else the previous origin.
//
// Every rule is stated beside its step in q2_move.cpp; the precision notes there say where a
// double enters. A slanted plane's walkability compares its normal's z with 0.7 as a double.
// Pure and allocation-free; the world is the caller's (`Q2World`: a trace and the contents at
// a point). Held to its own bits by tests/classic_q2_move_test.cpp
// (tests/golden/shooter_q2_move.txt).
//
// ge:summary Quake II's player move: packed integer state, brushes, a third of the pitch, steps
// ge:keywords q2 q2-move player-move packed-state eighths snap jitter ladder current
// ge:keywords conveyor duck landing water-jump step-slide clip-velocity air-accelerate gib
// ge:keywords freeze teleport-hold delta-angles view-height q2_move q2_world
// ge:keywords q2_command jump-up-move
// ge:skill classic shooter
// ge:test tests/classic_q2_move_test.cpp
#pragma once

#include <cstdint>

#include "shooter/brush_contents.hpp"
#include "shooter/pmove.hpp"
#include "shooter/vec.hpp"

namespace ge::brush {
class BrushTracer;
}

namespace ge::classic {

using ge::shooter::kSqMaskPlayerSolid;
using ge::shooter::MoveCmd;
using ge::shooter::MoveState;
using ge::shooter::Vec3f;

enum class Q2Type : uint8_t { normal = 0, spectator = 1, dead = 2, gib = 3, freeze = 4 };

// The packed state's flags.
inline constexpr float kSqViewStand = 22, kSqViewDucked = -2;  // the eye over the origin
inline constexpr uint16_t kSqDucked = 1, kSqJumpHeld = 2, kSqOnGround = 4, kSqTimeWaterJump = 8,
                          kSqTimeLand = 16, kSqTimeTeleport = 32, kSqNoPrediction = 64;
// The newer game interface's (version 3302) two more: players pass through one another, and the
// player is on a ladder (set or cleared by every move that is not a spectator's, `ladder_flag`).
inline constexpr uint16_t kSqIgnorePlayers = 128, kSqOnLadder = 256;

// What a player carries from command to command, and what a predicting client is sent. The
// original interface keeps the origin and velocity in 16 bits and the flags and timer in 8; the
// newer one widens them (`Q2Move::coord_bits`, `time_shift` say how much of the width is used).
struct Q2State {
  Q2Type type = Q2Type::normal;
  int32_t origin[3] = {};    // eighths of a unit
  int32_t velocity[3] = {};  // eighths of a unit/s
  uint16_t flags = 0;        // kSq*
  uint16_t time = 0;         // the flags' timer, in 8 ms (`time_shift` 3) or in ms (0)
  int16_t gravity = 800;     // units/s², down
  int16_t delta_angles[3] = {};

  bool operator==(const Q2State&) const = default;
};

struct Q2Cmd {
  uint8_t msec = 0;
  uint8_t buttons = 0;
  int16_t angles[3] = {};  // 65536ths of a turn
  int16_t forward = 0, side = 0, up = 0;
  uint8_t impulse = 0;
  uint8_t light = 0;
};

// The up move the classic protocol's jump button is (Quake II's jump: an up move of 10 or more).
inline constexpr int16_t kSqJumpUp = 200;

// A classic protocol's command (`MoveCmd`, degrees) as Quake II's move reads it: each angle in
// 65536ths of a turn (degrees × 65536 / 360 in double, wrapped to 16 bits), the moves, buttons and
// impulse as they are, and the jump button as the up move (`kSqJumpUp`, never below a held up
// move), so a let-go jump button lets the next jump come.
Q2Cmd q2_command(const MoveCmd& c);

// One trace's answer, as the world gives it.
struct Q2Trace {
  bool all_solid = false, start_solid = false;
  float fraction = 1;
  Vec3f end;
  Vec3f normal;  // the plane hit
  int32_t surface_flags = 0;
  int32_t contents = 0;
  int entity = -1;       // the entity hit, or -1 for none
  float plane_dist = 0;  // the plane hit's distance, kind and sign bits (the ground plane's)
  uint8_t plane_type = 0, plane_signbits = 0;
};

// The plane a move stood on (the newer interface's ground plane): all zero when in the air.
struct Q2Plane {
  Vec3f normal;
  float dist = 0;
  uint8_t type = 0, signbits = 0;
};

// The world a move collides through. The trace's mask (a living or a dead player's) is the
// world's to choose.
struct Q2World {
  void* ctx = nullptr;
  Q2Trace (*trace)(void* ctx, Vec3f start, Vec3f mins, Vec3f maxs, Vec3f end) = nullptr;
  int32_t (*contents)(void* ctx, Vec3f p) = nullptr;
  // A trace with the caller's content mask (the newer interface's): the ladder probe uses it with
  // kSqLadder when `ladder_mask` is set. Null: the probe takes the plain trace.
  Q2Trace (*trace_mask)(void* ctx, Vec3f start, Vec3f mins, Vec3f maxs, Vec3f end,
                        int32_t mask) = nullptr;
};

inline constexpr int kSqMaxTouch = 32;

struct Q2Move {
  // In: the state, the command, whether the game changed the state since the last move, and
  // the air acceleration the server set (only whether it is 0 matters). A game may run it on
  // other ground (classic remix's `sv_physics`): the running speed's cap, the ground's friction,
  // the landing pause and the water (the swimmer's share of the wish, the water's friction, the
  // speed up a water jump gives).
  Q2State s;
  Q2Cmd cmd;
  bool changed = false;
  float air_accelerate = 0;
  float max_speed = 300;
  float friction = 6;
  bool land_pause = true;  // a landing from a long fall holds the next jump (step 6)
  float swim_scale = 0.5f;
  float water_friction = 1;
  float water_jump = 350;
  // Jump held at waist depth or deeper lifts at a fixed speed by the liquid (100 in water, 80 in
  // slime, 50 in lava). False: no lift, the swim's own up move alone.
  bool surface_lift = true;
  // Steps climbed in the air as on the ground (the step slide), so a jump that reaches a ledge
  // still rising lands on it carrying the rise, and a jump from there adds to it: the stair's
  // double jump. False: the air's plain slide, the ledge climbed only by rising past it.
  bool air_step = true;
  // A spectator's flight: its step's time scaled (a later protocol's clients fly at twice the
  // time: twice as far, friction and acceleration twice as hard), its friction (0: 1.5 times
  // `friction`, 9), and whether it glides on when no more speed is wanted. False: the original
  // step, which then leaves the position where it was for that command.
  float fly_time = 1;
  float fly_friction = 0;
  bool fly_glide = true;
  // The packed state's width (the newer interface on an extended server): the origin and
  // velocity kept in `coord_bits` bits (16 wraps as the original does; 23 reaches ±524288 units)
  // and the timer in units of 2^`time_shift` ms (3: 8 ms, the original's; 0: milliseconds).
  int coord_bits = 16;
  int time_shift = 3;
  // The newer interface's move: the ladder probe traces with the ladder's mask (`trace_mask`),
  // kSqOnLadder is set or cleared at the end of each move that is not a spectator's, and
  // `ground_plane` is filled from the ground trace.
  bool ladder_mask = false;
  bool ladder_flag = false;
  // Out.
  int touch[kSqMaxTouch] = {};
  int touches = 0;
  Vec3f view_angles;
  float view_height = 0;
  Vec3f mins, maxs;
  int ground_entity = -1;
  Q2Plane ground_plane;
  int32_t water_type = 0;
  int water_level = 0;
};

// The ground, air and water a server runs its players' moves on (Q2Move's knobs above): what a
// client must hold to predict the same move. The default is the stock move with no air
// acceleration.
struct Q2Feel {
  float max_speed = 300, friction = 6, air_accelerate = 0;
  bool land_pause = true;
  float swim_scale = 0.5f, water_friction = 1, water_jump = 350;
  bool air_step = true;      // false: no step climbed in the air, so no stair's double jump
  bool surface_lift = true;  // false: no fixed lift for jump held under water
};

// A spectator's flight as a client of `protocol` predicts it: 34 the original step (no glide), 35
// at twice the time, 36 at twice the time with friction 4, gliding on.
inline void q2_fly_for_protocol(Q2Move& pm, int protocol) {
  pm.fly_time = protocol >= 35 ? 2.0f : 1.0f;
  pm.fly_friction = protocol >= 36 ? 4.0f : 0.0f;
  pm.fly_glide = protocol >= 36;
}

// `f` onto a move about to run.
inline void q2_feel(Q2Move& pm, const Q2Feel& f) {
  pm.max_speed = f.max_speed;
  pm.friction = f.friction;
  pm.air_accelerate = f.air_accelerate;
  pm.land_pause = f.land_pause;
  pm.swim_scale = f.swim_scale;
  pm.water_friction = f.water_friction;
  pm.water_jump = f.water_jump;
  pm.air_step = f.air_step;
  pm.surface_lift = f.surface_lift;
}

// One command (above). `pm.s` is the new state.
void q2_move(Q2Move& pm, const Q2World& world);

// A world of one map's brushes through `tracer` (model 0) with `mask`. The world is the hit
// entity of every trace (0), as a server's is; the contents are the map's.
struct Q2BrushWorld {
  brush::BrushTracer* tracer = nullptr;
  int32_t mask = kSqMaskPlayerSolid;
  Q2World world();
};

}  // namespace ge::classic
