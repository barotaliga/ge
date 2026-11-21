// THE CLASSIC LIQUIDS' HARM: what lava and slime deal a player on the classic games' clock, as the
// engine's `water::Harm` rows (docs/liquids.md § Harm), so a game's rules and the engine's
// `step_harm` deal the same damage:
//   - LAVA: 10 at immersion level 1, 20 at 2, 30 at 3, every 0.2 s; in the protective suit every
//     1 s at the same amounts.
//   - SLIME: 4, 8 and 12 by the level, every 1 s; nothing in the suit.
//   - WATER: nothing (drowning is the air's clock, the game's own).
// A tick is dealt the step the body is in the liquid with its clock run out, and the clock is set
// again from that step; it keeps running out of the liquid, but going in starts it again
// (`restart`), so the step after going in deals a second tick at once and a dip deals early.
// The rows carry no heat (`contact` 0): the classic damage is the ticks alone. Every swim scale is
// 1, the classic feel being the same in every liquid but for the swim up (row.hpp).
//
// ge:summary Lava and slime's damage ticks on the classic clock as the engine's harm rows
// ge:keywords classic-harm lava slime suit tick interval damage burn dissolve liquid-harm
// ge:keywords classic_harm waterlevel watertype
// ge:skill shooter water
// ge:test tests/shooter_liquid_harm_test.cpp
#pragma once

#include "shooter/hull.hpp"
#include "water/liquid.hpp"

namespace ge::shooter {

// The harm the liquid of classic contents `contents` deals (none for water and anything else),
// with the protective suit on when `suit`.
inline water::Harm classic_harm(int contents, bool suit = false) {
  water::Harm h;
  if (contents == kContentsLava) {
    h.kind = water::HarmKind::heat;
    h.tick[0] = 10;
    h.tick[1] = 20;
    h.tick[2] = 30;
    h.interval = suit ? 1 : 0.2;
    h.restart = true;
  } else if (contents == kContentsSlime && !suit) {
    h.kind = water::HarmKind::chemical;
    h.tick[0] = 4;
    h.tick[1] = 8;
    h.tick[2] = 12;
    h.interval = 1;
    h.restart = true;
  }
  return h;
}

}  // namespace ge::shooter
