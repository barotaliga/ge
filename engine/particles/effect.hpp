// PARTICLE EFFECTS AS DATA (`ge::particles`): an effect is a few lines of text a game or a mod
// ships, named, so what a hit, a blast or a flight throws is changed by a file instead of code.
// A game keeps a `Book` of the effects its files name; where it would throw one of its own, it
// looks the name up and, when the book has it, throws the book's instead (Shooter's Edge:
// client/world_fx.cpp, the files `particles/*.fx`). Presentation only: nothing here feeds the
// simulation or a digest of it, but every throw is a pure function of the place and a seed
// (core::Rng), so a replay throws the same specks.
//
// THE TEXT, a line a statement, `#` to the end of a line a comment, words split by blanks:
//
//   effect <name>               begins an effect (lower-case letters, digits, `_`, `.`, `-`); a
//                               later effect of the same name replaces it (a mod's over the game's)
//     light <radius> <r> <g> <b> [<fade_s>]
//                               a light where it is thrown (1 1 1 white; how strong a light that
//                               far reaching is, is the game's rule), shrinking to nothing over
//                               fade_s (0.5)
//     burst <count>             begins an emitter: <count> specks thrown at once
//     trail <spacing>           or one laid along a flight: a speck every <spacing>
//       box <half>              where each starts: within a cube of this half-size round the point
//       ball <radius>           or within a ball (neither: at the point)
//       speed <min> <max>       a way out picked at random, this fast
//       along <min> <max>       along the effect's way (a hit's face, a trail's flight), this fast
//       rise <min> <max>        up, this fast
//       life <min> <max>        seconds
//       size <min> <max>        across
//       fall <a>                falls at <a> game units a second squared (below 0: rises; not
//                               given: under the engine's gravity, 9.81 m/s²)
//       drag <per_s>            slowed toward the still air
//       colour <r> <g> <b> [<r> <g> <b>]
//                               linear colour, or a pick between two
//       palette <first> [<last>]
//                               an entry of the game's palette (`Throw::palette`), or a pick in a
//                               run
//       shine <strength>        an unlit speck of its colour at this light (the default, 1)
//       smoke <albedo> <opacity>
//                               a lit puff instead, its colour the tint of its whiteness
//       cool <s> [<grey>]       its colour turning toward a grey (0.5) by e each <s>
//       grow <per_s>            across it gains a second
//       streak <s>              its motion over this long drawn as a streak
//       bounce <share>          off the floor the game gives (`Throw::floor_y`), at this share
//
// LENGTHS ARE IN THE GAME'S OWN UNIT (a map unit, a metre): `Throw::metres` says how long one is,
// and every length, speed and size is scaled by it when thrown. Up is +y, as the engine's world.
//
// A FAULT names the file and line, what was wrong and how to write it, and the whole text is
// refused: a book is never half read.
//
// ge:summary Particle effects as data: the effect text, the book, a burst or a trail thrown
// ge:keywords particles particle-effects effect-script data-driven-particles burst trail emitter
// ge:keywords book parse_effects throw_effect throw_trail spark glow mod-effects fx-file
// ge:skill render
// ge:test tests/particles_effect_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ge::particles {

// Where an emitter's specks start.
enum class Shape : uint8_t { point, box, ball };

struct Range {
  double lo = 0, hi = 0;
};

struct Emitter {
  bool trail = false;  // laid along a flight (`spacing` apart) rather than thrown at once
  int count = 0;       // a burst's specks
  double spacing = 0;  // a trail's gap, game units
  Shape shape = Shape::point;
  double extent = 0;  // the box's half-size or the ball's radius, game units
  Range speed, along, rise;
  Range life{0.5, 0.5}, size{1, 1};
  bool falls_set = false;                       // `fall` given (else the engine's gravity)
  double fall = 0, drag = 0;                    // game units/s², 1/s
  float colour[2][3] = {{1, 1, 1}, {1, 1, 1}};  // linear; a pick between the two
  int palette[2] = {-1, -1};                    // a run of palette entries (-1: the colour)
  bool smoke = false;                           // a lit puff, not a shining speck
  double shine = 1;                             // a speck's light
  double albedo = 0.5, opacity = 1;             // a puff's
  double cool = 0, grow = 0, streak = 0, bounce = 0;
};

struct Effect {
  std::string name;
  std::vector<Emitter> emitters;
  double light_radius = 0;  // game units (0: no light)
  float light_rgb[3] = {1, 1, 1};
  double light_fade = 0.5;  // s
  int line = 0;             // where it began in its file
  std::string file;
};

// Every effect read, by name; the later of two of one name kept.
struct Book {
  std::vector<Effect> effects;
  const Effect* find(std::string_view name) const;
};

// `text` (the file `file`, for the faults' wording) read into `book`, each effect replacing one
// of its name. False, `book` unchanged, with the fault and the fix in `error`.
bool parse_effects(std::string_view text, std::string_view file, Book& book, std::string& error);

// A speck as thrown: the render pool's particle's fields (render/particles.hpp's `Particle`; a
// renderer copies them across, render/spark.hpp), engine metres and seconds.
struct Spark {
  float p[3];
  float v[3];
  float life = 1, size = 0.05f;
  float drag = 0, fall = 1, streak = 0;
  float albedo = 0.5f, opacity = 1;
  float kelvin = 0;  // below 0 a shining speck at -kelvin's light (Particle::kelvin)
  float cool_s = 0;
  float tint[3] = {1, 1, 1};
  float grow = 0;
  float floor_y = -1e9f, bounce = 0;
};
// An effect's light: where, how far it reaches, its colour, and how long it shrinks for.
struct Glow {
  double at[3];
  double radius_m = 0;
  float rgb[3] = {1, 1, 1};
  double fade_s = 0.5;
};

// Where and how an effect is thrown.
struct Throw {
  double at[3] = {0, 0, 0};   // engine metres
  double way[3] = {0, 1, 0};  // unit: a hit's face, a flight's way (`along`)
  double metres = 1;          // one game unit, m
  double floor_y = -1e9;      // where `bounce` meets the floor, m
  int times = 1;              // a burst's count multiplied (a shot's pellets)
  uint32_t seed = 1;
  std::span<const uint8_t> palette;  // 256 RGB triples (sRGB); empty: entry n a grey of n
};

// The effect's bursts at `t.at` into `out`, its light (when it has one) into `glows`.
void throw_effect(const Effect& e, const Throw& t, std::vector<Spark>& out,
                  std::vector<Glow>* glows = nullptr);
// The effect's trails along `from` to `to` (engine metres), a speck every spacing from `from`;
// its bursts are not thrown. `t.at` and `t.way` are ignored.
void throw_trail(const Effect& e, const double from[3], const double to[3], const Throw& t,
                 std::vector<Spark>& out);

}  // namespace ge::particles
