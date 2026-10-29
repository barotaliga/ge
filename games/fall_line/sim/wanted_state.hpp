// THE WANTED LEVEL AS A RUN HOLDS IT (`GameState::wanted`): the stars, the heat, seen or
// searching, the survival timer, how the hunt ended, and the pursuers out. Its own header so
// state.hpp can carry it without the rules that move it (wanted.hpp).
//
// A run that never raises the heat leaves it inactive, hashes none of it and moves no digest.
//
// ge:summary The wanted level as a run holds it: WantedState, Pursuer, WantedEnd, WantedChange
// ge:keywords wanted wanted-state pursuer stars heat seen searching survival-timer arrest
// ge:keywords wanted-end wanted-change last-seen
// ge:skill sim
// ge:test tests/fall_line_wanted_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "ai/memory.hpp"
#include "fall_line/sim/wanted_rules.hpp"

namespace fall_line::sim {

// ONE PURSUER as the sight test sees him: what he is and where his eyes are, world frame, m. The
// parts that move the army write one a step for every man or machine out; `id` is theirs.
struct Pursuer {
  int32_t id = 0;
  PursuerKind kind = PursuerKind::helicopter;
  double x = 0, y = 0, z = 0;
  template <class Io>
  friend void describe(Io& io, Pursuer& v) {
    io(v.id);
    io(v.kind);
    io(v.x);
    io(v.y);
    io(v.z);
  }
};

// How a hunt ended: still running, the stars lost (`escaped`), taken in where the army does not
// shoot to kill (`arrested`), or dead of his wounds where it does (`killed`).
enum class WantedEnd : uint8_t { running, escaped, arrested, killed };

// What a wanted event says (`EventKind::wanted`): a star gained or set down by hand, the stars
// dropped after a search ran out, a pursuer catching sight of him again, losing it, the arrest, or
// his death.
enum class WantedChange : uint8_t { raised, lowered, lost, seen, searching, arrested, killed };

struct WantedState {
  bool active = false;  // raised at least once this run: only then is any of it hashed
  int32_t stars = 0;    // 0..5 (0 once lost)
  double heat = 0;      // star units: stars <= heat < stars + 1 while hunted
  bool seen = false;    // a pursuer has him in sight or the fix holds: the stars solid
  double unseen = 0;    // s out of every pursuer's sight, this search
  double fix = 0;       // s left of the fix the last offence gave them
  ge::ai::Memory last;  // where and when he was last seen: the search's centre
  double look = 0;      // s to the next look
  // Whether the army shoots to kill; false (the injuries switch off) and they chase him down and
  // arrest him instead. Kept across hunts in one run.
  bool lethal = true;
  // THE SURVIVAL TIMER: the run time this hunt began and, once it ended, stopped.
  double since = 0;
  double stopped = 0;
  WantedEnd ended = WantedEnd::running;
  int32_t peak = 0;  // the most stars this hunt reached
  // The run time each star was reached at (index = stars; -1 not yet): the clock each star's
  // ground waves count from.
  std::array<double, wanted::kMaxStars + 1> reached{};
  WantedCause cause = WantedCause::dev;  // the last offence that raised the heat
  // THE PURSUERS OUT, written by the parts that move them; reserved for `wanted::kMaxPursuers`
  // at create, so filling it never allocates in a step.
  std::vector<Pursuer> pursuers;
  template <class Io>
  friend void describe(Io& io, WantedState& v) {
    io(v.active);
    io(v.stars);
    io(v.heat);
    io(v.seen);
    io(v.unseen);
    io(v.fix);
    io(v.last);
    io(v.look);
    io(v.lethal);
    io(v.since);
    io(v.stopped);
    io(v.ended);
    io(v.peak);
    io(v.reached);
    io(v.cause);
    io(v.pursuers);
  }
};

}  // namespace fall_line::sim
