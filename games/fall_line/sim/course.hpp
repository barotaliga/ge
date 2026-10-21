// THE COURSE AS FALL LINE RUNS IT: the gates in order, the finish, and the way back onto the
// piste.
//
// - A GATE is a line across the piste (`Checkpoint`: its centre, its heading, its width),
//   crossed by a move through it in its facing direction within half its width plus `kGrace`.
// - ONE GATE IS LIVE AT A TIME, the one owed. A racer who crosses its line outside the width has
//   MISSED it: he is not out, but no later gate counts until he takes it, and the reset stands
//   him above it. Gate 0 is the START GATE, the last the FINISH LINE, which ends his run at the
//   clock to the share of the step the line was crossed in.
// - THE RESET stands a racer at rest a few metres past the last gate he took, on the piste and
//   facing down it, or back on his slot of the grid before he has taken one. It is the racer's
//   (`Controls::reset`) and the run's: down `kDownReset` s after a wipeout, or `kStuckReset` s
//   going nowhere.
//
// ge:summary The course: gates crossed in order, a miss, the finish, the reset pose
// ge:keywords course gate gates checkpoint crossed miss missed finish reset reset-pose grace
// ge:keywords bearing crossed_line step_course reset_racer bearing_to_next
// ge:skill sim
// ge:test tests/fall_line_sim_test.cpp
#pragma once

#include <cstdint>

#include "fall_line/sim/state.hpp"
#include "mountain/mapgen/types.hpp"

namespace fall_line::sim {

// Either side of a gate's width a crossing still counts, m: at gate range "did I clip that?" is
// answered in the racer's favour.
inline constexpr double kGrace = 1;
// How far past the last gate taken the reset stands him, m.
inline constexpr double kResetPast = 4;
// The run's own resets: down this long after a wipeout, or going nowhere this long, s.
inline constexpr double kDownReset = 1.5;
inline constexpr double kStuckReset = 8;
// Under this speed a racer on his skis is going nowhere, m/s.
inline constexpr double kStuckSpeed = 0.5;

// Whether a plan move from (x0, z0) to (x1, z1) crossed the gate's line in its facing direction:
// the share of the move made before it (0..1) and the offset from its centre there (positive
// right). False when the move did not cross.
bool crossed_line(const ge::mountain::mapgen::Checkpoint& cp, double x0, double z0, double x1,
                  double z1, double& share, double& lateral);

// The course after racer `i` moved from (x0, z0) this step: a gate taken, a miss, the finish.
void step_course(GameState& state, int32_t i, double x0, double z0);

// Stand racer `i` back on the piste (the course's header says where).
void reset_racer(GameState& state, int32_t i);

// The plan heading from (x, z) to the gate a racer owes, rad.
double bearing_to_next(const ge::mountain::mapgen::Level& level, const Progress& p, double x,
                       double z);

}  // namespace fall_line::sim
