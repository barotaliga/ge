// THE HELD MODEL DRAWN ON THE CPU: the reference picture of the view-model pass, for the labs and
// the tests, on a GPU-less machine. It draws lit triangles in the VIEW's frame (+x right, +y up,
// +z forward, the eye at the origin) into an RGBA8 target with a depth buffer, perspective-correct,
// clipped at the near plane, back faces dropped, every pixel shaded by `shade_held` (the same
// light the GPU's shader computes), and resolves to sRGB with an exposure.
//
// A frame of the view-model pass is two draws into one target: the WORLD round the player (a
// lab's room, lit by the same lights with no floor and no gloss), then the depth cleared and the
// GUN drawn with its own lens, so it is never cut by a wall it is pushed into.
//
// The target holds linear light in floats until `resolve` writes the bytes; `supersample` draws
// it that many times larger each way and averages it down. Integer pixels from float arithmetic
// with no transcendental in the raster itself (the sRGB curve is a table built once with the
// engine's own libm), so a lab's pictures belong in its digest.
//
// ge:summary The held model drawn on the CPU: a lit, depth-tested raster in the view's frame
// ge:keywords viewmodel cpu raster rasterizer triangles depth z-buffer perspective-correct
// ge:keywords near-clip back-face supersample resolve exposure srgb draw_lit clear_depth
// ge:keywords held-raster view-target hidden-triangles hide-flame
// ge:skill render labs
// ge:test tests/viewmodel_raster_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "viewmodel/held.hpp"
#include "viewmodel/light.hpp"
#include "viewmodel/place.hpp"

namespace ge::viewmodel {

struct ViewTarget {
  int width = 0, height = 0;  // the drawn size (the picture's times the supersample)
  int supersample = 1;
  std::vector<float> rgb;    // linear, three a pixel
  std::vector<float> depth;  // view z, m (+inf: nothing)
};

ViewTarget make_target(int width, int height, int supersample, const float background[3]);
void clear_depth(ViewTarget& t);

// One draw: triangles in the view's frame. `colour` (linear rgb a vertex) or `skin` with `uv`
// (two a vertex) colours them; `fov_y` is the vertical lens, `near_m` the near plane.
struct DrawLit {
  std::span<const float> positions;  // xyz a vertex, view, m
  std::span<const float> normals;    // xyz a vertex, view, unit
  std::span<const uint32_t> indices;
  std::span<const float> colour;  // when no skin
  std::span<const float> uv;
  const Skin* skin = nullptr;
  double fov_y = 1.2;
  double near_m = 0.02;
  bool cull_back = true;
  std::span<const uint8_t> hidden;  // one a triangle: nonzero, not drawn (empty: all drawn)
};
void draw_lit(ViewTarget& t, const DrawLit& d, const Lighting& light);

// The target averaged down by its supersample and written as sRGB bytes (RGBA8, alpha 255), each
// pixel times `exposure` and rolled off above 1 (x / (1 + x) past a knee), so a flash does not
// clip flat.
std::vector<uint8_t> resolve(const ViewTarget& t, double exposure = 1.0);

// A held model placed by `place` (viewmodel/place.hpp) into the view: its frames `a`, `b` blended
// by `t`, drawn with `fov_y` into `out_positions` and `out_normals` (view).
void place_held(const HeldModel& model, const Placement& place, int a, int b, double t,
                std::vector<float>& out_positions, std::vector<float>& out_normals);

}  // namespace ge::viewmodel
