// THE SEQUEL'S PLAYER MOVE: one command moves one player through a world of brushes, as the
// sequel's server and its predicting client both run it. Its state is integers (origin and
// velocity in eighths), so a client predicts it bit for bit; the floats in between follow fixed
// precision rules (32-bit float everywhere, doubles only where stated, rounded to float when
// stored). Unlike the classic move (pmove.hpp) it has no server settings but gravity and the air
// acceleration, never splits a command, sees a third of the pitch when walking, steps up 18
// units after sliding (keeping whichever of the two moves went farther), ducks to 100 units/s,
// climbs ladders, rides water currents and conveyors, and takes a landing pause after a long fall.
//
// One command, in order (`sequel_move`):
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
// Every rule is stated beside its step in sequel_move.cpp; the precision notes there say where a
// double enters. A slanted plane's walkability compares its normal's z with 0.7 as a double.
// Pure and allocation-free; the world is the caller's (`SequelWorld`: a trace and the contents at
// a point). Held to its own bits by tests/classic_sequel_move_test.cpp
// (tests/golden/shooter_sequel_move.txt).
//
// ge:summary The sequel's player move: packed integer state, brushes, a third of the pitch, steps
// ge:keywords sequel sequel-move player-move packed-state eighths snap jitter ladder current
// ge:keywords conveyor duck landing water-jump step-slide clip-velocity air-accelerate gib
// ge:keywords freeze teleport-hold delta-angles view-height sequel_move sequel_world
// ge:keywords sequel_command jump-up-move
// ge:skill classic shooter
// ge:test tests/classic_sequel_move_test.cpp
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

enum class SequelType : uint8_t { normal = 0, spectator = 1, dead = 2, gib = 3, freeze = 4 };

// The packed state's flags.
inline constexpr uint8_t kSqDucked = 1, kSqJumpHeld = 2, kSqOnGround = 4, kSqTimeWaterJump = 8,
                         kSqTimeLand = 16, kSqTimeTeleport = 32, kSqNoPrediction = 64;

// What a player carries from command to command, and what a predicting client is sent.
struct SequelState {
  SequelType type = SequelType::normal;
  int16_t origin[3] = {};    // eighths of a unit
  int16_t velocity[3] = {};  // eighths of a unit/s
  uint8_t flags = 0;         // kSq*
  uint8_t time = 0;          // the flags' timer, in 8 ms
  int16_t gravity = 800;     // units/s², down
  int16_t delta_angles[3] = {};

  bool operator==(const SequelState&) const = default;
};

struct SequelCmd {
  uint8_t msec = 0;
  uint8_t buttons = 0;
  int16_t angles[3] = {};  // 65536ths of a turn
  int16_t forward = 0, side = 0, up = 0;
  uint8_t impulse = 0;
  uint8_t light = 0;
};

// The up move the classic protocol's jump button is (the sequel's jump: an up move of 10 or more).
inline constexpr int16_t kSqJumpUp = 200;

// A classic protocol's command (`MoveCmd`, degrees) as the sequel's move reads it: each angle in
// 65536ths of a turn (degrees × 65536 / 360 in double, wrapped to 16 bits), the moves, buttons and
// impulse as they are, and the jump button as the up move (`kSqJumpUp`, never below a held up
// move), so a let-go jump button lets the next jump come.
SequelCmd sequel_command(const MoveCmd& c);

// One trace's answer, as the world gives it.
struct SequelTrace {
  bool all_solid = false, start_solid = false;
  float fraction = 1;
  Vec3f end;
  Vec3f normal;  // the plane hit
  int32_t surface_flags = 0;
  int32_t contents = 0;
  int entity = -1;  // the entity hit, or -1 for none
};

// The world a move collides through. The trace's mask (a living or a dead player's) is the
// world's to choose.
struct SequelWorld {
  void* ctx = nullptr;
  SequelTrace (*trace)(void* ctx, Vec3f start, Vec3f mins, Vec3f maxs, Vec3f end) = nullptr;
  int32_t (*contents)(void* ctx, Vec3f p) = nullptr;
};

inline constexpr int kSqMaxTouch = 32;

struct SequelMove {
  // In: the state, the command, whether the game changed the state since the last move, and
  // the air acceleration the server set (only whether it is 0 matters). A game may run it on
  // other ground (classic remix's `sv_physics`): the running speed's cap, the ground's friction,
  // the landing pause and the water (the swimmer's share of the wish, the water's friction, the
  // speed up a water jump gives).
  SequelState s;
  SequelCmd cmd;
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
  // Out.
  int touch[kSqMaxTouch] = {};
  int touches = 0;
  Vec3f view_angles;
  float view_height = 0;
  Vec3f mins, maxs;
  int ground_entity = -1;
  int32_t water_type = 0;
  int water_level = 0;
};

// The ground, air and water a server runs its players' moves on (SequelMove's knobs above): what a
// client must hold to predict the same move. The default is the stock move with no air
// acceleration.
struct SequelFeel {
  float max_speed = 300, friction = 6, air_accelerate = 0;
  bool land_pause = true;
  float swim_scale = 0.5f, water_friction = 1, water_jump = 350;
  bool air_step = true;      // false: no step climbed in the air, so no stair's double jump
  bool surface_lift = true;  // false: no fixed lift for jump held under water
};

// `f` onto a move about to run.
inline void sequel_feel(SequelMove& pm, const SequelFeel& f) {
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
void sequel_move(SequelMove& pm, const SequelWorld& world);

// A world of one map's brushes through `tracer` (model 0) with `mask`. The world is the hit
// entity of every trace (0), as a server's is; the contents are the map's.
struct SequelBrushWorld {
  brush::BrushTracer* tracer = nullptr;
  int32_t mask = kSqMaskPlayerSolid;
  SequelWorld world();
};

}  // namespace ge::classic
