// THE SKIER BOT'S HANDS: Fall Line's hand profiles over the engine's `ai::Hands` (ai/hands.hpp),
// for the pilot and every rival. A profile is a name a run is set up with (`--hands=`, the sim's
// `SimOptions::hands`, the play contract's `hands` option):
//
// - `none`: the bot's controls as it decides them, every step. The default: every digest a run
//   held before the hands existed is unchanged.
// - `human`: what a person on a gamepad does, by the stated assumptions of ai/human.hpp's bands:
//   about 0.2 s to react, a thumb that crosses the stick's travel in about 0.15 s and overshoots
//   a quick move a little, the triggers squeezed a little short of flat out, a slow wander off
//   what was meant. Each racer's hands wander off their own stream (`hands_seed`).
//
// `BotHands::press` turns what the bot wants into what it presses, and records the intent (a
// decision called "hands", one fact an axis: `steer`, `tuck`, `brake`) when it is traced, so the
// humanness metrics measure the delay from it. The jump and the reset pass through untouched.
//
// ge:summary Fall Line's hand profiles (none, human) over ai::Hands, for the pilot and the rivals
// ge:keywords hands profile human none bot rivals pilot reaction slew wander intent humanness
// ge:skill bots sim
// ge:test tests/fall_line_hands_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "ai/hands.hpp"
#include "ai/why.hpp"
#include "fall_line/sim/state.hpp"

namespace fall_line::sim {

enum class HandsProfile : uint8_t { none, human };

// The profile called `name`, or nothing.
std::optional<HandsProfile> hands_profile(std::string_view name);
const char* hands_name(HandsProfile p);

// Racer `i`'s hands' own stream on the map built for `seed`.
uint32_t hands_seed(double seed, int32_t i);

class BotHands {
 public:
  BotHands() = default;
  BotHands(HandsProfile profile, uint32_t seed);
  HandsProfile profile() const { return profile_; }
  // How late these hands press what the bot decides, about: the steer's delay plus a third of its
  // settling. The bot steers from where it will be then (bot.hpp's `lead_s`).
  double lead() const;
  // What the bot presses for what it `wanted`, the intent recorded on `why` (null: not traced).
  Controls press(const Controls& wanted, ge::ai::Why* why);

 private:
  HandsProfile profile_ = HandsProfile::none;
  ge::ai::Hands hands_;
  template <class Io>
  friend void describe(Io& io, BotHands& v) {
    io(v.profile_);
    io(v.hands_);
  }
};

}  // namespace fall_line::sim
