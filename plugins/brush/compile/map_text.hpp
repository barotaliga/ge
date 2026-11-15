// THE MAP EDITORS' TEXT (`.map`): a map's source as the level editors read and write it, the one
// form every map is brought into before it is cut, merged or built again (docs/levels.md § The
// map editors' text). Entities in braces, each `"key" "value"` lines then its brushes, each brush
// in braces, one side a line:
//
//   ( x y z ) ( x y z ) ( x y z ) TEXTURE xoff yoff turn xscale yscale [contents flags value]
//   ( x y z ) ( x y z ) ( x y z ) TEXTURE [ ux uy uz uoff ] [ vx vy vz voff ] turn xscale yscale
//
// - The three points lie on the side's plane; the plane faces where (p0 − p1) × (p2 − p1)
//   points, out of the brush.
// - The first form (the classic one) projects the texture axially (`axial_vecs`) with the shift,
//   turn and scale given; the three numbers after it are the 38 family's surface contents, flags
//   and value (the contents read and dropped: they follow from the texture's name here).
// - The second (the "220" form, worldspawn's `mapversion` 220) gives the texture's axes and
//   shifts outright: s = (u · p) / xscale + uoff, t = (v · p) / yscale + voff. Any projection a
//   built map holds can be written this way, so the writer always uses it.
// - `//` starts a comment to the line's end.
//
// The writer puts each side's points on its plane a fixed way (the plane's nearest point to the
// origin and 64 units along two axes in it), every number in its shortest exact text, and sets
// `mapversion` 220; read back, every plane and projection is the one written within rounding.
//
// ge:summary The map editors' .map text read into a compiler source and written from one
// ge:keywords map-text dot-map map-file editor valve-220 mapversion brush side three-points
// ge:keywords projection axial import export common-format map-sdk
// ge:skill brush map-design
// ge:test tests/brush_map_text_test.cpp
#pragma once

#include <string>
#include <string_view>

#include "brush/compile/source.hpp"

namespace ge::brush::mapc {

// `text` read into `*out` (entities in order, the world first, each brush's contents from its
// textures). False with every fault found, one a line ("line N: what; next: what to write").
bool read_map_text(std::string_view text, Source* out, std::string* error);

// `src` as `.map` text, every side in the 220 form.
std::string write_map_text(const Source& src);

}  // namespace ge::brush::mapc
