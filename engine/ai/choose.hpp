// CHOOSING: the two shapes of a bot's choice, each its own decision record (why.hpp) when a
// trace is on, so `ge why` reads them without a line of recording in the bot.
//
// - A UTILITY SELECTOR (`pick_best`): options 0..n-1, each scored by the caller (higher is
//   better); the best is taken, the first of equals. With a trace on, the decision is recorded
//   with every option's score (the options' names the caller's, string literals) and the chosen
//   one's margin over the next.
// - A FLAT STATE MACHINE (`enter`): a mode and the time spent in it; entering another records
//   the move (the mode left, how long it was held, the mode entered and what settled it) and
//   starts the clock again. A bot that switches on its mode keeps its states flat: every mode is
//   one case, and every way out of one is an `enter` that says why.
//
// The record never decides: with a trace on or off a bot chooses the same.
//
// ge:summary Choosing: a utility selector and a flat state machine, both decision records
// ge:keywords ai choose utility selector best option score state-machine fsm mode enter
// ge:keywords transition decision-record why margin
// ge:skill bots
// ge:test tests/ai_steer_test.cpp
#pragma once

#include <cstdint>

#include "ai/why.hpp"

namespace ge::ai {

// THE BEST OF OPTIONS 0..n-1 by `score(i)` (-1 with none): recorded on `why` (when not null) as
// decision `decision`, option i named `name(i)`, the choice settled by `because`.
template <class Score, class Name>
int32_t pick_best(int32_t n, const Score& score, Why* why, const char* decision, const Name& name,
                  const char* because) {
  if (why) why->decide(decision);
  int32_t best = -1;
  double top = 0;
  for (int32_t i = 0; i < n; i++) {
    const double v = score(i);
    if (why) why->option(name(i), v);
    if (best < 0 || v > top) {
      best = i;
      top = v;
    }
  }
  if (why) why->choose(best < 0 ? "none" : name(best), because);
  return best;
}

// ENTER mode `next` from `mode`, held `held` s: recorded on `why` (when not null) as a "mode"
// decision (the mode left, the time it was held, the mode entered by `name`), `held` back to 0.
template <class Mode, class Name>
void enter(Mode& mode, double& held, Mode next, Why* why, const Name& name, const char* because) {
  if (why) {
    why->decide("mode");
    why->fact("from", static_cast<double>(mode));
    why->fact("held", held);
    why->choose(name(next), because);
  }
  mode = next;
  held = 0;
}

}  // namespace ge::ai
