// The ski-area lab helpers: the build under the lab's flags, and the painter.
//
// ge:summary The ski-area labs' build-from-flags and the from-above area painter
// ge:keywords lab resort draw-area build-area courses plan
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#include "mountain/labs/resort_map.hpp"

#include <algorithm>
#include <cmath>

#include "core/heightfield.hpp"
#include "core/libm/libm.hpp"
#include "mountain/labs/level_map.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/mapgen/regions.hpp"

namespace ge::mountain::labs {

using ge::core::sample_field;
using ge::lab::Rgba;

namespace {

// The generator version the ski area is built by in this tree (the routes of v9 and on wait for
// docs/goals.md M4c).
constexpr int kAreaVersion = 8;

constexpr Rgba kRoad{110, 110, 110};
constexpr Rgba kLift{20, 20, 20};
constexpr Rgba kTunnel{0, 170, 190};
constexpr Rgba kKicker{240, 140, 20};
constexpr Rgba kCliff{120, 40, 150};
constexpr Rgba kHub{230, 200, 40};
constexpr Rgba kTree{28, 72, 40, 220};

uint8_t mix(double a, double b, double t) { return static_cast<uint8_t>(a + (b - a) * t); }

// The ground shaded by height and by its facing to a light from the upper left, the packed snow
// tinted blue.
void shade_ground(ge::lab::Picture& pic, const mapgen::ResortAttempt& a, double k) {
  const ge::core::Heightfield& g = a.ground;
  double lo = g.data.empty() ? 0 : g.data[0];
  double hi = lo;
  for (const float h : g.data) {
    lo = std::min(lo, static_cast<double>(h));
    hi = std::max(hi, static_cast<double>(h));
  }
  const double span = hi > lo ? hi - lo : 1;
  for (int py = 0; py < pic.height; py++) {
    for (int px = 0; px < pic.width; px++) {
      const double x = (px + 0.5) / k;
      const double z = (py + 0.5) / k;
      const double t = (sample_field(g, x, z) - lo) / span;
      const auto s = ge::core::field_gradient(g, x, z);
      const double nl = std::sqrt(s.gx * s.gx + 1 + s.gz * s.gz);
      const double lit = std::clamp((s.gx + 1.41421356 + s.gz) / (2 * nl), 0.0, 1.0);
      const double shade = 0.45 + 0.55 * lit;
      const double packed = std::clamp(sample_field(a.packed, x, z), 0.0, 1.0);
      ge::lab::set_pixel(pic, px, py,
                         {static_cast<uint8_t>(mix(mix(118, 250, t), 150, packed * 0.5) * shade),
                          static_cast<uint8_t>(mix(mix(130, 250, t), 190, packed * 0.5) * shade),
                          static_cast<uint8_t>(mix(mix(120, 255, t), 255, packed * 0.5) * shade)});
    }
  }
}

void polyline(ge::lab::Picture& pic, const std::vector<mapgen::TrackPoint>& pts, double k, Rgba c,
              double thick) {
  for (size_t i = 1; i < pts.size(); i++) {
    ge::lab::draw_line(pic, pts[i - 1].x * k, pts[i - 1].z * k, pts[i].x * k, pts[i].z * k, c,
                       thick);
  }
}

}  // namespace

Rgba grade_colour(mapgen::PisteGrade grade) {
  switch (grade) {
    case mapgen::PisteGrade::green:
      return {30, 160, 60};
    case mapgen::PisteGrade::blue:
      return {30, 70, 220};
    case mapgen::PisteGrade::red:
      return {220, 30, 30};
    case mapgen::PisteGrade::black:
      return {0, 0, 0};
  }
  return {0, 0, 0};
}

std::vector<AttemptTried> build_area(ge::lab::Lab& lab, double seed, mapgen::ResortAttempt& out) {
  std::vector<AttemptTried> tried;
  const auto region = region_named(lab.arg("region"));
  const std::string seed_word = std::to_string(static_cast<long long>(seed));
  if (!region) {
    lab.fail("--region=" + lab.arg("region") + " is not a region",
             "Use alpine, fell, continental or maritime (mapgen/regions.hpp).");
    return tried;
  }
  const int attempts = static_cast<int>(lab.arg_num("attempts"));
  for (int i = 0; i < attempts; i++) {
    out = mapgen::begin_resort(seed, i, mapgen::sub_seed(seed, i), mapgen::region_row(region),
                               kAreaVersion);
    std::string reached;
    const mapgen::StageResult why = mapgen::build_resort(out, reached);
    tried.push_back({i, reached, why.value_or("")});
    if (!why) return tried;
  }
  lab.fail("seed " + seed_word + " built no area in " + std::to_string(attempts) + " attempts",
           "Read the refusals table, or try --attempts=" + std::to_string(attempts * 2) +
               ". Skill: mountain.");
  return tried;
}

std::vector<std::pair<std::string, std::string>> area_inputs(const ge::lab::Lab& lab,
                                                             const mapgen::ResortAttempt& a) {
  return {{"seed", std::to_string(static_cast<long long>(a.seed))},
          {"version", std::to_string(a.version)},
          {"region", lab.arg("region")},
          {"attempt", std::to_string(a.attempt)}};
}

ge::lab::Picture draw_area(const mapgen::ResortAttempt& a, int px, bool courses) {
  const double k = px / a.plan.size;  // pixels a metre
  auto pic = ge::lab::create_picture(px, px, {0, 0, 0});
  shade_ground(pic, a, k);
  for (const mapgen::TreeDef& t : a.trees) {
    ge::lab::fill_disc(pic, t.x * k, t.z * k, std::max(0.6, t.crown * k), kTree);
  }
  // The hub's edges, column by column.
  const mapgen::Hub& hub = a.hub.hub;
  for (size_t i = 1; i < hub.top.size(); i++) {
    const double x0 = (hub.x0 + hub.step * double(i - 1)) * k;
    const double x1 = (hub.x0 + hub.step * double(i)) * k;
    ge::lab::draw_line(pic, x0, hub.top[i - 1] * k, x1, hub.top[i] * k, kHub);
    ge::lab::draw_line(pic, x0, hub.bottom[i - 1] * k, x1, hub.bottom[i] * k, kHub);
  }
  for (const mapgen::WindTunnel& t : a.tunnels) {
    for (size_t i = 1; i < t.points.size(); i++) {
      ge::lab::draw_line(pic, t.points[i - 1].x * k, t.points[i - 1].z * k, t.points[i].x * k,
                         t.points[i].z * k, kTunnel, 2);
    }
  }
  const double thick = px >= 768 ? 2 : 1;
  for (const mapgen::BuiltRun& b : a.built) {
    const bool road = b.walked.spec.kind == mapgen::RunKind::road;
    const Rgba c = courses ? Rgba{255, 255, 255, 150} : road ? kRoad : grade_colour(b.grade);
    polyline(pic, b.walked.track.points, k, c, road ? 1 : thick);
  }
  if (courses) {
    // Every course in the colour it measures, numbered at its top by the run it starts on.
    for (size_t i = 0; i < a.courses.size(); i++) {
      const Rgba c = grade_colour(a.courses[i].grade);
      const auto& pts = a.course_plans[i].points;
      polyline(pic, pts, k, c, thick + 1);
      ge::lab::draw_text(pic, pts.front().x * k + 4, pts.front().z * k - 4, a.courses[i].id, c);
    }
  } else {
    for (const mapgen::BuiltRun& b : a.built) {
      if (b.walked.spec.kind == mapgen::RunKind::road) continue;
      const mapgen::TrackPoint& p = b.walked.track.points.front();
      ge::lab::draw_text(pic, p.x * k + 4, p.z * k + 2, b.walked.spec.id, grade_colour(b.grade));
      for (const mapgen::Kicker& kk : b.kickers) {
        ge::lab::fill_disc(pic, kk.x * k, kk.z * k, 2, kKicker);
      }
    }
    for (const mapgen::Kicker& kk : a.off_kickers) {
      ge::lab::fill_disc(pic, kk.x * k, kk.z * k, 3, kKicker);
      ge::lab::draw_text(pic, kk.x * k + 5, kk.z * k - 3, kk.id, {150, 70, 0});
    }
    for (const mapgen::Cliff& c : a.cliffs) {
      const double dx = ge::core::libm::cos(c.heading);
      const double dz = -ge::core::libm::sin(c.heading);
      const double h = c.width / 2;
      ge::lab::draw_line(pic, (c.x - dx * h) * k, (c.z - dz * h) * k, (c.x + dx * h) * k,
                         (c.z + dz * h) * k, kCliff, 2);
      ge::lab::draw_text(pic, c.x * k + 4, c.z * k - 4, c.id, kCliff);
    }
  }
  for (const mapgen::LiftPlan& l : a.lift_plans) {
    ge::lab::draw_line(pic, l.bottom.x * k, l.bottom.z * k, l.top.x * k, l.top.z * k, kLift, 1);
    ge::lab::fill_rect(pic, l.bottom.x * k - 2, l.bottom.z * k - 2, 5, 5, kLift);
    ge::lab::fill_rect(pic, l.top.x * k - 2, l.top.z * k - 2, 5, 5, kLift);
    ge::lab::draw_text(pic, l.top.x * k + 5, l.top.z * k - 9, l.id, kLift);
  }
  ge::lab::fill_disc(pic, a.village.x * k, a.village.z * k, 4, {200, 60, 160});
  return pic;
}

}  // namespace ge::mountain::labs
