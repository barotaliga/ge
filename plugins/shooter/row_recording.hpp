// THE CLASSIC ROW AGAINST THE RECORDINGS: every recorded scenario (recording.hpp) run through the
// engine's mover on the classic rows (row.hpp) over the same scene, the scene's brushes built
// into a brush model (brush_hull.hpp) and each recorded box into one more, measured line by line.
//
// TWO MEASURES, each line's largest component's difference in position and velocity:
//   - THE STEP (`RowReplay::step`): each command run from the RECORDED state before it: how far
//     one command of the row lands from the classic's, with no drift carried in.
//   - THE RUN (`RowReplay::run`): the whole script from the start with the row's own state
//     carried: how far a player on the row ends up from the classic's.
// THE TOLERANCE is the recordings' own (recording.hpp: 0.01 unit, 0.05 unit/s) on both measures,
// with on-ground, the ground's solid, the water level and the water type equal: the row runs
// another model on the same numbers (double where the classic is float, a point's edge probe,
// no snap of small clipped parts) and lands within what a faithful float move is allowed. The
// jump flag, the slide's bits and the touches are the classic move's own and are not compared.
// COVERED (`row_covers`): every scenario whose move type and settings are the row's. Not covered,
// for the reasons row.hpp lists: the old spectator, unsticking (`nudge_*`), `pm_ktjump` 0,
// `pm_slidefix`, `pm_airstep` with `pm_pground`. They are run and reported all the same.
//
// ge:summary The classic row on the engine's mover measured against the recorded classic moves
// ge:keywords row-recording replay_row row_covers step run tolerance parity engine-mover
// ge:keywords classic-row vectors
// ge:skill shooter move testing
// ge:test tests/shooter_row_recording_test.cpp
#pragma once

#include <string>

#include "shooter/recording.hpp"

namespace ge::shooter {

// Whether the row is meant to carry this scenario (its type and settings are the row's).
bool row_covers(const RecordedScenario& scenario);

struct RowReplay {
  ReplayResult step;      // each command from the recorded state before it
  ReplayResult run;       // the whole script from the start
  int step_discrete = 0;  // step lines whose on-ground, ground or water level differ
  int run_discrete = 0;
};

// The scenario through the row, both measures; a line is `ok` within the tolerance with the
// compared discrete fields equal.
RowReplay replay_row(const Recording& rec, const RecordedScenario& scenario);

}  // namespace ge::shooter
