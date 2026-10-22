// FALL LINE'S BANK: every discrete sound a run makes, as data (`SoundDef`: an id, the sentence
// it should feel like, its voices). `route.hpp` names the ids; the `audition` lab meters each.
//
// SNOW SWALLOWS TRANSIENTS. That is the rule every landing is written to: a skier coming down is
// a brown thump that opens over a few milliseconds with a pink puff of snow over it, and the only
// sharp edges belong to the things that are not snow: a trunk cracking, a gate's panel slapped,
// two skiers shouldering, the finish horn and its cowbells, the skis clattering off after a fall.
// COLOUR BEFORE FILTER: brown is mass and distance, pink is snow in the air, white is grit and the
// crack of wood. And drive on anything with a body behind it, because a clean sine is a bell.
//
// ge:summary Fall Line's sound bank: the ids route.hpp plays and the bank itself
// ge:keywords bank sound-def ids landing tree gate missed finish wipeout reset bump
// ge:skill audio sim
// ge:test tests/fall_line_sound_test.cpp
#pragma once

#include <span>
#include <string_view>

#include "audio/sound.hpp"

namespace fall_line::sound {

inline constexpr std::string_view kLandSoft = "land_soft";
inline constexpr std::string_view kLandHard = "land_hard";
inline constexpr std::string_view kLandPowder = "land_powder";
inline constexpr std::string_view kHitTree = "hit_tree";
inline constexpr std::string_view kBrushTree = "brush_tree";
inline constexpr std::string_view kBump = "bump";
inline constexpr std::string_view kGateTaken = "gate";
inline constexpr std::string_view kGateMissed = "missed";
inline constexpr std::string_view kReset = "reset";
inline constexpr std::string_view kFinish = "finish";
inline constexpr std::string_view kWipeoutTree = "wipeout_tree";
inline constexpr std::string_view kWipeoutNose = "wipeout_nose";
inline constexpr std::string_view kWipeoutCatch = "wipeout_catch";

extern const std::span<const ge::audio::SoundDef> kBank;

}  // namespace fall_line::sound
