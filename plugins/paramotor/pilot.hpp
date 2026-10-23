// THE BOT'S HANDS ON THE PARAMOTOR: the four controls a pilot has and nothing more. What the
// scenarios stage their flights with, what a game's bot flies the wing on, and what proves the
// bare physics can be flown at all.
//
// - OFF THE SNOW, before its first flight: straight down the fall line on full throttle (the
//   toggles half toward the aim) until the wing lifts him off.
// - IN THE AIR: the toggles turn the wing's heading toward the aim (or down the fall line with
//   none); the throttle holds `height` m over the highest snow under him and `pilot::kLook` m
//   ahead of him; a fold is pumped out on the brakes, twice a second, the other toggle holding
//   the line against the folded side.
// - LANDING (`ParaAim::land`): within the glide of the aim the throttle is cut and he glides down
//   to it; under `pilot::kFlare` m the brakes come on in a flare and he touches down on his skis.
// - LANDED: the throttle cut and the brakes on; the caller drops the rig.
//
// ge:summary The bot's hands on the paramotor: launch, fly to an aim at a height, glide and flare
// ge:keywords paramotor para pilot bot para_pilot aim launch hold-height terrain-following
// ge:keywords glide flare landing pump fold
// ge:skill paramotor
// ge:test tests/paramotor_scenarios_test.cpp
#pragma once

#include "paramotor/para.hpp"

namespace ge::paramotor {

namespace pilot {
// The height it holds over the snow, m (an aim's default), and how much throttle a metre short of
// it asks over the cruise's `kCruise`.
inline constexpr double kHold = 60;
inline constexpr double kCruise = 0.45;
inline constexpr double kPerMetre = 1.0 / 40;
// ...and a metre a second of it closing (his sink less the snow's fall under his way).
inline constexpr double kPerSink = 0.1;
// How far ahead of him the snow is read for the height, m, and every how far.
inline constexpr double kLook = 150;
inline constexpr double kLookStep = 25;
// How hard a radian off the heading it wants pulls a toggle.
inline constexpr double kTurn = 1.2;
// How deep a fold it pumps out, share of the wing, and how hard.
inline constexpr double kPump = 0.1;
inline constexpr double kPumpBrake = 0.7;
// LANDING: the glide it plans the descent on (out of still air), the margin, m, and the height
// over the snow it flares from, m.
inline constexpr double kGlide = 3.5;
inline constexpr double kMargin = 30;
inline constexpr double kFlare = 5;
}  // namespace pilot

// Where the bot flies: toward (x, z) when `has_aim` (else down the fall line), `height` m over the
// snow, and with `land` down onto the snow there.
struct ParaAim {
  bool has_aim = false;
  double x = 0;
  double z = 0;
  double height = pilot::kHold;
  bool land = false;
};

// The bot's controls this step, flying `p` (on skier `s`) to `aim`.
ParaControls para_pilot(const ParaState& p, const skier::SkierState& s, const ParaWorld& world,
                        const ParaAim& aim);

}  // namespace ge::paramotor
