// Curves found in a brush level: an eight-sided pillar is one convex run all the way round, fitted
// to its centre and radius; a half-round arch built of wedge cells is one concave run with its
// centre on the arch's axis; and a plain box room has none.
//
// ge:summary Test: faceted pillars and arches found as curves and fitted
// ge:keywords test rooms curves arch pillar convex concave fit radius
// ge:skill rooms testing
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "check.hpp"
#include "rooms/brush_build.hpp"
#include "rooms/curves.hpp"
#include "rooms/text.hpp"

namespace {

namespace rm = ge::rooms;
constexpr double kPi = 3.14159265358979323846;

std::string face(double nx, double ny, double nz, double d, bool hidden = false) {
  char b[256];
  std::snprintf(b, sizeof b, "face %.9g,%.9g,%.9g %.9g stone s=32,0,0,0 t=0,0,32,0%s\n", nx + 0.0,
                ny + 0.0, nz + 0.0, d + 0.0, hidden ? " hidden" : "");
  return b;
}

std::string box(double x0, double y0, double z0, double x1, double y1, double z1) {
  return "brush solid\n" + face(1, 0, 0, x1) + face(-1, 0, 0, -x0) + face(0, 1, 0, y1) +
         face(0, -1, 0, -y0) + face(0, 0, 1, z1) + face(0, 0, -1, -z0) + "\n";
}

rm::Level parse(const std::string& body) {
  rm::Level level;
  std::string error;
  CHECK_MSG(rm::parse("material stone rgb=128,128,128 size=64,64\n\n" + body, &level, &error),
            error);
  return level;
}

}  // namespace

TEST(an_eight_sided_pillar_is_one_convex_run_round_its_axis) {
  // The pillar's sides are tangent to a circle of 0.5 m round (3, y, 2).
  std::string text = "brush solid\n";
  for (int k = 0; k < 8; ++k) {
    const double a = k * kPi / 4;
    const double nx = std::cos(a), nz = std::sin(a);
    text += face(nx, 0, nz, nx * 3 + nz * 2 + 0.5);
  }
  text += face(0, 1, 0, 3) + face(0, -1, 0, 0) + "\n";
  const rm::Level level = parse(text);
  const std::vector<rm::Curve> curves = rm::find_curves(level, rm::build_brushes(level, 1));
  CHECK(curves.size() == 1);
  if (curves.size() != 1) return;
  const rm::Curve& c = curves[0];
  CHECK(!c.concave);
  CHECK(c.facets == 8);
  CHECK(c.faces.size() == 8);
  CHECK_MSG(std::fabs(std::fabs(c.axis[1]) - 1) < 1e-9, "axis y " + std::to_string(c.axis[1]));
  CHECK_MSG(std::fabs(c.centre[0] - 3) < 1e-6 && std::fabs(c.centre[2] - 2) < 1e-6,
            "centre " + std::to_string(c.centre[0]) + "," + std::to_string(c.centre[2]));
  CHECK_MSG(std::fabs(c.radius - 0.5) < 1e-6, "radius " + std::to_string(c.radius));
  CHECK(c.sweep_deg == 360);
  CHECK(std::fabs(c.length - 3) < 1e-6);
}

TEST(a_half_round_arch_of_wedge_cells_is_one_concave_run) {
  // Six cells round the axis (0, 2, z), each the solid beyond one chord of a 1 m circle between
  // two radial planes and inside 2 m: the soffit of a round arch 1 m deep.
  std::string text;
  const double cy = 2;
  for (int k = 0; k < 6; ++k) {
    const double a0 = k * kPi / 6, a1 = (k + 1) * kPi / 6, am = (a0 + a1) / 2;
    const double ux = std::cos(am), uy = std::sin(am);
    const double apothem = std::cos(kPi / 12);
    text += "brush solid\n";
    text += face(-ux, -uy, 0, -(uy * cy) - apothem);                         // beyond the chord
    text += face(std::sin(a0), -std::cos(a0), 0, -std::cos(a0) * cy, true);  // past a0
    text += face(-std::sin(a1), std::cos(a1), 0, std::cos(a1) * cy, true);   // before a1
    text += face(ux, uy, 0, uy * cy + 2, true);                              // inside 2 m
    text += face(0, 0, 1, 1, true) + face(0, 0, -1, 0, true) + "\n";
  }
  const rm::Level level = parse(text);
  const std::vector<rm::Curve> curves = rm::find_curves(level, rm::build_brushes(level, 1));
  CHECK(curves.size() == 1);
  if (curves.size() != 1) return;
  const rm::Curve& c = curves[0];
  CHECK(c.concave);
  CHECK(c.facets == 6);
  CHECK_MSG(std::fabs(std::fabs(c.axis[2]) - 1) < 1e-9, "axis z " + std::to_string(c.axis[2]));
  CHECK_MSG(std::fabs(c.centre[0]) < 1e-6 && std::fabs(c.centre[1] - 2) < 1e-6,
            "centre " + std::to_string(c.centre[0]) + "," + std::to_string(c.centre[1]));
  CHECK_MSG(std::fabs(c.radius - std::cos(kPi / 12)) < 1e-6, "radius " + std::to_string(c.radius));
  CHECK_MSG(std::fabs(c.sweep_deg - 150) < 1e-6, "sweep " + std::to_string(c.sweep_deg));
}

TEST(a_plain_box_room_has_no_curves) {
  const std::string text = box(-0.5, -0.5, -0.5, 8.5, 0, 8.5) + box(-0.5, 0, -0.5, 0, 3, 8.5) +
                           box(8, 0, -0.5, 8.5, 3, 8.5) + box(0, 0, -0.5, 8, 3, 0) +
                           box(0, 0, 8, 8, 3, 8.5) + box(-0.5, 3, -0.5, 8.5, 3.5, 8.5);
  const rm::Level level = parse(text);
  CHECK(rm::find_curves(level, rm::build_brushes(level, 1)).empty());
}

