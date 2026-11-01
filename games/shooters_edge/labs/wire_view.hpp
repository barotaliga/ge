// What Shooter's Edge's movement labs (`se-moves`, `se-hop`) draw a real map and a path through
// it with: the map as a WIREFRAME (every face's outline of the world model, each edge once),
// seen from above or from an angle, and a path laid over it coloured by speed. Header-only, beside
// the labs (a lab's helper never goes into a library: the labs skill).
//
// THE VIEW (`WireView`) is an orthographic camera: the map turned by `yaw` about z, then tilted
// so `elevation` 90 looks straight down (x east, y north up the picture) and less looks at it
// from the south-east at that angle. It is fitted to the points handed to `fit` (the map's
// vertices, or a path's for a close-up) with a margin. An edge is drawn lighter the further it
// lies (from above: the lower it lies), so floors read under walls.
//
// THE SPEED COLOURS (`speed_colour`): grey standing, blue at a walk's 320 units/s, then green,
// yellow, orange to red at 720 and more, so a bunny hop's gain reads along its line.
//
// ge:summary The movement labs' wireframe: a map's face outlines from above or at an angle, paths
// ge:keywords lab wireframe wire map edges orthographic oblique top-view path speed-colour fit
// ge:skill shooter labs
// ge:test tests/shooter_hops_test.cpp
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "brush/map.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "lab/raster.hpp"
#include "shooter/vec.hpp"

namespace shooters_edge::labs {

namespace gl = ge::lab;
namespace sh = ge::shooter;

struct WireEdge {
  sh::Vec3f a, b;
  bool sky = false, liquid = false;
};

// Every face outline of the map's world model, each edge once (a triangle's edge inside one face
// is not an outline; an edge two faces share is kept once).
inline std::vector<WireEdge> wire_edges(const ge::brush::Map& map) {
  struct Key {
    int32_t a[3], b[3];
    bool sky, liquid;
  };
  auto q = [](const float* p, int32_t* o) {
    for (int i = 0; i < 3; i++) o[i] = static_cast<int32_t>(std::lround(p[i] * 8));
  };
  std::vector<Key> keys;
  if (map.models.empty()) return {};
  const ge::brush::Model& world = map.models[0];
  std::vector<std::pair<uint32_t, uint32_t>> local;
  for (uint32_t s = world.first_surface; s < world.first_surface + world.surface_count; s++) {
    if (s >= map.surfaces.size()) break;
    const ge::brush::Surface& surf = map.surfaces[s];
    const bool sky = surf.kind == ge::brush::SurfaceKind::sky ||
                     (surf.texture < map.textures.size() && map.textures[surf.texture].sky);
    const bool liquid = surf.kind == ge::brush::SurfaceKind::liquid;
    local.clear();
    for (uint32_t t = 0; t + 2 < surf.count; t += 3)
      for (int e = 0; e < 3; e++) {
        uint32_t i = map.indices[surf.first + t + static_cast<uint32_t>(e)];
        uint32_t j = map.indices[surf.first + t + static_cast<uint32_t>((e + 1) % 3)];
        if (i > j) std::swap(i, j);
        local.push_back({i, j});
      }
    std::sort(local.begin(), local.end());
    for (size_t k = 0; k < local.size(); k++) {
      const bool twice =
          (k + 1 < local.size() && local[k + 1] == local[k]) || (k > 0 && local[k - 1] == local[k]);
      if (twice) continue;  // inside the face
      Key key{};
      q(map.vertices[local[k].first].pos, key.a);
      q(map.vertices[local[k].second].pos, key.b);
      if (std::lexicographical_compare(key.b, key.b + 3, key.a, key.a + 3))
        for (int i = 0; i < 3; i++) std::swap(key.a[i], key.b[i]);
      key.sky = sky;
      key.liquid = liquid;
      keys.push_back(key);
    }
  }
  auto less = [](const Key& x, const Key& y) {
    for (int i = 0; i < 3; i++)
      if (x.a[i] != y.a[i]) return x.a[i] < y.a[i];
    for (int i = 0; i < 3; i++)
      if (x.b[i] != y.b[i]) return x.b[i] < y.b[i];
    return false;
  };
  std::sort(keys.begin(), keys.end(), less);
  std::vector<WireEdge> out;
  for (size_t k = 0; k < keys.size(); k++) {
    if (k > 0 && !less(keys[k - 1], keys[k])) continue;
    const Key& e = keys[k];
    auto f = [](int32_t v) { return static_cast<float>(v) / 8.0f; };
    out.push_back(
        {{f(e.a[0]), f(e.a[1]), f(e.a[2])}, {f(e.b[0]), f(e.b[1]), f(e.b[2])}, e.sky, e.liquid});
  }
  return out;
}

struct WireView {
  double yaw = 0, elevation = 90;  // degrees
  int width = 1200, height = 900, margin = 28;
  // Filled by fit():
  double scale = 1, ox = 0, oy = 0;
  double dlo = 0, dhi = 1;  // the depth's range over what was fitted

