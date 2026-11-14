// A FLOOR PLAN DRAWN AS AN SVG, read as a map plan (plan.hpp). Draw the level from above in any
// vector editor; each shape that is part of the map carries a `data-ge` attribute holding its
// plan statement without the shape, and the drawing gives the shape:
//
//   <rect x=".." y=".." width=".." height=".." data-ge="room hall z=0..192 ceil=sky1"/>
//   <polygon points=".." data-ge="room bend z=0..128"/>          <path d="M .. L .. Z" .../>
//   <polyline points=".." stroke-width="16" data-ge="wall z=0..128"/>   (thick= the stroke)
//   <rect ... data-ge="stairs z=0..64 up=+y"/>                  (stairs and ramps: a rect)
//   <circle cx=".." cy=".." data-ge="spawn z=24 angle=90"/>      (a point: light, spawn, entity)
//   <svg data-ge="world message=&quot;Two Towers&quot; light=12" data-ge-scale="1"> ...
//
// A statement with no shape (world, textures, pad, symmetry) is taken from any element. One
// drawing unit is `data-ge-scale` map units (1 when unset); the drawing's y runs down the page,
// the map's y up it, so a plan reads as drawn seen from above. Translations on an element or its
// groups (`transform="translate(x,y)"`) are applied; any other transform is refused. Path data
// takes M, L, H, V and Z (absolute or relative); curves are refused.
//
// ge:summary An SVG floor plan read as a map plan: data-ge statements, shapes from the drawing
// ge:keywords map-compiler svg floor-plan drawing data-ge rect polygon path polyline circle
// ge:keywords translate scale cad architecture plan
// ge:skill brush
// ge:test tests/brush_compile_test.cpp
#pragma once

#include <string>
#include <string_view>

namespace ge::brush::mapc {

// The drawing in `svg` as plan text; false with every fault found in `*error`.
bool svg_to_plan(std::string_view svg, std::string* plan, std::string* error);

}  // namespace ge::brush::mapc
