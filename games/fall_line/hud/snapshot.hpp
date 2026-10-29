// WHAT FALL LINE'S HUD READS: the run's state reduced to the figures drawn over the free ride,
// a pure function of the state and of what the HUD remembers across the run (`HudRun`: the
// best air, the news column). The view (view.hpp) draws a snapshot and nothing else, so the
// tests and the labs check what the HUD shows without a picture, and a recorded run's HUD is the
// same on every replay.
//
// THE FREE RIDE'S FIGURES, as the player reads them:
//   - the SPEED (km/h, |v| as the skier state keeps it), the EDGE he stands on (-1 full left
//     .. 1 full right, of the pair's most) and the TUCK;
//   - the HEADING he faces and the WIND: the air he feels (the mountain's air at his body, less
//     his own velocity) and the weather's wind over open ground, for the wind meter;
//   - the AIR CLOCK while a flight lasts past the skier's least, and whether it beats the run's
//     best so far;
//   - the map's SEED and where on it he stands, the run under his skis, the metres skied, the
//     altitude: the numbers that say which mountain and where;
//   - the WANTED LEVEL while the army hunts him: the stars, flashing while they search where he
//     was last seen, the share of that search left, hidden under the trees, the survival timer;
//   - the MACHINE he is on (a lift, the snowmobile, the helicopter, the paramotor) and its own
//     reading;
//   - the PROMPT that tells him what is happening to him: down, stuck, waiting for a chair, dead,
//     arrested.
// Race and trick figures (the clock, the gates, the place, the splits, the score) are not here:
// races and tricks are not yet a player's to run.
//
// ge:summary Fall Line's HUD snapshot: the free ride's figures, the HUD's memory and its news
// ge:keywords hud snapshot speed km-h edge tuck air-clock best-air seed position run skied
// ge:keywords heading felt-wind weather-wind altitude wanted stars flashing search survived
// ge:keywords machine lift sled heli para prompt down stuck died arrested hud-run note_step news
// ge:skill ui sim
// ge:test tests/fall_line_hud_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "fall_line/sim/state.hpp"
#include "hud/feed.hpp"

namespace fall_line::hud {

// The air clock shows a flight once it has lasted this long, s (a hop over a roller is no air).
inline constexpr double kAirShown = 0.3;
// "Stuck" shows once he has gone nowhere this long, s (the run resets him at `kStuckReset`).
inline constexpr double kStuckShown = 2;

enum class Machine : uint8_t { skis, lift, sled, heli, para };
enum class Prompt : uint8_t { none, down, stuck, waiting, died, arrested };

struct WantedTile {
  int32_t stars = 0;
  int32_t of = 5;
  bool flashing = false;  // they search where he was last seen: the lit stars blink
  double search = -1;     // while they search: the share of it left (1 .. 0); -1 while seen
  bool hidden = false;    // still on foot under a crown: hidden from the air
  double survived = 0;    // s this hunt has run (stopped once it ended)
  sim::WantedEnd ended = sim::WantedEnd::running;
};

struct HudSnapshot {
  double t = 0;
  double seed = 0;
  double x = 0, z = 0;
  double speed_kmh = 0;
  double edge = 0;                      // -1 .. 1 of the pair's most
  double tuck = 0;                      // 0 .. 1
  double heading = 0;                   // rad, the way his skis point
  double felt_x = 0, felt_z = 0;        // m/s, the air he feels (world, the way it blows to)
  double weather_x = 0, weather_z = 0;  // m/s, the weather's wind at 10 m over open ground
  double altitude_m = 0;
  double air_s = 0;  // 0 unless a flight shows
  bool air_best = false;
  double best_air_s = 0;
  double skied_m = 0;
  std::string run;  // the run under his skis as read ("B3 RED"), or empty off every run
  Machine machine = Machine::skis;
  double machine_kmh = 0;     // the machine's own speed (the snowmobile's, the helicopter's)
  double machine_height = 0;  // the helicopter's or the paramotor's height over the snow, m
  Prompt prompt = Prompt::none;
  std::optional<WantedTile> wanted;
};

// What the HUD remembers across a run's steps.
struct HudRun {
  double best_air = 0;  // s, the run's longest flight landed
  double flight = 0;    // s, the flight under way (0 on the snow)
  ge::hud::Feed news{4, 3.5, 0.8, 0.2};
};

// The colours the news is written in (the game's HUD style's signals).
struct NewsInk {
  ge::ui::Color plain{1, 1, 1, 1};
  ge::ui::Color good{0.31f, 0.86f, 0.49f, 1};
  ge::ui::Color bad{1, 0.35f, 0.31f, 1};
};

// After each step of the run: its events written into the news, the best air kept. Reads the
// player's (racer 0's) events only.
void note_step(HudRun& run, const sim::GameState& state, const NewsInk& ink);

HudSnapshot snapshot(const sim::GameState& state, const HudRun& run);

}  // namespace fall_line::hud
