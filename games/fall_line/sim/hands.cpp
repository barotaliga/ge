// The hand profiles and BotHands::press (hands.hpp).
//
// ge:summary The none and human hand profiles' numbers, hands_seed and BotHands::press
// ge:keywords hands profile human none seed press intent
// ge:skill bots sim
// ge:test tests/fall_line_hands_test.cpp
#include "fall_line/sim/hands.hpp"

#include <array>

#include "core/num.hpp"
#include "fall_line/sim/state.hpp"

namespace fall_line::sim {

namespace {

namespace ai = ge::ai;

// The axes the hands hold, in this order: what press hands them and record_intent names.
constexpr std::array<const char*, 3> kAxisNames{"steer", "tuck", "brake"};

// A person on a gamepad (ai/human.hpp's bands state why each number is about right).
constexpr std::array<ai::AxisHands, 3> kHuman{{
    // steer: a thumb on a stick. Reacts in 0.2 s, settles a move in 0.15 s with a little
    // overshoot, crosses the travel in no less than a quarter second, wanders a little.
    {false, 0.20, 0.04, 0.7, 0.15, 0.7, 8, -1, 1, 0},
    // tuck: a trigger. Slower to fold in than to sit up; squeezed short of flat out at times.
    {false, 0.25, 0.04, 0.5, 0.20, 1.0, 5, 0, 1, 0},
    // brake: a trigger, quicker: the skid is what saves a line.
    {false, 0.20, 0.04, 0.5, 0.12, 0.9, 6, 0, 1, 0},
}};

}  // namespace

std::optional<HandsProfile> hands_profile(std::string_view name) {
  if (name == "none") return HandsProfile::none;
  if (name == "human") return HandsProfile::human;
  return std::nullopt;
}

const char* hands_name(HandsProfile p) { return p == HandsProfile::human ? "human" : "none"; }

uint32_t hands_seed(double seed, int32_t i) {
  const auto s = static_cast<uint32_t>(ge::core::num::to_uint32(seed));
  return (s * 0x9e3779b9u) ^ (static_cast<uint32_t>(i) * 0x85ebca6bu) ^ 0x68616e64u;
}

BotHands::BotHands(HandsProfile profile, uint32_t seed) : profile_(profile) {
  if (profile == HandsProfile::human) hands_ = ai::Hands(kHuman, kHz, seed);
}

double BotHands::lead() const {
  if (profile_ == HandsProfile::none) return 0;
  return kHuman[0].delay_s + kHuman[0].settle_s / 3;
}

Controls BotHands::press(const Controls& wanted, ai::Why* why) {
  const std::array<double, 3> want{wanted.ski.steer, wanted.ski.tuck, wanted.ski.brake};
  ai::record_intent(why, kAxisNames, want);
  if (profile_ == HandsProfile::none) return wanted;
  std::array<double, 3> out{};
  hands_.step(want, out);
  Controls c = wanted;
  c.ski.steer = out[0];
  c.ski.tuck = out[1];
  c.ski.brake = out[2];
  return c;
}

}  // namespace fall_line::sim