  // The turned and tilted point: screen x, screen y (up), depth (further larger).
  void turned(sh::Vec3f p, double& sx, double& sy, double& depth) const {
    const double c = ge::core::libm::cos(yaw * ge::core::kPi / 180),
                 s = ge::core::libm::sin(yaw * ge::core::kPi / 180);
    const double rx = p.x * c + p.y * s, ry = -p.x * s + p.y * c;
    const double ce = ge::core::libm::cos(elevation * ge::core::kPi / 180),
                 se = ge::core::libm::sin(elevation * ge::core::kPi / 180);
    sx = rx;
    sy = ry * se + p.z * ce;
    depth = ry * ce - p.z * se;
  }
  // Fits the view to `points` (the picture's size kept; the scale the largest that fits both).
  void fit(std::span<const sh::Vec3f> points) {
    double x0 = 1e30, x1 = -1e30, y0 = 1e30, y1 = -1e30, d0 = 1e30, d1 = -1e30;
    for (const sh::Vec3f& p : points) {
      double sx, sy, d;
      turned(p, sx, sy, d);
      x0 = std::min(x0, sx), x1 = std::max(x1, sx), y0 = std::min(y0, sy), y1 = std::max(y1, sy);
      d0 = std::min(d0, d), d1 = std::max(d1, d);
    }
    if (x1 <= x0 || y1 <= y0) return;
    const double w = width - 2.0 * margin, h = height - 2.0 * margin - 14;
    scale = std::min(w / (x1 - x0), h / (y1 - y0));
    ox = margin + (w - (x1 - x0) * scale) / 2 - x0 * scale;
    oy = margin + 14 + (h - (y1 - y0) * scale) / 2 + y1 * scale;
    dlo = d0;
    dhi = d1 > d0 ? d1 : d0 + 1;
  }
  // The pixel and the depth's share (0 near .. 1 far) of a map point.
  void pixel(sh::Vec3f p, double& px, double& py, double& far) const {
    double sx, sy, d;
    turned(p, sx, sy, d);
    px = ox + sx * scale;
    py = oy - sy * scale;
    far = std::clamp((d - dlo) / (dhi - dlo), 0.0, 1.0);
  }
};

inline std::vector<sh::Vec3f> edge_points(std::span<const WireEdge> edges) {
  std::vector<sh::Vec3f> v;
  v.reserve(edges.size() * 2);
  for (const WireEdge& e : edges) v.push_back(e.a), v.push_back(e.b);
  return v;
}

inline gl::Picture wire_picture(std::span<const WireEdge> edges, const WireView& v,
                                gl::Rgba paper = {250, 250, 246, 255}) {
  gl::Picture p = gl::create_picture(v.width, v.height, paper);
  for (const WireEdge& e : edges) {
    double ax, ay, af, bx, by, bf;
    v.pixel(e.a, ax, ay, af);
    v.pixel(e.b, bx, by, bf);
    const double far = (af + bf) / 2;
    const auto k = static_cast<uint8_t>(40 + 150 * far);  // near dark, far light
    gl::Rgba c{k, static_cast<uint8_t>(k + 6), static_cast<uint8_t>(k + 14), 255};
    if (e.sky) c = {200, 214, 236, 255};
    if (e.liquid) c = {90, 150, 210, 255};
    gl::draw_line(p, ax, ay, bx, by, c);
  }
  return p;
}

inline gl::Rgba speed_colour(float speed) {
  struct Stop {
    float v;
    gl::Rgba c;
  };
  static constexpr Stop kStops[] = {{0, {150, 150, 150, 255}},  {320, {40, 90, 220, 255}},
                                    {420, {20, 170, 120, 255}}, {520, {210, 190, 20, 255}},
                                    {620, {240, 120, 20, 255}}, {720, {210, 30, 40, 255}}};
  if (speed <= kStops[0].v) return kStops[0].c;
  for (size_t i = 1; i < std::size(kStops); i++)
    if (speed <= kStops[i].v) {
      const float t = (speed - kStops[i - 1].v) / (kStops[i].v - kStops[i - 1].v);
      auto mix = [t](uint8_t a, uint8_t b) {
        return static_cast<uint8_t>(static_cast<float>(a) + (static_cast<float>(b) - a) * t);
      };
      const gl::Rgba &a = kStops[i - 1].c, &b = kStops[i].c;
      return {mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), 255};
    }
  return kStops[std::size(kStops) - 1].c;
}

// The speed colours' legend at the picture's top-left.
inline void speed_legend(gl::Picture& p, int x, int y) {
  gl::draw_text(p, x, y, "SPEED", {40, 40, 40, 255});
  int at = x + gl::text_width("SPEED ") + 4;
  for (int v = 0; v <= 720; v += 8) {
    gl::fill_rect(p, at + v * 0.25, y, 2, 8, speed_colour(static_cast<float>(v)));
  }
  for (int v : {0, 320, 520, 720}) {
    const std::string s = std::to_string(v);
    gl::draw_text(p, at + v * 0.25 - gl::text_width(s) * 0.5, y + 10, s, {60, 60, 60, 255});
  }
}

// A path (one point a frame, `speeds` beside) over the picture: a line coloured by speed, broken
// where `cut` says so (a respawn, a teleport).
inline void draw_path(gl::Picture& p, const WireView& v, std::span<const sh::Vec3f> pts,
                      std::span<const float> speeds, std::span<const uint8_t> cut,
                      double thickness = 2) {
  for (size_t i = 1; i < pts.size(); i++) {
    if (!cut.empty() && (cut[i] || cut[i - 1])) continue;
    double ax, ay, af, bx, by, bf;
    v.pixel(pts[i - 1], ax, ay, af);
    v.pixel(pts[i], bx, by, bf);
    gl::draw_line(p, ax, ay, bx, by, speed_colour(speeds[i]), thickness);
  }
}

}  // namespace shooters_edge::labs
