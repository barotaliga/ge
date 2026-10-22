// The map drawn from above (map_picture.hpp).
//
// ge:summary draw_map's definition: the shaded face, the woods, the piste, cliffs, kickers, gates
// ge:keywords map picture draw_map hillshade light packed gates
// ge:skill mountain
// ge:test tests/mountain_map_picture_test.cpp
#include "mountain/picture/map_picture.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "core/heightfield.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::mountain::picture {

using ge::lab::Rgba;

namespace {

uint8_t mix(double a, double b, double t) { return static_cast<uint8_t>(a + (b - a) * t); }

}  // namespace

ge::lab::Picture draw_map(const mapgen::Level& level, int px) {
  return draw_map(level, px, MapWindow{level.size / 2, level.size / 2, level.size});
}

ge::lab::Picture draw_map(const mapgen::Level& level, int px, const MapWindow& window) {
  const double k = px / window.metres;  // pixels a metre
  // A map point to the picture: (x - x0) * k across, (z - z0) * k down.
  const double x0 = window.x - window.metres / 2, z0 = window.z - window.metres / 2;
  const auto sx = [&](double x) { return (x - x0) * k; };
  const auto sz = [&](double z) { return (z - z0) * k; };
  auto pic = ge::lab::create_picture(px, px, {0, 0, 0});
  const ge::core::Heightfield& g = level.ground;

  // The face: height to colour (the valley's grey-green to the summit's white), shaded by the
  // slope's facing to a light from the upper left, and the packed snow tinted blue.
  double lo = g.data.empty() ? 0 : g.data[0], hi = lo;
  for (const float h : g.data) {
    lo = std::min(lo, static_cast<double>(h));
    hi = std::max(hi, static_cast<double>(h));
  }
  const double span = hi > lo ? hi - lo : 1;
  for (int py = 0; py < px; py++) {
    for (int pxx = 0; pxx < px; pxx++) {
      const double x = x0 + (pxx + 0.5) / k, z = z0 + (py + 0.5) / k;
      const double t = (level.ground_at(x, z) - lo) / span;
      const auto s = ge::core::field_gradient(g, x, z);
      // Light from -x, -z, 45 degrees up: the normal (-gx, 1, -gz) against (-1, 1.41, -1)/2.
      const double nl = std::sqrt(s.gx * s.gx + 1 + s.gz * s.gz);
      const double lit = std::clamp((s.gx + 1.41421356 + s.gz) / (2 * nl), 0.0, 1.0);
      const double shade = 0.45 + 0.55 * lit;
      const double packed = std::clamp(level.packed_at(x, z), 0.0, 1.0);
      const double r = mix(mix(118, 250, t), 150, packed * 0.5) * shade;
      const double gg = mix(mix(130, 250, t), 190, packed * 0.5) * shade;
      const double b = mix(mix(120, 255, t), 255, packed * 0.5) * shade;
      ge::lab::set_pixel(
          pic, pxx, py,
          {static_cast<uint8_t>(r), static_cast<uint8_t>(gg), static_cast<uint8_t>(b)});
    }
  }

  for (const mapgen::TreeDef& tree : level.trees) {
    ge::lab::fill_disc(pic, sx(tree.x), sz(tree.z), std::max(0.7, tree.crown * k),
                       {28, 72, 40, 220});
  }

  const auto& pts = level.track.points;
  for (size_t i = 1; i < pts.size(); i++) {
    ge::lab::draw_line(pic, sx(pts[i - 1].x), sz(pts[i - 1].z), sx(pts[i].x), sz(pts[i].z),
                       {200, 40, 40}, px >= 768 ? 2 : 1);
  }

  for (const mapgen::Cliff& c : level.cliffs) {
    const double dx = ge::core::libm::cos(c.heading), dz = -ge::core::libm::sin(c.heading);
    const double h = c.width / 2;
    ge::lab::draw_line(pic, sx(c.x - dx * h), sz(c.z - dz * h), sx(c.x + dx * h), sz(c.z + dz * h),
                       {120, 40, 150}, 2);
    ge::lab::draw_text(pic, sx(c.x) + 4, sz(c.z) - 4, c.id, {120, 40, 150});
  }
  for (const mapgen::Kicker& kk : level.kickers) {
    ge::lab::fill_disc(pic, sx(kk.x), sz(kk.z), 3, {240, 140, 20});
    ge::lab::draw_text(pic, sx(kk.x) + 5, sz(kk.z) - 3, kk.id, {150, 70, 0});
  }

  // Gates: a bar across the piste at its width (heading 0 = +z, clockwise from above, so the
  // way across is (cos h, -sin h)), numbered; the start gate green, the finish black.
  for (size_t i = 0; i < level.checkpoints.size(); i++) {
    const mapgen::Checkpoint& cp = level.checkpoints[i];
    const double ax = ge::core::libm::cos(cp.heading), az = -ge::core::libm::sin(cp.heading);
    const double h = cp.width / 2;
    const bool first = i == 0, last = i + 1 == level.checkpoints.size();
    const Rgba c =
        first  ? Rgba{30, 160, 60}
        : last ? Rgba{0, 0, 0}
               : (cp.colour == mapgen::GateColour::red ? Rgba{220, 30, 30} : Rgba{30, 70, 220});
    ge::lab::draw_line(pic, sx(cp.x - ax * h), sz(cp.z - az * h), sx(cp.x + ax * h),
                       sz(cp.z + az * h), c, 2);
    const std::string label = first ? "START" : last ? "FINISH" : std::to_string(i);
    ge::lab::draw_text(pic, sx(cp.x + ax * h) + 3, sz(cp.z) - 3, label, c);
  }
  ge::lab::fill_disc(pic, sx(level.spawn.x), sz(level.spawn.z), 3, {30, 160, 60});
  return pic;
}

}  // namespace ge::mountain::picture
