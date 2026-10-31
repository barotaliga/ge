// What the classic move's labs (`pmove`, `framemove`) draw their scenes with: a scenario name in
// capitals, a comma list's membership, a brush cut by the plane y = 0 as a chart outline, and a
// chart kept as a captioned shot with its SVG beside it. Header-only, beside the labs (a lab's
// helper never goes into a library: the labs skill).
//
// ge:summary The classic move labs' scene charts: names, lists, brush outlines at y 0, shots
// ge:keywords lab scene chart brush-outline side-view shoot svg upper listed
// ge:skill shooter labs
// ge:test tests/shooter_recording_test.cpp
#pragma once

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooter/brush_hull.hpp"

namespace ge::shooter::labs {

namespace gl = ge::lab;
namespace sh = ge::shooter;

inline constexpr double kNan = std::numeric_limits<double>::quiet_NaN();

inline std::string upper(std::string s) {
  for (char& c : s)
    if (c >= 'a' && c <= 'z')
      c = static_cast<char>(c - 32);
    else if (c == '_')
      c = ' ';
  return s;
}

inline bool listed(const std::string& list, const std::string& name) {
  if (list == "all") return true;
  size_t i = 0;
  while (i <= list.size()) {
    size_t j = list.find(',', i);
    if (j == std::string::npos) j = list.size();
    if (list.compare(i, j - i, name) == 0) return true;
    i = j + 1;
  }
  return false;
}

// A brush cut by the plane y = 0, as a closed outline in (x, z): a big square clipped by each of
// the brush's planes that has an x or z part, then clamped to the window.
inline void brush_outline(const sh::Brush& b, double x0, double x1, double z0, double z1,
                          gl::Trace& t) {
  std::vector<std::pair<double, double>> poly = {{x0, z0}, {x1, z0}, {x1, z1}, {x0, z1}};
  for (const sh::Plane& p : b.planes) {
    const double nx = p.normal.x, nz = p.normal.z, d = p.dist;
    if (nx == 0 && nz == 0) {
      if (d < 0) poly.clear();  // y = 0 is outside the brush
      continue;
    }
    std::vector<std::pair<double, double>> out;
    for (size_t i = 0; i < poly.size(); i++) {
      const auto& a = poly[i];
      const auto& c = poly[(i + 1) % poly.size()];
      const double da = nx * a.first + nz * a.second - d;
      const double dc = nx * c.first + nz * c.second - d;
      if (da <= 0) out.push_back(a);
      if ((da < 0 && dc > 0) || (da > 0 && dc < 0)) {
        const double f = da / (da - dc);
        out.emplace_back(a.first + f * (c.first - a.first), a.second + f * (c.second - a.second));
      }
    }
    poly = out;
  }
  if (poly.size() < 3) return;
  for (const auto& q : poly) {
    t.x.push_back(q.first);
    t.y.push_back(q.second);
  }
  t.x.push_back(poly.front().first);
  t.y.push_back(poly.front().second);
  t.x.push_back(poly.front().first);
  t.y.push_back(kNan);
}

inline void shoot(gl::Lab& lab, const std::string& id, const gl::Chart& chart,
                  const std::string& caption, const std::string& look_for,
                  const std::string& scenario) {
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({id,
            chart.title,
            caption,
            look_for,
            {{"scenario", scenario}, {"recording", lab.arg("recording")}}},
           gl::to_picture(chart.plot));
}

}  // namespace ge::shooter::labs
