// THE BSP RENDERER'S LIGHT GRID (bsp_draw.hpp's v2 lit in real time): world cells round the eye,
// kGridCell m a side, each listing the frame's lights whose reach touches it, in the lights' own
// order, so a pixel of bsp.wgsl reads only its cell's lights (one outside the grid reads every
// light). A light left out of a cell is one its pixels skip anyway, and the rest are added in the
// same order, so the picture is the same as with every light read.
//
// ge:summary The BSP's light grid: the frame's lights binned into world cells round the eye
// ge:keywords bsp brush light-grid light-cull lights-per-pixel bin_light_grid gpu-light
// ge:skill brush render
// ge:test tests/brush_bsp_light_grid_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace ge::brush::app {

// THE LIGHT GRID (v2 lit in real time): world cells round the eye, kGridCell m a side, each
// listing the frame's lights whose reach touches it, in the lights' own order; a pixel reads only
// its cell's (one outside the grid reads every light). A light left out of a cell is one its
// pixels skip anyway, and the rest are added in the same order, so the picture is the same.
inline constexpr double kGridCell = 4.0;
inline constexpr uint32_t kGridX = 32, kGridY = 16, kGridZ = 32;  // bsp.wgsl repeats them
inline constexpr uint32_t kGridCells = kGridX * kGridY * kGridZ;
inline constexpr uint32_t kMaxGridRefs = 65536;  // the cells' lists in all; past it, no grid

// A light as bsp.wgsl reads it.
struct GpuLight {
  float pos[4];
  float colour[4];
  float info[4];
  float spot[4];  // xyz the way an aimed light points, w its cone's edge's cosine (-2: no cone)
};

// THE LIGHT GRID binned: into `rows`, each cell's first entry (kGridCells + 1, the last the
// lists' end), then the lists, each light's index in `lights` in ascending order, for every cell
// whose box `lights[i]`'s reach (pos[3]) comes within a margin of; `grid` gets the grid's corner
// round `eye` and its cell (w 0 when the lists would pass kMaxGridRefs: no grid). Returns the
// lists' entries (0 with no grid). Sizes `rows` the first time only.
uint32_t bin_light_grid(std::span<const GpuLight> lights, const double eye[3],
                        std::vector<uint32_t>& rows, float grid[4]);

}  // namespace ge::brush::app
