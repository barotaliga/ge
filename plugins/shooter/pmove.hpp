// THE CLASSIC PLAYER MOVE: one command moves one player by its own duration, in 32-bit floats
// over clip hulls, as a server and a predicting client both run it. The move a classic-content
// player expects to the unit: ground friction (doubled at an edge) and acceleration, an air whose
// 30 unit/s wish cap rewards turning (strafe jumping, bunny hopping), the jump that must be
// pressed afresh, stairs climbed up to 18 units, slopes steeper than 0.7 slid down, swimming,
// the water jump onto a bank, flying, spectating, and the server settings that tune it.
//
// One command, in order (`player_move`):
//  1. dt = msec x 0.001; the touch list is emptied.
//  2. none and lock: categorise the position and stop.
//  3. the view from the command; forward, right and up from it.
//  4. spectators fly through everything (their own friction and acceleration) and stop.
//  5. UNSTICK: the first free of 27 points 1/8 unit apart (z outermost, then y, then x, each
//     0, -1/8, +1/8), then the origin raised 1..18 units; else the original is kept.
//  6. CATEGORISE: on the ground when a trace one unit down hits a plane with normal z >= 0.7
//     (never while rising faster than 180); the water level from the world's point hull at the
//     feet + 1, the middle and the eyes; then snapped to the ground trace's end.
//  7. the water jump check at water level 2; 8. the water jump's clock; 9. the jump;
//  10. friction; 11. the water, fly or walk/air move; 12. categorise again; 13. the landing fix.
// The rules of each step are stated beside it in pmove.cpp and slide.cpp.
//
// Pure, allocation-free, no clock, transcendentals through core's libm only. Held within the
// recordings' tolerance by tests/shooter_recording_test.cpp (tests/golden/shooter_recording.txt),
// to its own bits by tests/shooter_pmove_test.cpp, and drawn by the `pmove` lab.
//
// ge:summary The classic player move: one command over clip hulls in float32, every move type
// ge:keywords pmove player-move movement usercmd command msec friction accelerate air-accelerate
// ge:keywords strafe-jump bunny-hop jump ktjump waterjump swim fly spectator noclip dead
// ge:keywords stopspeed maxspeed gravity categorize unstick nudge ground slidefix airstep pground
// ge:keywords bunnyspeedcap move-type player_move run_command move_settings move_state move_cmd
// ge:skill shooter move
// ge:test tests/shooter_pmove_test.cpp tests/shooter_recording_test.cpp
#pragma once

#include <cstdint>

#include "shooter/world.hpp"

namespace ge::shooter {

enum class MoveType : uint8_t {
  normal,         // alive, walking
  dead,           // friction and collision, no acceleration; a held jump is remembered
  fly,            // three-dimensional acceleration with collision, its own friction
  spectator,      // free flight through everything
  old_spectator,  // the same, but a wish slower than the coast does not move it at all
  none,           // does not move; position categorised only
  lock,           // the server holds the origin and view; categorised only
};

constexpr uint8_t kButtonAttack = 1;
constexpr uint8_t kButtonJump = 2;
constexpr uint8_t kButtonUse = 4;
constexpr uint8_t kButtonAttack2 = 8;

// One command: how long it lasts and what the player asked for.
struct MoveCmd {
  uint8_t msec = 0;  // 0..255 ms; a command over 50 ms is split (`run_command`)
  Vec3f angles;      // the view: pitch (down positive), yaw, roll, degrees
  int16_t forward = 0, side = 0, up = 0;  // the movement axes, units/s (±508 on a grid of 4)
  uint8_t buttons = 0;                    // kButton* bits
  uint8_t impulse = 0;                    // a one-shot command for the game's logic
};

// The server settings the move reads, at the modern defaults. They travel to clients (the
// server info) so a client predicts with the same numbers.
struct MoveSettings {
  float gravity = 800;            // sv_gravity, units/s^2
  float stopspeed = 100;          // sv_stopspeed: friction acts on at least this speed
  float maxspeed = 320;           // sv_maxspeed: the wish speed's cap
  float spectatormaxspeed = 500;  // sv_spectatormaxspeed
  float accelerate = 10;          // sv_accelerate: on the ground, and in the air
  float airaccelerate = 10;       // sv_airaccelerate: sent to clients, unused by the move
  float wateraccelerate = 10;     // sv_wateraccelerate
  float friction = 4;             // sv_friction
  float waterfriction = 4;        // sv_waterfriction
  float ktjump = 1;               // pm_ktjump, 0..1: a weak jump pulled up toward 270
  float bunnyspeedcap = 0;        // pm_bunnyspeedcap: air gain capped at maxspeed x cap; 0 off
  bool slidefix = false;          // pm_slidefix: downward speed kept on the ground
  bool airstep = false;           // pm_airstep: steps climbed in the air (a console sets
                                  // pm_pground with it)
  bool pground = false;           // pm_pground: on the ground persists between moves
};

// What a player carries from one command to the next.
struct MoveState {
  Vec3f origin;    // 24 above the feet
  Vec3f velocity;  // units/s
  Vec3f angles;    // the view the last command set
  MoveType type = MoveType::normal;
  int ground = -1;           // the index of the solid stood on in the move's list; -1 off
  Vec3f ground_normal;       // the plane stood on (for the jump and the landing fix)
  bool jump_held = false;    // the jump button not released since the last jump
  float waterjump_time = 0;  // seconds left of a water jump, 0 when none
  int waterlevel = 0;        // 0 dry .. 3 eyes under
  int watertype = kContentsEmpty;
  // The player's own values the game's logic sets.
  float entgravity = 1;  // the entity's gravity factor
  float maxspeed = 320;  // the player's own cap (the game sets it, sv_maxspeed by default)
};

// The solids a move touched, each once, in the order touched.
struct TouchList {
  int ents[kMaxSolids];
  int count = 0;

  void add(int ent) {
    for (int i = 0; i < count; i++)
      if (ents[i] == ent) return;
    if (count < kMaxSolids) ents[count++] = ent;
  }
};

// Which kinds of plane the move's slide hit.
constexpr int kBlockedFloor = 1;  // normal z >= 0.7
constexpr int kBlockedStep = 2;   // normal z exactly 0: a wall
constexpr int kBlockedOther = 4;  // anything else

struct MoveResult {
  int blocked = 0;  // the walk/air, water or fly move's slide result; 0 when it did not slide
  TouchList touches;
};

// One command of at most 50 ms (`run_command` splits a longer one).
MoveResult player_move(MoveState& state, const MoveCmd& cmd, const MoveSettings& settings,
                       const MoveWorld& world);

// One command as a server runs it: longer than 50 ms, it is split into two halves of msec / 2
// (integer division: an odd millisecond is lost), the second with its impulse cleared, each half
// split again while still too long. Returns the last half's slide result and every half's
// touches.
MoveResult run_command(MoveState& state, const MoveCmd& cmd, const MoveSettings& settings,
                       const MoveWorld& world);

}  // namespace ge::shooter
