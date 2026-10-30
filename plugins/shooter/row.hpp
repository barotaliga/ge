// THE CLASSIC ROW ON THE ENGINE'S MOVER: the classic player move's numbers as rows of the engine's
// own mover (`engine/move`) and swimmer (`engine/water/swim`), and one classic command run
// through them over a brush world (hull_world.hpp). What a game on the engine's generic movement
// gets when it wants the classic feel; `player_move` (pmove.hpp) stays the bit-faithful one the
// recordings hold to their bits, and this row is measured against the same recordings within
// their own tolerance (row_recording.hpp), so the generic mover is known to carry the classic
// feel.
//
// THE ROWS (`classic_rows`), from the server's settings and the player's own max speed, in map
// units (32 to the metre):
//   - WALK: the player's box (32 wide, 56 tall, the eyes 46 over the feet, no crouch), the
//     ground's `accelerate`, `friction` and `stopspeed`, friction doubled where nothing is under
//     the origin within a step (`edgeprobe` 0), steps of 18, ground at 0.7 within 1 unit, off it
//     rising past 180; the air with the GROUND's factor and its wish held to 30 along it (strafe
//     jumping), `pm_bunnyspeedcap` as the air's plan-speed limit; gravity all before the move
//     (`gravitylead` 1); the jump 270, added to a rising speed and raised to 270 (`pm_ktjump` 1);
//     a landing's speed into the ground kept unless falling faster than 300.
//   - FLY: the walk row with friction 4 on the whole speed (no stop speed) and no gravity.
//   - SPECTATOR: through everything at `sv_spectatormaxspeed`, friction 1.5 times the ground's.
//   - SWIM: the kinematic regime at 0.7 of the max speed, `sv_wateraccelerate` and
//     `sv_waterfriction` by the level, the idle sink 60 at the swim's 0.7 (42 a second), the
//     waist 28 and the eyes 46 over the feet; the water jump onto a bank 310 up and 50 out for 2 s.
// What no row holds is this file's, as the game's rules over the mover: the 50 ms split of a
// command, swimming up on the jump button (100 in water, 80 in slime, 50 in lava), the swimmer's
// velocity carried through the hulls by the mover (`move::carry_along`, the step up included),
// a water jump that rides over the lip unslowed and ends when it falls or lands.
//
// NOT IN THE ROW (the classic move has them; the engine's mover does not): unsticking a body
// sunk in a wall, `pm_ktjump` below 1, `pm_slidefix`, `pm_airstep`, `pm_pground`, the old
// spectator's coast, the 0.1 snap of a clipped velocity's small parts, the edge probe's box (the
// row probes a point), floats (the mover runs in double). A dead player is the game's: his
// command reaches the mover with no stick and no jump.
//
// ge:summary The classic move's numbers as rows of the engine's mover and swimmer, one command
// ge:keywords classic-row engine-mover move-spec swim-spec classic_rows row_command row_body
// ge:keywords row_state gravity-lead land-clip-speed edge-friction swim water-jump spectator
// ge:keywords fly carry_along brush-world
// ge:skill shooter move water
// ge:test tests/shooter_row_test.cpp tests/shooter_row_recording_test.cpp
#pragma once

#include <vector>

#include "move/step.hpp"
#include "shooter/hull_world.hpp"
#include "shooter/pmove.hpp"
#include "water/swim.hpp"

namespace ge::shooter {

constexpr double kUnitsPerMetre = 32;

struct ClassicRows {
  move::MoveSpec walk, fly, spectator;
  water::SwimSpec swim;
};

// The rows for these settings and a player's own max speed and gravity factor.
ClassicRows classic_rows(const MoveSettings& set, float maxspeed = 320, float entgravity = 1);

// The world a row moves through: the models (filled by the caller), the mover's trace over them
// and the liquids of model 0 laid as water. Never copy one once built (the trace points at
// `hulls`).
struct RowWorld {
  HullWorld hulls;
  move::World mover;
  water::World water;
  std::vector<int> kinds;  // the contents each water body was laid for
};
// Points the mover at `world.hulls` and lays model 0's liquids.
void build_row_world(RowWorld& world);

// A player on the row: the engine's body (the feet), the swimmer, the classic's own labels.
struct RowBody {
  move::MoveState body;
  water::SwimState swim;
  MoveType type = MoveType::normal;
  float maxspeed = 320;
  int waterlevel = 0;
  int watertype = kContentsEmpty;
};

// A row body from a classic state (its origin 24 over the feet): on the ground when the state
// says so or a unit's probe finds it, as the classic's categorising does.
RowBody row_body(const MoveState& classic, const ClassicRows& rows, const RowWorld& world);
// The classic state a row body stands for: origin, velocity, the ground and its solid, the water.
void row_state(const RowBody& row, MoveState& classic);

// One command as a server runs it (split in halves past 50 ms) through the row.
void row_command(RowBody& row, const MoveCmd& cmd, const ClassicRows& rows, const RowWorld& world);

}  // namespace ge::shooter
