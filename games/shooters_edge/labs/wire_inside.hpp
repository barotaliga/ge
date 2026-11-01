// What Shooter's Edge's movement labs draw a map with from INSIDE it: a perspective camera standing
// in the map (an eye, a yaw and a pitch, a field of view), the world model's faces filled pale by
// how they face the light and written to a depth buffer, then every face outline (wire_view.hpp's
// edges) and a path drawn as a line in the world, each pixel kept only where nothing of the map is
// nearer, so walls hide what is behind them and the path shows where it runs in the rooms.
// Header-only, beside the labs (a lab's helper never goes into a library: the labs skill).
//
// The camera's axes are the map's: yaw counter-clockwise from +x, pitch up positive. A triangle
// or a segment is clipped at the near plane (`near` units), so a wall beside the eye is cut, not
// dropped. Colours: faces near white (floors lighter than walls), outlines dark fading with
// distance to `fade` units, sky faces pale blue, liquids blue.
//
// ge:summary The movement labs' view from inside a map: faces in a depth buffer, outlines, paths
// ge:keywords lab wireframe perspective inside camera depth-buffer zbuffer hidden-line path chase
// ge:skill shooter labs
// ge:test tests/shooter_hops_test.cpp
#pragma once

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

#include "brush/map.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "lab/raster.hpp"
#include "shooters_edge/labs/wire_view.hpp"

namespace shooters_edge::labs {

struct InsideCamera {
  sh::Vec3f eye;
  double yaw = 0, pitch = 0, fov = 90;  // degrees; fov across
  int width = 960, height = 600;
  double near = 4, fade = 2400;
};

class InsideView {
 public:
  explicit InsideView(const InsideCamera& c) : c_(c) {
    const double y = c.yaw * ge::core::kPi / 180, p = c.pitch * ge::core::kPi / 180;
    fwd_[0] = ge::core::libm::cos(p) * ge::core::libm::cos(y),
    fwd_[1] = ge::core::libm::cos(p) * ge::core::libm::sin(y), fwd_[2] = ge::core::libm::sin(p);
    right_[0] = ge::core::libm::sin(y), right_[1] = -ge::core::libm::cos(y), right_[2] = 0;
    up_[0] = -ge::core::libm::sin(p) * ge::core::libm::cos(y),
    up_[1] = -ge::core::libm::sin(p) * ge::core::libm::sin(y), up_[2] = ge::core::libm::cos(p);
    focal_ = (c.width / 2.0) / ge::core::libm::tan(c.fov * ge::core::kPi / 360);
    depth_.assign(static_cast<size_t>(c.width) * static_cast<size_t>(c.height), 1e30);
    pic_ = gl::create_picture(c.width, c.height, {236, 238, 242, 255});
  }

  // The world model's faces into the picture and the depth buffer.
  void faces(const ge::brush::Map& map) {
    if (map.models.empty()) return;
    const ge::brush::Model& w = map.models[0];
    for (uint32_t s = w.first_surface; s < w.first_surface + w.surface_count; s++) {
      if (s >= map.surfaces.size()) break;
      const ge::brush::Surface& surf = map.surfaces[s];
      const bool sky = surf.kind == ge::brush::SurfaceKind::sky ||
                       (surf.texture < map.textures.size() && map.textures[surf.texture].sky);
      for (uint32_t t = 0; t + 2 < surf.count; t += 3) {
        const float* v[3];
        for (int k = 0; k < 3; k++)
          v[k] = map.vertices[map.indices[surf.first + t + static_cast<uint32_t>(k)]].pos;
        // The face's normal: floors light, walls a shade darker, ceilings darker still.
        const double e1[3] = {v[1][0] - v[0][0], v[1][1] - v[0][1], v[1][2] - v[0][2]};
        const double e2[3] = {v[2][0] - v[0][0], v[2][1] - v[0][1], v[2][2] - v[0][2]};
        double n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                       e1[0] * e2[1] - e1[1] * e2[0]};
        const double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (len <= 0) continue;
        const double nz = std::fabs(n[2] / len);
        const auto k = static_cast<uint8_t>(205 + 40 * nz);
        gl::Rgba col{k, k, static_cast<uint8_t>(k > 250 ? 255 : k + 5), 255};
        if (sky) col = {214, 226, 244, 255};
        if (surf.kind == ge::brush::SurfaceKind::liquid) col = {150, 190, 230, 255};
        triangle(v, col);
      }
    }
  }

