// THE CORNER READOUTS a player turns on for any game: the speed (`show_speed`: 1 the speed over
// the ground, 2 the whole speed, up and down too) and the clock (`scr_clock`: 1 the time of day,
// 2 the game's time), drawn in the top-right corner under the frame rate (frame_graph.hpp), each
// on its own plate in the style's caption face. The game reads the numbers (its units a second, its
// clock), the kit draws them.
//
// A GAME'S DEFAULTS: the settings it registers hold them (`add_readout_cvars` takes their values
// at the call as the cvars' defaults).
//
// ge:summary The speed and clock readouts in a corner (show_speed, scr_clock) for any game
// ge:keywords hud show_speed scr_clock speedometer speed-readout clock time-of-day game-time
// ge:keywords corner-readouts draw_corner_readouts
// ge:skill ui
// ge:test tests/hud_corner_readouts_test.cpp
#pragma once

#include <cstdint>
#include <string>

#include "console/cvar.hpp"
#include "hud/hud.hpp"
#include "ui/draw.hpp"

namespace ge::hud {

struct ReadoutSettings {
  int32_t speed = 0;  // show_speed: 0 off, 1 over the ground, 2 the whole speed
  int32_t clock = 0;  // scr_clock: 0 off, 1 the time of day, 2 the game's time
};

// Registers show_speed and scr_clock over `s` (it must outlive `cvars`), archived, their defaults
// the values `s` holds now.
void add_readout_cvars(console::Cvars& cvars, ReadoutSettings& s);

// What the game reads this frame.
struct Readings {
  double ground_speed = 0, speed = 0;  // the player's, in the game's units a second
  int32_t hour = 0, minute = 0;        // the time of day, 0..23 and 0..59
  double game_s = 0;                   // the game's (or the match's) time, seconds
};

// The readouts' words: "320" for a speed (rounded), "07:05" for the time of day, "12:03" (minutes
// and seconds; "1:02:03" past the hour) for the game's time.
std::string speed_words(double units_a_second);
std::string time_of_day_words(int32_t hour, int32_t minute);
std::string game_time_words(double seconds);

// Draws the readouts `s` turns on, stacked down the top-right corner from `top` px below the safe
// area's top (the frame rate's box bottom, or 0); returns the px of height taken.
float draw_corner_readouts(ui::DrawList& dl, const Frame& frame, const Style& style,
                           const ReadoutSettings& s, const Readings& r, float top = 0);

}  // namespace ge::hud
