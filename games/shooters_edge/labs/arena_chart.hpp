// What Shooter's Edge's labs draw the test arena with (the `se-loop` and `mod` labs): a brush's
// bounds, a rectangle and a path as chart traces in two of the map's axes (0 x, 1 y, 2 z), each
// broken off from the next by a NaN, and a chart kept as a captioned shot with its SVG beside it.
// Header-only, beside the labs (a lab's helper never goes into a library: the labs skill).
//
// ge:summary The arena charts' helpers the Shooter's Edge labs share: bounds, rectangles, paths
// ge:keywords lab arena chart brush-bounds rect path shoot svg trace
// ge:skill shooter labs
// ge:test tests/shooters_edge_match_test.cpp
#pragma once

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooter/brush_hull.hpp"

namespace shooters_edge::labs {

namespace gl = ge::lab;
namespace sh = ge::shooter;

inline constexpr double kNan = std::numeric_limits<double>::quiet_NaN();

// A box brush's bounds off its axial planes.
inline void brush_bounds(const sh::Brush& b, sh::Vec3f& lo, sh::Vec3f& hi) {
  for (const sh::Plane& p : b.planes)
    for (int i = 0; i < 3; i++) {
      if (p.normal[i] == 1) hi[i] = p.dist;
      if (p.normal[i] == -1) lo[i] = -p.dist;
    }
}

// A rectangle in the chart's two axes, closed and broken off from the next.
inline void rect(gl::Trace& t, sh::Vec3f lo, sh::Vec3f hi, int a, int b) {
  const double xs[5] = {lo[a], hi[a], hi[a], lo[a], lo[a]};
  const double ys[5] = {lo[b], lo[b], hi[b], hi[b], lo[b]};
  for (int i = 0; i < 5; i++) {
    t.x.push_back(xs[i]);
    t.y.push_back(ys[i]);
  }
  t.x.push_back(lo[a]);
  t.y.push_back(kNan);
}

inline void path(gl::Trace& t, const std::vector<sh::Vec3f>& pts, int a, int b) {
  for (const sh::Vec3f& p : pts) {
    t.x.push_back(p[a]);
    t.y.push_back(p[b]);
  }
  if (!pts.empty()) {
    t.x.push_back(pts.back()[a]);
    t.y.push_back(kNan);
  }
}

inline void shoot(gl::Lab& lab, const std::string& id, const gl::Chart& chart,
                  const std::string& caption, const std::string& look_for,
                  const std::vector<std::pair<std::string, std::string>>& in) {
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({id, chart.title, caption, look_for, in}, gl::to_picture(chart.plot));
}

}  // namespace shooters_edge::labs