TEST(a_pillar_drawn_round_lies_on_its_circle_and_its_caps_gap_is_filled) {
  std::string text = "brush solid\n";
  for (int k = 0; k < 8; ++k) {
    const double a = k * kPi / 4;
    const double nx = std::cos(a), nz = std::sin(a);
    text += face(nx, 0, nz, nx * 3 + nz * 2 + 0.5);
  }
  text += face(0, 1, 0, 3) + face(0, -1, 0, 0) + "\n";
  const rm::Level level = parse(text);
  const std::vector<rm::Curve> curves = rm::find_curves(level, rm::build_brushes(level, 1));
  CHECK(curves.size() == 1);
  if (curves.size() != 1) return;
  const rm::BrushMesh flat = rm::build_brushes(level, 1);
  const rm::BrushMesh round = rm::round_curves(level, flat, curves, 7.5);
  // 45 degree facets cut every 7.5 degrees: six pieces a facet where there was one.
  CHECK_MSG(round.triangles() > flat.triangles() * 4,
            std::to_string(flat.triangles()) + " -> " + std::to_string(round.triangles()));
  const double corner = 0.5 / std::cos(kPi / 8);
  double worst = 0;
  size_t surface = 0, fills = 0;
  for (size_t t = 0; t < round.triangles(); ++t) {
    if (level.faces[round.face[t]].n[1] != 0) continue;  // the caps stay flat
    // The round surface's triangles are shaded with the cylinder's normal, out of the axis; the
    // strips that fill the gap at a cap are not.
    bool radial = true;
    for (size_t k = 0; k < 3; ++k) {
      const size_t v = round.idx[t * 3 + k];
      const double dx = round.pos[v * 3] - 3, dz = round.pos[v * 3 + 2] - 2;
      const double r = std::sqrt(dx * dx + dz * dz);
      radial =
          radial && std::fabs(round.nrm[v * 3 + 1]) < 1e-6 &&
          std::fabs(round.nrm[v * 3] - dx / r) + std::fabs(round.nrm[v * 3 + 2] - dz / r) < 1e-4;
    }
    if (!radial) {
      ++fills;
      continue;
    }
    ++surface;
    for (size_t k = 0; k < 3; ++k) {
      const size_t v = round.idx[t * 3 + k];
      const double dx = round.pos[v * 3] - 3, dz = round.pos[v * 3 + 2] - 2;
      worst = std::max(worst, std::fabs(std::sqrt(dx * dx + dz * dz) - corner));
    }
  }
  CHECK_MSG(surface >= 8 * 6 * 2, "surface " + std::to_string(surface));
  CHECK_MSG(fills > 0, "no strip fills the gap at the caps");
  CHECK_MSG(worst < 1e-5, "off the circle by " + std::to_string(worst));
}

TEST(an_arch_drawn_round_lies_on_its_circle_facing_the_opening_its_ends_filled) {
  std::string text;
  const double cy = 2;
  for (int k = 0; k < 6; ++k) {
    const double a0 = k * kPi / 6, a1 = (k + 1) * kPi / 6, am = (a0 + a1) / 2;
    const double ux = std::cos(am), uy = std::sin(am);
    const double apothem = std::cos(kPi / 12);
    text += "brush solid\n";
    text += face(-ux, -uy, 0, -(uy * cy) - apothem);
    text += face(std::sin(a0), -std::cos(a0), 0, -std::cos(a0) * cy, true);
    text += face(-std::sin(a1), std::cos(a1), 0, std::cos(a1) * cy, true);
    text += face(ux, uy, 0, uy * cy + 2, true);
    text += face(0, 0, 1, 1, true) + face(0, 0, -1, 0, true) + "\n";
  }
  const rm::Level level = parse(text);
  const std::vector<rm::Curve> curves = rm::find_curves(level, rm::build_brushes(level, 1));
  CHECK(curves.size() == 1);
  if (curves.size() != 1) return;
  const rm::BrushMesh flat = rm::build_brushes(level, 1);
  const rm::BrushMesh round = rm::round_curves(level, flat, curves, 7.5);
  size_t drawn = 0, fills = 0;
  double worst = 0;
  for (size_t t = 0; t < round.triangles(); ++t) {
    if (round.hidden[t]) continue;
    // The soffit's triangles face the axis (into the opening); the strips that fill the gap at
    // its ends and springings do not.
    bool facing = true;
    for (size_t k = 0; k < 3; ++k) {
      const size_t v = round.idx[t * 3 + k];
      const double dx = round.pos[v * 3], dy = round.pos[v * 3 + 1] - 2;
      const double r = std::sqrt(dx * dx + dy * dy);
      facing = facing && -(round.nrm[v * 3] * dx + round.nrm[v * 3 + 1] * dy) / r > 0.9999;
    }
    if (!facing) {
      ++fills;
      continue;
    }
    ++drawn;
    for (size_t k = 0; k < 3; ++k) {
      const size_t v = round.idx[t * 3 + k];
      const double dx = round.pos[v * 3], dy = round.pos[v * 3 + 1] - 2;
      worst = std::max(worst, std::fabs(std::sqrt(dx * dx + dy * dy) - 1));
    }
  }
  CHECK_MSG(drawn >= 12, "drawn " + std::to_string(drawn));
  CHECK_MSG(worst < 1e-5, "off the circle by " + std::to_string(worst));
  CHECK_MSG(fills > 0, "no strip fills the gap at the ends");
}
