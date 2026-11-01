// A MAP'S PLACE NAMES: the points a team calls out ("safe [ra]", "enemy at [quad-low]"), each a
// position and a short name, the place a point is called by being the name of its nearest point.
//
// THE FILE FORMAT (a `.loc` file, one a map, named after it): plain text, one point a line, each
// `x y z name`, the three coordinates whole numbers in EIGHTHS of a map unit (a point at 512 is
// written 4096) and the name the rest of the line. A line that is blank, too short or does not
// start with three numbers is skipped, and so are both line ends (LF and CR LF). A name may carry
// MACROS, `$loc_name_<key>`, that a client expands to the player's own short words: `<key>` is
// one of the names `kLocKeys` lists, matched longest first, and expands to itself, except
// `separator`, which is the joining `-`. So `$loc_name_mh$loc_name_separatorlow` reads `mh-low`. A
// macro whose key is not listed keeps the run of letters, digits and underscores after it.
//
// Where no file is there, a game names its points itself (the Shooter's Edge bots name each item's
// spot), so every map has its place names.
//
// Parsing allocates; `nearest_loc` does not. Deterministic: integer coordinates, a stable order.
//
// ge:summary A map's place names: the .loc format read, macros expanded, the nearest point named
// ge:keywords loc locations place-name callout loc-file location-macro nearest-loc teamplay
// ge:keywords parse_locs nearest_loc loc_name_at
// ge:skill shooter bots
// ge:test tests/shooter_locs_test.cpp
#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "shooter/vec.hpp"

namespace ge::shooter {

// The macro keys a name may use, as `$loc_name_<key>`.
inline constexpr std::array<std::string_view, 16> kLocKeys = {
    "separator", "quad", "pent", "ring", "suit", "ssg", "sng", "ng",
    "gl",        "rl",   "lg",   "ra",   "ya",   "ga",  "mh",  "rune"};

struct Loc {
  std::string name;
  Vec3f at;
};

struct Locs {
  std::vector<Loc> points;
  std::string from;  // where they came from: a file's path, or how the game made them
};

// A name with its macros expanded.
std::string expand_loc_name(std::string_view raw);

// The points of a `.loc` file's text, added to `out`; the number of lines read as points.
int parse_locs(std::string_view text, Locs& out);

// The point nearest `p` (the first of equals), or nullptr with none.
const Loc* nearest_loc(const Locs& locs, Vec3f p);

// The name of the place `p` is in: its nearest point's, or "" with none.
std::string loc_name_at(const Locs& locs, Vec3f p);

}  // namespace ge::shooter
