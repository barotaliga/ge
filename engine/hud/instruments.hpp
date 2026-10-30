// THE INSTRUMENTS: HUD widgets that are DRAWN rather than printed, each a pure function of the
// numbers it is handed (docs/hud.md § The instruments). None of them reads a game.
//
//   - a DIAL: a needle gauge (a speedometer, a tachometer). A range, an optional KNEE that
//     squeezes the values under it into the first share of the sweep (so the band a player spends
//     the run in gets most of the dial), an optional RED band to the top, ticks and figures on a
//     ring, a needle, and the reading in digits in the face's lower half;
//   - a WIND meter, heading-up: the top is the way the player faces. The bold arrow through the
//     middle is the air he FEELS (where he is, less his own velocity: at speed in still air it
//     points straight back at him); a mark on the rim stands on the side the WEATHER's wind blows
//     from and points inward, so the two never cross. The reading under it is the felt wind's
//     speed. When the weather's wind is past a gale the arrow turns the alarm colour (his own
//     speed is no alarm); under a calm floor no arrow is drawn;
//   - a COMPASS: a strip of headings, the current one centred under a notch, the cardinal points
//     lettered, and a game's own marks (an objective, a teammate) on it.
//
// ANGLES are the draw list's: radians, 0 at twelve o'clock, clockwise on the screen. HEADINGS are
// the engine's: 0 along +z (north), clockwise toward +x (east), so a world bearing turned
// heading-up is the bearing less the heading. Every sine and cosine is the engine's own libm, so a
// lab's digest is the same on every platform.
//
// THE NEEDLE is eased in the view, never in the simulation: `ease_needle` is a critically damped
// spring that settles in about `Dial::settle` seconds, stepped by the frame's clock, so a bumpy
// reading does not make it jitter and the same clock draws the same needle on every replay.
//
// ge:summary The HUD's instruments: the dial, the heading-up wind meter, the compass strip
// ge:keywords hud instruments dial gauge needle speedometer knee red-band ticks figures ease
// ge:keywords critically-damped wind felt-wind weather heading-up gale calm compass strip
// ge:keywords cardinal marks dial_angle ease_needle draw_dial draw_wind draw_compass
// ge:skill ui
// ge:test tests/hud_instruments_test.cpp
#pragma once

#include <span>
#include <string>
#include <string_view>

#include "hud/hud.hpp"

namespace ge::hud {

// A needle gauge's shape. Its values are the game's units (km/h, rpm); its sizes reference px.
struct Dial {
  double min = 0, max = 100;
  // The values min .. knee take the first `knee_share` of the sweep, knee .. max the rest; a knee
  // at or under min (or at or over max) leaves the dial linear.
  double knee = 0;
  double knee_share = 0.25;
  double red = 0;      // the red band runs from here to max; at or under min: none
  double major = 20;   // a long tick and a figure every this many (0: none)
  int minor = 1;       // short ticks between two long ones
  double figures = 1;  // a figure is the value over this (a tachometer's 1000s); 0: no figures
  float from = -2.2f, to = 2.2f;  // the sweep, rad: min at `from`, max at `to` (126 degrees)
  float size = 150;               // across, reference px
  double settle = 0.1;            // s, the needle's settling time; 0: none
};

// The angle a dial's needle stands at for `v` (clamped to the range).
double dial_angle(const Dial& d, double v);

// A needle's state between frames: where it is drawn, how fast it moves, and the clock it was
// last stepped at (negative: never, so the first step jumps to the target).
struct Needle {
  double at = 0, rate = 0;
  double t = -1;
};

// Steps `n` toward `target` to the frame's clock `t` (s) as a critically damped spring settling
// in about `settle` s; returns where it is drawn. A clock that went back, or a gap over half a
// second, jumps it (a seek, a pause); `settle` 0 jumps it every time.
double ease_needle(Needle& n, double target, double t, double settle);

// What a dial shows this frame.
struct DialReading {
  double needle = 0;            // the value the needle stands at (eased)
  std::string figure;           // the reading in digits ("87")
  std::string unit;             // under it ("KM/H")
  ui::Color color{1, 1, 1, 1};  // the needle's and the figure's
};

Size measure(const Frame& f, const Dial& d);
void draw(ui::DrawList& dl, const Frame& f, const Style& st, const Dial& d, const DialReading& r,
          ui::Rect at);

// A wind meter's shape.
struct WindGauge {
  float size = 120;    // across, reference px
  double full = 20;    // m/s: the arrow's full length (a slower air draws a shorter one)
  double gale = 17.2;  // m/s: the weather's wind past it (the felt one's without a weather) turns
                       // the arrow and the figure the alarm colour
  double calm = 0.5;   // m/s: under it no arrow is drawn
};

// What a wind meter shows this frame. Vectors are the world's (x east, z north), the way the air
// blows TO, m/s.
struct WindReading {
  double heading = 0;                   // rad, the way the player faces
  double felt_x = 0, felt_z = 0;        // the air he feels: the air where he is less his velocity
  bool weather = false;                 // whether the weather's mark is drawn
  double weather_x = 0, weather_z = 0;  // the weather's wind
  std::string figure;                   // the felt wind's speed in digits ("12")
  std::string unit;                     // under it ("M/S")
  ui::Color color{1, 1, 1, 1};
};

// A world vector's screen angle with `heading` up.
double heading_up(double heading, double x, double z);

Size measure(const Frame& f, const Style& st, const WindGauge& g);
void draw(ui::DrawList& dl, const Frame& f, const Style& st, const WindGauge& g,
          const WindReading& r, ui::Rect at);

// A compass strip's shape.
struct Compass {
  float w = 360, h = 44;    // reference px
  double span = 2.0943951;  // rad of headings across the strip (120 degrees)
  double step = 0.2617994;  // rad between ticks (15 degrees); the cardinal points are lettered
};

// One of a game's marks on the strip: an objective, a teammate, the way down.
struct CompassMark {
  double bearing = 0;  // rad, world
  ui::Color color{1, 1, 1, 1};
};

struct CompassReading {
  double heading = 0;  // rad
  std::span<const CompassMark> marks;
  ui::Color color{1, 1, 1, 1};
};

Size measure(const Frame& f, const Compass& c);
void draw(ui::DrawList& dl, const Frame& f, const Style& st, const Compass& c,
          const CompassReading& r, ui::Rect at);

}  // namespace ge::hud
