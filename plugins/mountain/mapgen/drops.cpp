// Port of game5 `engine/mapgen/drops.ts`.
//
// ge:summary The drops' placement and profile, off their own stream (none on v1)
// ge:keywords drop layDrops publishDrops
// ge:oracle game5/engine/mapgen/drops.ts
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#include "mountain/mapgen/drops.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

#include "core/js.hpp"
#include "core/math.hpp"
#include "core/prng.hpp"
#include "mountain/mapgen/cliffs.hpp"
#include "mountain/mapgen/spawn.hpp"

namespace ge::mountain::mapgen {

namespace js = ge::core::js;
namespace R = rules;

namespace {

// Salt on the attempt's sub-seed for the drops' own stream.
constexpr uint32_t kDropSalt = 0x0d7095e;

// How many stands a drop is tried at before the dealer settles for those it has.
constexpr int kTries = 90;

// How much steeper than R8's floor the line must still fall over a shelf's steepest rise.
constexpr double kShelfSlack = 0.04;

}  // namespace

std::vector<TrackDrop> lay_drops(uint32_t sub, Piste& piste, const GradeRow& grade) {
  if (grade.drops.max <= 0) return {};
  namespace D = R::drop;
  ge::core::Rng rng = ge::core::make_rng(sub ^ kDropSalt);
  const int want = ge::core::int_in(rng, int32_t(grade.drops.min), int32_t(grade.drops.max));
  auto& pts = piste.track.points;
  const auto n = static_cast<long>(pts.size());
  const double step = piste.track.length / double(n - 1);
  const std::vector<double> gates = gate_arcs(piste.track.length);
  const double lo = start_gate_arc() + R::spawn::kicker_gap + D::shelf.max;
  const double hi = finish_from(piste.track.length) - D::finish_clear;
  const auto w = static_cast<long>(js::max(1.0, js::round(R::track::grade_window / step)));
  // The least fall over a grade window between two arcs, m per m.
  const auto least_fall = [&](double a, double b) {
    double least = std::numeric_limits<double>::infinity();
    const long i0 = long(js::max(0.0, std::floor(a / step)));
    const long i1 = long(js::min(double(n - 1 - w), std::ceil(b / step)));
    for (long i = i0; i <= i1; i++) {
      least = js::min(least, (pts[size_t(i)].y - pts[size_t(i + w)].y) / (double(w) * step));
    }
    return least;
  };
  std::vector<TrackDrop> chosen;
  for (int t = 0; t < want * kTries && int(chosen.size()) < want && hi > lo; t++) {
    const auto index = static_cast<size_t>(js::round(ge::core::range(rng, lo, hi) / step));
    const double drop = in_band(rng, D::drop);
    const double s0 = pts[index].s;
    const double apron = 0;
    const double face = drop * R::cliff::face;
    const double landing = D::landing;
    const double top = drop;
    // A PITCH under it, and a shelf long enough that its steepest rise leaves the line still
    // falling: sized off the pitch under the shortest shelf, then the pitch read again.
    const auto shelf_for = [&](double fall) {
      return js::max(D::shelf.min,
                     (1.5 * top) / js::max(1e-3, fall - R::track::min_grade - kShelfSlack));
    };
    double fall = least_fall(s0 - D::shelf.min, s0 + face + landing);
    if (fall < D::min_fall) continue;
    double shelf = shelf_for(fall);
    if (shelf > D::shelf.min) {
      fall = least_fall(s0 - shelf, s0 + face + landing);
      shelf = shelf_for(fall);
    }
    if (fall < D::min_fall || shelf > D::shelf.max) continue;
    const double from = s0 - shelf;
    const double to = s0 + face + landing;
    // Straight over the whole of it.
    double lo2 = 0;
    double hi2 = 0;
    double turned = 0;
    for (long i = long(std::floor(from / step)); double(i) < std::ceil(to / step) && i + 1 < n;
         i++) {
      turned += ge::core::angle_diff(pts[size_t(i)].heading, pts[size_t(i + 1)].heading);
      lo2 = js::min(lo2, turned);
      hi2 = js::max(hi2, turned);
    }
    if (hi2 - lo2 > D::straight) continue;
    // Off the gates and the other drops.
    if (std::any_of(gates.begin(), gates.end(),
                    [&](double g) { return g > from - D::gate_clear && g < to + D::gate_clear; })) {
      continue;
    }
    if (std::any_of(chosen.begin(), chosen.end(), [&](const TrackDrop& d) {
          return std::abs(pts[d.index].s - s0) < D::spacing;
        })) {
      continue;
    }
    chosen.push_back({index, drop, shelf, face, apron, landing});
  }
  std::stable_sort(chosen.begin(), chosen.end(),
                   [](const TrackDrop& a, const TrackDrop& b) { return a.index < b.index; });
  for (const TrackDrop& d : chosen) {
    const double s0 = pts[d.index].s;
    const long i0 = long(std::floor((s0 - d.shelf) / step));
    const long i1 = long(std::ceil((s0 + d.face + d.landing) / step));
    for (long i = i0; i <= i1; i++) {
      if (i < 0 || i >= n) continue;  // `if (p)`
      TrackPoint& p = pts[size_t(i)];
      p.y += cliff_profile(d.drop, d.shelf, d.face, d.apron, d.landing, p.s - s0);
    }
  }
  return chosen;
}

std::vector<Cliff> publish_drops(const Piste& piste, const std::vector<TrackDrop>& drops) {
  std::vector<Cliff> out;
  for (size_t k = 0; k < drops.size(); k++) {
    const TrackDrop& d = drops[k];
    const TrackPoint& p = piste.track.points[d.index];
    out.push_back(Cliff{"D" + std::to_string(k + 1), p.x, p.z, p.y, p.heading, d.drop, d.face,
                        d.apron, d.landing, d.shelf, p.width, true, p.s});
  }
  return out;
}

}  // namespace ge::mountain::mapgen
