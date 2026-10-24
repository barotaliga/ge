// THE DIRECTOR: a run's marked moments cut into a highlight reel (docs/replays.md § The director).
// A live camera cannot cut to the lens beside a kicker BEFORE the jump; a replay's can, because
// the run has already happened. The game files its moments as marks (file.hpp), back-dated where
// it only knows them later (a flight is filed at its take-off, known at its landing); the director
// turns them into a CUT: the shots, each a stretch of the run round one mark, its lens and its
// time rate.
//
// THE GAME'S HALF is a `ShotRule` a kind of mark: how far before the mark the shot opens (`lead_s`:
// a flight opens on the run-in, before the lip) and how long after it holds (`hold_s`), the time
// rate (0.5: half speed; slow motion is fewer steps a frame and nothing else), what a mark of the
// kind is worth beside the others (`scale` times its weight) and the lens the game's renderer
// draws it through (a name only the game reads). A mark of a kind with no rule is never cut to.
//
// THE ENGINE'S HALF is the choice, a pure function of the marks, the rules and the policy:
//   - every mark with a rule is a candidate, scored `weight * scale`;
//   - candidates are taken best first (ties: the earlier mark), each accepted when it leaves at
//     least `spacing_s` of run between it and every shot already taken, and the reel's run
//     seconds stay within `most_s` (0: no limit) and its shots within `most_shots` (0: no limit);
//   - a candidate whose mark falls inside a shot already taken is COVERED (it is in the reel),
//     one refused for spacing or length is DROPPED; both are listed, so a lab can say why;
//   - the shots are then put in run order.
// The same marks always make the same cut, so a reel replays like everything else.
//
// THE REEL CLOCK: a reel plays its shots one after another, each at its rate; `reel_at` says, for
// the seconds the reel has played, which shot is up and the run step it shows (fractional, for
// drawing between steps). Between two shots is a CUT: the player seeks (player.hpp), so the
// first frame of a shot may hold for the steps its keyframe is behind.
//
// ge:summary The director: a run's marks ranked and spaced into a cut of shots; the reel's clock
// ge:keywords replay director cut shot shots highlight reel rank spacing lens rate slow-motion
// ge:keywords lead hold covered dropped plan_cut reel_at reel_seconds shot-rule moment
// ge:skill replay
// ge:test tests/replay_director_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "replay/file.hpp"

namespace ge::replay {

// How the game shoots one kind of mark.
struct ShotRule {
  std::string kind;
  double lead_s = 2;  // before the mark
  double hold_s = 2;  // after it
  double rate = 1;    // the time rate: 0.5 half speed; must be above 0
  double scale = 1;   // a mark's score is its weight times this
  std::string lens;   // the game's name for the camera the shot is drawn through
};

struct DirectorPolicy {
  double spacing_s = 2;    // the least run between two shots
  double most_s = 0;       // the reel's run seconds at most (0: no limit)
  int32_t most_shots = 0;  // shots at most (0: no limit)
};

struct Shot {
  int64_t from = 0, to = 0;  // the run's steps shown, from <= to
  size_t mark = 0;           // the mark it is cut round (an index into the marks)
  std::string kind, lens;
  double rate = 1;
  double score = 0;
};

struct Cut {
  std::vector<Shot> shots;      // in run order
  std::vector<size_t> covered;  // marks inside a shot cut round another, by index
  std::vector<size_t> dropped;  // marks refused for spacing, length or count, by index
  std::vector<size_t> unruled;  // marks of a kind with no rule, by index
};

// Why `rules` cannot direct ("" when they can): a rate not above 0, a negative lead or hold, a
// kind named twice or not `mark_kind_ok`.
std::string rules_problem(std::span<const ShotRule> rules);

// The cut of `marks` (any order) on a run of `length` steps at `hz` steps a second.
Cut plan_cut(std::span<const Mark> marks, std::span<const ShotRule> rules,
             const DirectorPolicy& policy, double hz, int64_t length);

// The reel's length in seconds of watching: each shot's run seconds over its rate.
double reel_seconds(const Cut& cut, double hz);

struct ReelAt {
  size_t shot = 0;    // the shot up (the last one once the reel is over)
  double step = 0;    // the run step it shows, fractional
  bool over = false;  // the reel has played to its end
};

// Where the reel stands `seconds` into it (held at 0 and at its end). A cut with no shots is over
// at once, at step 0.
ReelAt reel_at(const Cut& cut, double hz, double seconds);

}  // namespace ge::replay