  // A segment in the world, kept where the map is not nearer.
  void line(sh::Vec3f a, sh::Vec3f b, gl::Rgba col, double thickness = 1, bool fade = true,
            double bias = 2) {
    double ca[3], cb[3];
    cam(a, ca);
    cam(b, cb);
    if (ca[2] < c_.near && cb[2] < c_.near) return;
    if (ca[2] < c_.near || cb[2] < c_.near) {
      const double t = (c_.near - ca[2]) / (cb[2] - ca[2]);
      double* lo = ca[2] < c_.near ? ca : cb;
      for (int i = 0; i < 3; i++) lo[i] = ca[i] + (cb[i] - ca[i]) * t;
    }
    const double ax = sx(ca), ay = sy(ca), bx = sx(cb), by = sy(cb);
    const double steps = std::max(1.0, std::ceil(std::max(std::fabs(bx - ax), std::fabs(by - ay))));
    if (steps > 8000) return;
    const int r = static_cast<int>(thickness / 2);
    for (int i = 0; i <= static_cast<int>(steps); i++) {
      const double u = i / steps;
      // Depth along a projected line: interpolate the inverse.
      const double iz = (1 - u) / ca[2] + u / cb[2];
      const double z = 1 / iz;
      const int x = static_cast<int>(ax + (bx - ax) * u), y = static_cast<int>(ay + (by - ay) * u);
      gl::Rgba c = col;
      if (fade) {
        const double f = std::clamp(z / c_.fade, 0.0, 0.85);
        c.r = static_cast<uint8_t>(c.r + (236 - c.r) * f);
        c.g = static_cast<uint8_t>(c.g + (238 - c.g) * f);
        c.b = static_cast<uint8_t>(c.b + (242 - c.b) * f);
      }
      for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) plot(x + dx, y + dy, z - bias - z * 0.002, c);
    }
  }

  // The map's outlines.
  void edges(std::span<const WireEdge> es) {
    for (const WireEdge& e : es)
      line(e.a, e.b, e.sky ? gl::Rgba{170, 190, 220, 255} : gl::Rgba{40, 46, 58, 255});
  }

  // A point's pixel and whether it is in front of the eye.
  bool pixel(sh::Vec3f p, double& x, double& y) const {
    double c[3];
    cam(p, c);
    if (c[2] < c_.near) return false;
    x = sx(c);
    y = sy(c);
    return true;
  }

  gl::Picture& picture() { return pic_; }

 private:
  void cam(sh::Vec3f p, double* out) const {
    const double d[3] = {p.x - c_.eye.x, p.y - c_.eye.y, p.z - c_.eye.z};
    out[0] = d[0] * right_[0] + d[1] * right_[1] + d[2] * right_[2];
    out[1] = d[0] * up_[0] + d[1] * up_[1] + d[2] * up_[2];
    out[2] = d[0] * fwd_[0] + d[1] * fwd_[1] + d[2] * fwd_[2];
  }
  double sx(const double* c) const { return c_.width / 2.0 + c[0] / c[2] * focal_; }
  double sy(const double* c) const { return c_.height / 2.0 - c[1] / c[2] * focal_; }

  void plot(int x, int y, double z, gl::Rgba c) {
    if (x < 0 || y < 0 || x >= c_.width || y >= c_.height) return;
    const size_t i =
        static_cast<size_t>(y) * static_cast<size_t>(c_.width) + static_cast<size_t>(x);
    if (z > depth_[i]) return;
    gl::set_pixel(pic_, x, y, c);
  }

  void triangle(const float* const v[3], gl::Rgba col) {
    // To camera space, clipped at the near plane (up to four corners).
    double in[3][3], poly[4][3];
    for (int k = 0; k < 3; k++) cam({v[k][0], v[k][1], v[k][2]}, in[k]);
    int n = 0;
    for (int k = 0; k < 3; k++) {
      const double* a = in[k];
      const double* b = in[(k + 1) % 3];
      const bool ain = a[2] >= c_.near, bin = b[2] >= c_.near;
      if (ain) {
        for (int i = 0; i < 3; i++) poly[n][i] = a[i];
        n++;
      }
      if (ain != bin) {
        const double t = (c_.near - a[2]) / (b[2] - a[2]);
        for (int i = 0; i < 3; i++) poly[n][i] = a[i] + (b[i] - a[i]) * t;
        n++;
      }
    }
    for (int k = 1; k + 1 < n; k++) fill(poly[0], poly[k], poly[k + 1], col);
  }

  void fill(const double* a, const double* b, const double* c, gl::Rgba col) {
    const double x[3] = {sx(a), sx(b), sx(c)}, y[3] = {sy(a), sy(b), sy(c)};
    const double iz[3] = {1 / a[2], 1 / b[2], 1 / c[2]};
    const double area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
    if (std::fabs(area) < 1e-9) return;
    const int x0 = std::max(0, static_cast<int>(std::floor(std::min({x[0], x[1], x[2]}))));
    const int x1 =
        std::min(c_.width - 1, static_cast<int>(std::ceil(std::max({x[0], x[1], x[2]}))));
    const int y0 = std::max(0, static_cast<int>(std::floor(std::min({y[0], y[1], y[2]}))));
    const int y1 =
        std::min(c_.height - 1, static_cast<int>(std::ceil(std::max({y[0], y[1], y[2]}))));
    for (int py = y0; py <= y1; py++)
      for (int px = x0; px <= x1; px++) {
        const double qx = px + 0.5, qy = py + 0.5;
        const double w0 = ((x[1] - qx) * (y[2] - qy) - (x[2] - qx) * (y[1] - qy)) / area;
        const double w1 = ((x[2] - qx) * (y[0] - qy) - (x[0] - qx) * (y[2] - qy)) / area;
        const double w2 = 1 - w0 - w1;
        if (w0 < 0 || w1 < 0 || w2 < 0) continue;
        const double z = 1 / (w0 * iz[0] + w1 * iz[1] + w2 * iz[2]);
        const size_t i =
            static_cast<size_t>(py) * static_cast<size_t>(c_.width) + static_cast<size_t>(px);
        if (z >= depth_[i]) continue;
        depth_[i] = z;
        gl::set_pixel(pic_, px, py, col);
      }
  }

  InsideCamera c_;
  double fwd_[3], right_[3], up_[3], focal_ = 1;
  std::vector<double> depth_;
  gl::Picture pic_;
};

}  // namespace shooters_edge::labs
