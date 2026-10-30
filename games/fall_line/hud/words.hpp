// EVERY WORD FALL LINE'S HUD SHOWS: the captions, the units, the prompts and the news lines, as
// constants and the few functions that put a number into a line. Nothing in the HUD's view writes
// a literal; a word changed here changes on every screen, and a translation is this file again.
//
// The HUD draws in a bitmap charset, so every word is ASCII and SHORT: a caption fits under its
// value on a phone held upright (the `hud` lab's layout check fails one that does not).
//
// ge:summary Fall Line's HUD words: captions, units, prompts and the news lines
// ge:keywords hud words strings text captions units prompts news labels km-h translation
// ge:skill ui
// ge:test tests/fall_line_hud_test.cpp
#pragma once

#include <cstdint>
#include <string>

#include "fall_line/sim/state.hpp"
#include "mountain/mapgen/types.hpp"

namespace fall_line::hud::words {

inline constexpr const char* kSpeedUnit = "KM/H";
inline constexpr const char* kWindUnit = "M/S";
inline constexpr const char* kEdge = "EDGE";
inline constexpr const char* kTuck = "TUCK";
inline constexpr const char* kSeed = "SEED";
inline constexpr const char* kSkied = "SKIED";
inline constexpr const char* kAir = "AIR";
inline constexpr const char* kBestAir = "BEST AIR";
inline constexpr const char* kAltitude = "ALTITUDE";
inline constexpr const char* kWanted = "WANTED";
inline constexpr const char* kSearching = "OUT OF SIGHT";
inline constexpr const char* kHidden = "HIDDEN";
inline constexpr const char* kSurvived = "SURVIVED";
inline constexpr const char* kOffRun = "OFF PISTE";

inline constexpr const char* kDown = "DOWN";
inline constexpr const char* kDownHow = "UP IN A MOMENT";
inline constexpr const char* kStuck = "STUCK";
inline constexpr const char* kStuckHow = "POLE OUT OR RESET";
inline constexpr const char* kWaiting = "LIFT";
inline constexpr const char* kWaitingHow = "WAIT FOR THE NEXT CHAIR";
inline constexpr const char* kRiding = "RIDING THE LIFT";
inline constexpr const char* kDied = "YOU DIED";
inline constexpr const char* kArrested = "ARRESTED";
inline constexpr const char* kEscaped = "LOST THEM";

inline constexpr const char* kOnSled = "SNOWMOBILE";
inline constexpr const char* kOnHeli = "HELICOPTER";
inline constexpr const char* kOnPara = "PARAMOTOR";
inline constexpr const char* kHeight = "HEIGHT";

// "38 @ 412,-1290": the map's seed and where on it he stands, m.
std::string seed_at(double seed, double x, double z);
// "2.4 KM" past a kilometre, "640 M" under it.
std::string distance(double m);
// "1.84 S": an air clock or a flight's length.
std::string seconds(double s);
// "1:07" (m:ss) for the survival timer.
std::string clock(double s);
// "2140 M" above the sea.
std::string altitude(double m);
// A run's name as the HUD reads it: its id, with its grade's word ("B3 RED").
std::string run_name(const ge::mountain::mapgen::Run& run);

// THE NEWS LINES.
std::string landed(double air_s, double g);
std::string wipeout(ge::skier::Wipeout cause);
inline constexpr const char* kTree = "BRUSHED A TREE";
std::string onto(const ge::mountain::mapgen::Run& run);
std::string arrived(const ge::mountain::mapgen::Run& run);
std::string lift_top(double ride_s);
std::string wanted(sim::WantedChange change, int32_t stars);

}  // namespace fall_line::hud::words
