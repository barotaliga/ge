// WHAT A CAMERA'S LENS SEES, sketched on the CPU raster for the camera labs: the ground as a
// grid of lines (every 2 m round the subject, each line bent over the ground's height), the
// trunks as posts, the subject as a figure (red: the body, its head a disc), and the horizon,
// all projected through the pose exactly as the renderer's matrices do (the same basis, the
// same roll, the vertical fov). A frame from this reads the composition, the roll, the field of
// view and what is in the way, without a GPU; and the pose's own numbers in its corner.
//
// ge:summary Lab helper: a camera pose's view sketched on the CPU raster, and sheets of them
// ge:keywords lab camera sketch wireframe view projection grid horizon subject trunks sheet
// ge:keywords sketch_view
// ge:skill camera labs
// ge:test tests/camera_scenarios_test.cpp
#pragma once

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "camera/pose.hpp"
#include "camera/scenarios.hpp"
#include "camera/subject.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "lab/raster.hpp"

namespace ge::lab::camera_sketch {

namespace cam = ge::camera;
namespace libm = ge::core::libm;

inline constexpr Rgba kSky{150, 186, 222};
inline constexpr Rgba kSnow{236, 240, 246};
inline constexpr Rgba kGrid{120, 132, 150};
inline constexpr Rgba kHorizon{70, 90, 120};
inline constexpr Rgba kBody{210, 40, 40};
inline constexpr Rgba kTrunk{90, 60, 40};
inline constexpr Rgba kInk{20, 24, 30};

// The lens's basis, as engine/render/view.cpp builds it: right, up, forward.
struct Basis {
  cam::Vec3 r, u, f;
  double tx, ty;  // tan of the half fovs, across and up
};

inline Basis basis(const cam::Pose& p, double aspect) {
  Basis b;
  b.f = cam::look_forward(p.yaw, p.pitch);
  double rx = b.f.z, rz = -b.f.x;
  const double rl = ge::core::hypot(rx, rz);
  if (rl < 1e-9) {
    rx = libm::cos(p.yaw);
    rz = -libm::sin(p.yaw);
  } else {
    rx /= rl;
    rz /= rl;
  }
  cam::Vec3 r{rx, 0, rz};
  cam::Vec3 u{b.f.y * r.z - b.f.z * r.y, b.f.z * r.x - b.f.x * r.z, b.f.x * r.y - b.f.y * r.x};
  if (p.roll != 0) {
    const double c = libm::cos(p.roll), s = libm::sin(p.roll);
    const cam::Vec3 r2{r.x * c - u.x * s, r.y * c - u.y * s, r.z * c - u.z * s};
    const cam::Vec3 u2{u.x * c + r.x * s, u.y * c + r.y * s, u.z * c + r.z * s};
    r = r2;
    u = u2;
  }
  b.r = r;
  b.u = u;
  b.ty = libm::tan(p.fov_y * 0.5);
  b.tx = b.ty * aspect;
  return b;
}

struct Lens {
  cam::Pose pose;
  Basis b;
  int w, h;
};

inline cam::Vec3 to_cam(const Lens& l, const cam::Vec3& p) {
  const cam::Vec3 d{p.x - l.pose.eye.x, p.y - l.pose.eye.y, p.z - l.pose.eye.z};
  return {d.x * l.b.r.x + d.y * l.b.r.y + d.z * l.b.r.z,
          d.x * l.b.u.x + d.y * l.b.u.y + d.z * l.b.u.z,
          d.x * l.b.f.x + d.y * l.b.f.y + d.z * l.b.f.z};
}

inline void screen(const Lens& l, const cam::Vec3& c, double& x, double& y) {
  x = (0.5 + 0.5 * c.x / (c.z * l.b.tx)) * l.w;
  y = (0.5 - 0.5 * c.y / (c.z * l.b.ty)) * l.h;
}

// A world segment, clipped at the near plane.
inline void segment(Picture& pic, const Lens& l, cam::Vec3 a, cam::Vec3 b, Rgba c,
                    double thick = 1) {
  cam::Vec3 ca = to_cam(l, a), cb = to_cam(l, b);
  const double n = 0.05;
  if (ca.z < n && cb.z < n) return;
  if (ca.z < n || cb.z < n) {
    const double t = (n - ca.z) / (cb.z - ca.z);
    const cam::Vec3 m{ca.x + (cb.x - ca.x) * t, ca.y + (cb.y - ca.y) * t, n};
    (ca.z < n ? ca : cb) = m;
  }
  double x0, y0, x1, y1;
  screen(l, ca, x0, y0);
  screen(l, cb, x1, y1);
  const double lim = 4.0 * (l.w + l.h);
  if (std::fabs(x0) > lim || std::fabs(x1) > lim || std::fabs(y0) > lim || std::fabs(y1) > lim)
    return;
  draw_line(pic, x0, y0, x1, y1, c, thick);
}

// The lens's view of a scenario's world at a moment, `w` by `h`.
inline Picture sketch_view(const cam::Pose& pose, const cam::Scenario& sc, const cam::Subject& s,
                           int w, int h, const std::string& label) {
  Picture pic = create_picture(w, h, kSky);
  Lens l{pose, basis(pose, static_cast<double>(w) / h), w, h};
  // The horizon: the level directions, far off.
  {
    cam::Vec3 prev{};
    bool have = false;
    for (int k = 0; k <= 72; ++k) {
      const double a = k * ge::core::kTau / 72;
      const cam::Vec3 p{pose.eye.x + 1e4 * libm::sin(a), pose.eye.y,
                        pose.eye.z + 1e4 * libm::cos(a)};
      if (have) segment(pic, l, prev, p, kHorizon, 2);
      prev = p;
      have = true;
    }
  }
  // The ground: a grid every 2 m round the subject, bent over the ground.
  const double cx = std::floor(s.pos.x / 2) * 2, cz = std::floor(s.pos.z / 2) * 2;
  constexpr int kSpan = 40;
  for (int gi = -kSpan; gi <= kSpan; gi += 2) {
    for (int ti = -kSpan; ti < kSpan; ++ti) {
      const double g = gi, t = ti;
      const double x0 = cx + g, z0 = cz + t, z1 = cz + t + 1;
      segment(pic, l, {x0, cam::scenario_ground(sc, x0, z0), z0},
              {x0, cam::scenario_ground(sc, x0, z1), z1}, kGrid);
      const double xa = cx + t, xb = cx + t + 1, z = cz + g;
      segment(pic, l, {xa, cam::scenario_ground(sc, xa, z), z},
              {xb, cam::scenario_ground(sc, xb, z), z}, kGrid);
    }
  }
  // The trunks.
  const cam::World world = cam::scenario_world(sc);
  cam::Obstacle near[32];
  const int n = cam::obstacles_near(world, pose.eye.x, pose.eye.z, 80, near, 32);
  for (int i = 0; i < n; ++i) {
    const cam::Obstacle& o = near[i];
    const double g = cam::scenario_ground(sc, o.x, o.z);
    for (double dx : {-o.radius, o.radius}) {
      segment(pic, l, {o.x + dx, g, o.z}, {o.x + dx, g + 8, o.z}, kTrunk, 2);
    }
  }
  // The subject: legs, body, arms and a head, standing along its heading.
  {
    const double hx = libm::sin(s.heading), hz = libm::cos(s.heading);
    const double rx = hz, rz = -hx;
    const cam::Vec3 f = s.pos;
    const double tall = 1.75 - 0.6 * s.crouch;
    const cam::Vec3 hip{f.x, f.y + tall * 0.5, f.z};
    const cam::Vec3 neck{f.x, f.y + tall * 0.82, f.z};
    for (double side : {-0.15, 0.15}) {
      segment(pic, l, {f.x + rx * side, f.y, f.z + rz * side}, hip, kBody, 3);
      segment(pic, l, neck, {neck.x + rx * side * 3, neck.y - 0.45, neck.z + rz * side * 3}, kBody,
              2);
    }
    segment(pic, l, hip, neck, kBody, 4);
    const cam::Vec3 head{f.x, f.y + tall * 0.93, f.z};
    const cam::Vec3 c = to_cam(l, head);
    if (c.z > 0.05) {
      double x, y;
      screen(l, c, x, y);
      fill_disc(pic, x, y, 0.13 / (c.z * l.b.ty) * h * 0.5 + 1, kBody);
    }
    // The skis or the feet, along the heading.
    segment(pic, l, {f.x - hx * 0.8, f.y, f.z - hz * 0.8}, {f.x + hx * 0.8, f.y, f.z + hz * 0.8},
            kInk, 2);
  }
  // The frame's middle and the label.
  draw_line(pic, w * 0.5 - 4, h * 0.5, w * 0.5 + 4, h * 0.5, kInk);
  draw_line(pic, w * 0.5, h * 0.5 - 4, w * 0.5, h * 0.5 + 4, kInk);
  fill_rect(pic, 0, 0, text_width(label) + 6, kGlyphH + 4, kSnow);
  draw_text(pic, 3, 2, label, kInk);
  return pic;
}

// A sheet: `cells` laid out `cols` across, each `w` by `h`, a gap between.
inline Picture sheet(const std::vector<Picture>& cells, int cols, int w, int h) {
  const int rows = (static_cast<int>(cells.size()) + cols - 1) / cols;
  Picture out = create_picture(cols * (w + 4) + 4, rows * (h + 4) + 4, kInk);
  for (size_t i = 0; i < cells.size(); ++i) {
    blit(out, cells[i], 4 + static_cast<int>(i % cols) * (w + 4),
         4 + static_cast<int>(i / cols) * (h + 4));
  }
  return out;
}

inline std::string num(double v, int digits = 2) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

}  // namespace ge::lab::camera_sketch
