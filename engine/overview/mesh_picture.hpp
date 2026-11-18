// A TEXTURED MESH DRAWN SMALL ON THE CPU, for a level seen from above: a model as a cut-out icon
// fitted to its picture (`draw_icon`: a map's preview), or as a figure at a fixed size a unit with
// its origin at the picture's middle (`draw_figure`: the live overview), so a figure animates and
// turns in place without its size jumping from frame to frame. Lit from the upper left, cut out
// on a clear ground with a dark rim. Whatever the model's file, a game's reader turns one frame of
// it into a `Mesh` (plugins/morph/overview_mesh.hpp for vertex-animated models).
//
// ge:summary A textured mesh rastered small on the CPU: a cut-out icon or a figure at its size
// ge:keywords overview figure icon raster cut-out three-quarter draw_figure draw_icon live-map
// ge:skill map-design ui
// ge:test tests/overview_plate_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "ui/image.hpp"

namespace ge::overview {

// Triangles with their texture: positions in map units (z up), texels into `tex` within each
// vertex's rectangle.
struct Mesh {
  std::vector<float> pos;   // xyz a vertex
  std::vector<float> st;    // texels
  std::vector<float> rect;  // x y w h a vertex
  std::vector<uint32_t> idx;
  std::vector<uint8_t> tex;  // RGBA
  int tw = 0, th = 0;
};

// `m` from three quarters above (`azimuth` round z from +x toward +y, `elevation` up), lit from
// the upper left, cut out on a clear ground with a dark rim, `size` px square.
ge::ui::Image draw_icon(const Mesh& m, int size, double azimuth_deg, double elevation_deg);
// `m` with its nose up `pitch_deg`, turned to `yaw_deg` (counter-clockwise from +x seen from
// above, as the map's) and seen from the south from `elevation_deg` above, `px_per_unit` px a unit,
// its origin at the middle of a `size` px picture, cut out with a dark rim; what falls outside the
// picture is cut.
ge::ui::Image draw_figure(const Mesh& m, int size, double yaw_deg, double pitch_deg,
                          double elevation_deg, double px_per_unit);

}  // namespace ge::overview
