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
// Parsing allocates; `nearest_loc` does not (its `seen` is the caller's). Deterministic: integer
// coordinates, a stable order.
//
// ge:summary A map's place names: the .loc format read, macros expanded, the nearest point named
// ge:keywords loc locations place-name callout loc-file location-macro nearest-loc teamplay
// ge:keywords parse_locs nearest_loc loc_name_at loc_places place-middle area-labels
// ge:keywords recorded_loc_lines read_recorded_loc recorded-places demo-places
// ge:keywords add_loc remove_nearest_loc locs_text loc-editing save-locs
// ge:skill shooter bots
// ge:test tests/shooter_locs_test.cpp
#pragma once

#include <array>
#include <functional>
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
  std::string raw;  // the name as the file writes it, macros kept ("": `name`)
};

struct Locs {
  std::vector<Loc> points;
  std::string from;  // where they came from: a file's path, or how the game made them
};

// A name with its macros expanded.
std::string expand_loc_name(std::string_view raw);

// The points of a `.loc` file's text, added to `out`; the number of lines read as points.
int parse_locs(std::string_view text, Locs& out);

// Each place once: its name at the middle of its points, in the order the names first appear (a map
// pictured with its places named; a callout's place read as one spot).
std::vector<Loc> loc_places(const Locs& locs);

// Whether the straight line from `from` to a point is clear (no solid between them), for a
// nearest point that must be in sight; empty: every point counts.
using LocSight = std::function<bool(Vec3f from, Vec3f to)>;

// The point nearest `p` (the first of equals), or nullptr with none; with `range` above 0, only a
// point at most that far (units, straight line) counts, and with `seen`, only one it says is in
// sight from `p`.
const Loc* nearest_loc(const Locs& locs, Vec3f p, float range = 0, const LocSight& seen = {});

// The name of the place `p` is in: its nearest point's (within `range`, as above), or "" with none.
std::string loc_name_at(const Locs& locs, Vec3f p, float range = 0, const LocSight& seen = {});

// EDITING, as a player names the places of a map standing in them: a point added at `at` named
// `raw` (its macros kept for the file, expanded for `name`), always at the end, with no check for
// another point near it; the point nearest `at` removed (the first of equals), false with none;
// and the points as a file's text (`locs_text`): one line a point in order, each coordinate times
// eight rounded to the nearest whole number (halves away from zero), right-aligned in at least
// four places, one space between, then a space and the raw name; LF line ends, the last line's
// too; no header. A name longer than kLocNameMax is cut to it on reading, as by `parse_locs`.
inline constexpr size_t kLocNameMax = 63;
void add_loc(Locs& locs, Vec3f at, std::string_view raw);
bool remove_nearest_loc(Locs& locs, Vec3f at, Loc* removed = nullptr, float range = 0,
                        const LocSight& seen = {});
std::string locs_text(const Locs& locs);

// PLACES CARRIED IN A RECORDING: a recording keeps the place names its game was played with as
// stuffed lines, `//loc ` and a line of the file's text (`locs_text`), one a point, so watching it
// needs no file and another client runs nothing (the line is a comment to its console).
// `recorded_loc_lines` makes them, each ending in a newline; `read_recorded_loc` adds the point of
// one such line to `out` (true), false when the line is not one.
inline constexpr std::string_view kRecordedLoc = "//loc ";
std::vector<std::string> recorded_loc_lines(const Locs& locs);
bool read_recorded_loc(std::string_view line, Locs& out);

}  // namespace ge::shooter
