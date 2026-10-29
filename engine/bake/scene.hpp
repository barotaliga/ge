// WHAT A LIGHT BAKE READS: the static world as triangles, each with a material and the chart its
// luxels live on, the lights, and the open sky's brightness. A level format (a brush map, a room
// file) fills one of these; `bake.hpp` turns it into a lightmap. The baker knows nothing of where
// the triangles came from.
//
// UNITS. Every brightness is in the LIGHTMAP'S UNITS, the ones a surface is shown in: a white
// face under an irradiance of 1 shows white (shown = albedo × E). So:
// - a point or spot light's `colour` is the irradiance it gives a face 1 m away, facing it;
// - the sun's `colour` is the irradiance on a face turned to it;
// - a material's `emit` and the scene's `sky` are SHOWN brightnesses: an emitting face shows
//   albedo × E + emit, and a face under an open hemisphere of sky of brightness s gets E = s.
// Light leaving a surface (what a gather ray carries) is shown / π, so the units close: the
// irradiance integral of a radiance L over the hemisphere, cosine-weighted, is π × L.
//
// GEOMETRY. Metres, y up (the engine's frame). Triangles are one-sided for light: a face lit is
// lit on the side its winding faces (counter-clockwise seen from in front). A charted triangle
// lies in its chart's plane (charts are planar; `chart.hpp` plans them).
//
// ge:summary What a light bake reads: triangles, materials, charts, lights, the sky
// ge:keywords bake lightmap light-bake scene material albedo emit emissive sky light point-light
// ge:keywords spot-light sun chart luxel irradiance units
// ge:skill render
// ge:test tests/bake_bake_test.cpp
#pragma once

#include <cstdint>
#include <vector>

namespace ge::bake {

inline constexpr uint32_t kNoChart = 0xffffffffu;

struct Material {
  float albedo[3] = {0.5f, 0.5f, 0.5f};  // the share of light a face sends on, linear
  float emit[3] = {0, 0, 0};             // what it shows of its own (lava, a lamp's panel)
  bool sky = false;                      // a window onto the sky: a ray that meets it sees `sky`
};

enum class LightKind : uint8_t { point, spot, sun };

struct Light {
  LightKind kind = LightKind::point;
  double pos[3] = {0, 0, 0};     // point, spot: m
  double dir[3] = {0, -1, 0};    // spot: where it points; sun: the way its light TRAVELS (unit)
  double colour[3] = {1, 1, 1};  // the units' header: at 1 m (point, spot), or facing (sun)
  double radius = 0.05;  // m: its size (soft shadows), its light falling as 1 / (d^2 + r^2);
                         // the sun: its angular radius, rad
  double reach = 0;      // m: where its light has faded to nothing, smoothly (0: never)
  double cone_cos[2] = {0.9, 0.8};  // spot: full inside the first, none outside the second
};

// A PLANAR CHART: a rectangle of luxels on one plane. Luxel (i, j)'s centre is
// origin + (i + 0.5) u + (j + 0.5) v; `u` and `v` are one luxel long each and at right angles,
// and u × v is the chart's normal (the side its triangles face). Its place in the atlas is
// (x, y) .. (x + width, y + height); a chart keeps a luxel of border round its triangles so a
// bilinear read at an edge never reaches a neighbour's luxels.
struct Chart {
  double origin[3] = {0, 0, 0};
  double u[3] = {1, 0, 0}, v[3] = {0, 0, 1};
  double normal[3] = {0, 1, 0};
  uint32_t width = 0, height = 0;  // luxels, the border included
  uint32_t x = 0, y = 0;           // in the atlas (pack_charts)
};

struct Scene {
  std::vector<float> positions;        // xyz a vertex, m
  std::vector<uint32_t> indices;       // three a triangle
  std::vector<uint32_t> tri_material;  // one a triangle, into `materials`
  std::vector<uint32_t> tri_chart;     // one a triangle, into `charts`; kNoChart: never lit
  std::vector<Material> materials;
  std::vector<Light> lights;
  std::vector<Chart> charts;
  float sky[3] = {0, 0,
                  0};  // the open sky's shown brightness, past the scene or through a sky face
  uint32_t atlas_width = 0, atlas_height = 0;  // pack_charts
  size_t triangles() const { return indices.size() / 3; }
};

}  // namespace ge::bake
