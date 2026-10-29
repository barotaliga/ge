// CHARTS: where a surface's luxels live. A level hands groups of coplanar triangles (a brush
// face, a merged floor) and the luxel size it wants; `plan_chart` lays a rectangle of luxels over
// each group's plane and `pack_charts` places every rectangle in one atlas. Both are pure
// functions of their input (the same atlas on every machine), so a bake's output can be cached by
// what went in.
//
// THE AXES. A chart's u runs along the world's x where the plane allows it (floors, ceilings and
// walls facing ±z), else along z; v completes it so u × v is the plane's normal. A level's
// textures usually run the same way, so a luxel's square sits square on the bricks.
//
// ge:summary Lightmap charts: a luxel rectangle over a plane, and every chart packed in one atlas
// ge:keywords bake lightmap chart atlas pack luxel plan_chart pack_charts chart_point
// ge:keywords chart_uv shelf-pack border add_chart add_quad
// ge:skill render
// ge:test tests/bake_chart_test.cpp
#pragma once

#include <cstdint>
#include <span>

#include "bake/scene.hpp"

namespace ge::bake {

// The largest chart side, luxels: a group bigger than this at `luxel_m` gets coarser luxels.
inline constexpr uint32_t kMaxChartSide = 512;

// A chart over the triangles `tris` (indices into scene.triangles()), which must be coplanar and
// face one way, at `luxel_m` metres a luxel (coarser when the group would pass kMaxChartSide).
// Its atlas place is left at 0, 0.
Chart plan_chart(const Scene& scene, std::span<const uint32_t> tris, double luxel_m);

// `plan_chart` over `tris`, appended to scene.charts and named by those triangles' tri_chart;
// returns its index.
uint32_t add_chart(Scene& scene, std::span<const uint32_t> tris, double luxel_m);

// A quad (corners in order, counter-clockwise seen from the side it faces) as two triangles of
// `material`, uncharted; returns the first triangle's index (the second follows it).
uint32_t add_quad(Scene& scene, const double corners[4][3], uint32_t material);

// Every chart of `scene` placed in an atlas `width` luxels wide (shelves, tallest first, ties by
// index), and the atlas's height (a multiple of 4) written to scene.atlas_height. False when a
// chart is wider than `width`.
bool pack_charts(Scene& scene, uint32_t width);

// Where the world point `p` falls on `chart`, in luxels from the chart's corner (0..width).
void chart_point(const Chart& chart, const double p[3], double* s, double* t);
// The same point as the atlas's 0..1 coordinates (what a vertex carries to the GPU).
void chart_uv(const Chart& chart, uint32_t atlas_width, uint32_t atlas_height, const double p[3],
              float uv[2]);

}  // namespace ge::bake
