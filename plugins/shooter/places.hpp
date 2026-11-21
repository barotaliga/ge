// A MAP'S PLACE NAMES as any shooter reads them: the points a team calls out ("safe [ra]",
// "enemy at [quad-low]"), each a position and a short name, the place a point is called by being
// the name of its nearest point. How a game gets them (a file, a recording, its own naming) is the
// game's: the classic `.loc` format is plugins/classic/locs.hpp.
//
// `nearest_loc` does not allocate (its `seen` is the caller's). Deterministic: a stable order.
//
// ge:summary A map's place names as points: the nearest point and the place a spot is in
// ge:keywords loc locs place-name places callout nearest-loc loc_name_at loc_places place-middle
// ge:keywords area-labels teamplay
// ge:skill shooter bots
// ge:test tests/classic_locs_test.cpp
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "shooter/vec.hpp"

namespace ge::shooter {

struct Loc {
  std::string name;
  Vec3f at;
  std::string raw;  // the name as the file writes it, macros kept ("": `name`)
};

struct Locs {
  std::vector<Loc> points;
  std::string from;  // where they came from: a file's path, or how the game made them
};

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

}  // namespace ge::shooter
