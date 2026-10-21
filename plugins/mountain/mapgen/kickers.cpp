// The definitions behind kickers.hpp.
//
// ge:summary Places and stamps the kickers on the piste and off it
// ge:keywords kicker
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#include "mountain/mapgen/kickers.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/query.hpp"
#include "mountain/mapgen/regions.hpp"
#include "mountain/mapgen/spawn.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace R = rules;
using ge::core::angle_diff;
using ge::core::clamp;
using ge::core::hypot;
using ge::core::sample_field;
using ge::core::smoothstep;

double kicker_profile(double height, double ramp, double landing, double u) {
  if (u <= -ramp || u >= landing) return 0;
  if (u <= 0) {
    const double t = 1 + u / ramp;
    return height * t * t;
  }
  const double t = 1 - u / landing;
  return height * t * t;
}

std::vector<TrackKicker> lay_track_kickers(ge::core::Rng& rng, Piste& piste, const GradeRow& grade,
                                           std::span<const TrackDrop> drops) {
  namespace K = R::kickers::on;
  auto& pts = piste.track.points;
  const auto n = static_cast<long>(pts.size());
  const double step = R::track::step;
  const auto& G = grade.kickers;
  // How many this map carries, dealt in the grade's band.
  const int want = ge::core::int_in(rng, int32_t(G.on.min), int32_t(G.on.max));
  // Every station a kicker could stand at, scored; then the best that keep their spacing, with
  // a little noise in the score so two equally good rolls are not always picked the same way.
  struct Candidate {
    long index;
    double score;
    double fall;
  };
  std::vector<Candidate> candidates;
  // One shape a map: the ramp and the landing as multiples of the lip's height, and the lip's
  // height read off each stand's own pitch. The stands are scored at the tallest lip's reach.
  const double ramp_of = in_band(rng, K::ramp);
  const double landing_of = in_band(rng, K::landing);
  const auto height_for = [&](double fall) {
    return (K::height.min +
            (K::height.max - K::height.min) *
                clamp((fall - K::pitch.min) / (K::pitch.max - K::pitch.min), 0, 1)) *
           G.height;
  };
  const double ramp = K::height.max * G.height * ramp_of;
  const double landing = K::height.max * G.height * landing_of;
  const auto back = static_cast<long>(std::ceil((ramp + 10) / step));
  const auto ahead = static_cast<long>(std::ceil((landing + 10) / step));
  const auto first =
      static_cast<long>(std::ceil((R::grid::back + step + R::spawn::kicker_gap) / step));
  const long last =
      n - 1 - static_cast<long>(std::ceil((R::track::finish + R::track::runout + landing) / step));
  const auto ramp_steps = static_cast<long>(num::round(ramp / step));
  const auto landing_steps = static_cast<long>(num::round(landing / step));
  for (long i = first; i < last; i += 2) {
    if (i - back < 0 || i + ahead >= n) continue;
    // A roll: the line comes down to the lip gently and falls away past it at least as steeply,
    // read over the tallest lip's reach …
    const double y_lip = pts[size_t(i)].y;
    const double y_foot = pts[size_t(i - ramp_steps)].y;
    const double y_end = pts[size_t(i + landing_steps)].y;
    const double rise = (y_foot - y_lip) / ramp;
    const double fall = (y_lip - y_end) / landing;
    if (rise > G.approach_grade || fall < rise - K::roll) continue;
    // … and straight enough over that lip's own ramp and landing.
    const double height = height_for(fall);
    const auto from = -static_cast<long>(std::ceil((height * ramp_of) / step));
    const auto to = static_cast<long>(std::ceil((height * landing_of) / step));
    double lo = 0;
    double hi = 0;
    double turned = 0;
    for (long j = from; j < to; j++) {
      turned += angle_diff(pts[size_t(i + j)].heading, pts[size_t(i + j + 1)].heading);
      if (turned < lo) lo = turned;
      if (turned > hi) hi = turned;
    }
    if (hi - lo > K::straight) continue;
    candidates.push_back({i, fall - rise + ge::core::range(rng, 0, 0.04), fall});
  }
  std::stable_sort(candidates.begin(), candidates.end(),
                   [](const Candidate& a, const Candidate& b) { return b.score < a.score; });
  std::vector<TrackKicker> chosen;
  for (const Candidate& c : candidates) {
    if (long(chosen.size()) >= want) break;
    const double s_c = pts[size_t(c.index)].s;
    bool clear = true;
    for (const TrackKicker& k : chosen) {
      if (!(std::abs(pts[k.index].s - s_c) >= K::spacing)) clear = false;
    }
    if (!clear) continue;
    const double height = height_for(c.fall);
    if (!drops.empty()) {
      const double s0 = s_c;
      const double gap = R::drop::kicker_clear;
      bool on_drop = false;
      for (const TrackDrop& d : drops) {
        const double ds = pts[d.index].s;
        if (s0 - height * ramp_of < ds + d.face + d.landing + gap &&
            s0 + height * landing_of > ds - d.shelf - gap) {
          on_drop = true;
        }
      }
      if (on_drop) continue;
    }
    chosen.push_back({size_t(c.index), height, height * ramp_of, height * landing_of});
  }
  std::stable_sort(chosen.begin(), chosen.end(),
                   [](const TrackKicker& a, const TrackKicker& b) { return a.index < b.index; });
  for (const TrackKicker& k : chosen) {
    const double s0 = pts[k.index].s;
    const auto j0 = -static_cast<long>(std::ceil(k.ramp / step));
    const auto j1 = static_cast<long>(std::ceil(k.landing / step));
    for (long j = j0; j <= j1; j++) {
      const long at = long(k.index) + j;
      if (at < 0 || at >= n) continue;  // `if (!p) continue`
      TrackPoint& p = pts[size_t(at)];
      p.y += kicker_profile(k.height, k.ramp, k.landing, p.s - s0);
    }
  }
  return chosen;
}

std::vector<Kicker> publish_track_kickers(const Piste& piste,
                                          std::span<const TrackKicker> kickers) {
  std::vector<Kicker> out;
  for (const TrackKicker& k : kickers) {
    const TrackPoint& p = piste.track.points[k.index];
    out.push_back(
        Kicker{"", p.x, p.z, p.y, p.heading, k.height, k.ramp, k.landing, p.width, true, p.s});
  }
  std::stable_sort(out.begin(), out.end(),
                   [](const Kicker& a, const Kicker& b) { return *a.s < *b.s; });
  for (size_t i = 0; i < out.size(); i++) out[i].id = "K" + std::to_string(i + 1);
  return out;
}

std::vector<Kicker> lay_off_kickers(ge::core::Rng& rng, const TerrainPlan& plan,
                                    ge::core::Heightfield& ground, const Piste& piste) {
  namespace K = R::kickers::off;
  // R21, R23 — the region's and the grade's multiple of the rule's count.
  const Band count = scale_count(K::count, plan.region->kickers * plan.grade->kickers.off);
  const int want = ge::core::int_in(rng, int32_t(count.min), int32_t(count.max));
  std::vector<Kicker> out;
  const double size = plan.size;
  const double face = R::mountain::flank::inner - 60;
  for (int tries = 0; tries < want * 40 && int(out.size()) < want; tries++) {
    const double x = size / 2 + ge::core::range(rng, -face, face);
    const double z =
        plan.summit_z + (plan.base_z - plan.summit_z) * ge::core::range(rng, 0.08, 0.96);
    const double height = in_band(rng, K::height);
    const double ramp = in_band(rng, K::ramp);
    const double landing = in_band(rng, K::landing);
    const double width = in_band(rng, K::width);
    const double reach = num::max(ramp, landing) + width / 2 + R::kickers::edge;
    if (flank_at(plan, x, z) > 0.02 || descent_at(plan, z) > 1) continue;
    // Down the fall line, on ground a lip can be shaped on …
    const ge::core::FieldSlope g = ge::core::field_gradient(ground, x, z);
    const double slope = hypot(g.gx, g.gz);
    if (slope < R::cliff::fall || slope > K::max_slope) continue;
    const double heading = libm::atan2(-g.gx, -g.gz);
    // … and on a ROLL: the ground past the lip falls away steeper than it comes up to it.
    const Fall above = fall_along(ground, x - libm::sin(heading) * ramp,
                                  z - libm::cos(heading) * ramp, heading, ramp);
    const Fall below = fall_along(ground, x, z, heading, landing);
    if (below.steepest - above.gentlest < K::roll) continue;
    const double distance = nearest_track_point(piste.track, x, z).distance;
    if (distance - reach < R::track::width.max / 2 + K::clearance) continue;
    bool crowded = false;
    for (const Kicker& k : out) {
      if (hypot(k.x - x, k.z - z) < reach + num::max(k.ramp, k.landing) + 20) crowded = true;
    }
    if (crowded) continue;
    const double y0 = sample_field(ground, x, z);
    stamp_kicker(ground, x, z, heading, height, ramp, landing, width);
    out.push_back(Kicker{"X" + std::to_string(out.size() + 1), x, z, y0 + height, heading, height,
                         ramp, landing, width, false, std::nullopt});
  }
  // Publish the ground height the stamp actually left at each lip.
  for (Kicker& k : out) k.y = sample_field(ground, k.x, k.z);
  return out;
}

void stamp_kicker(ge::core::Heightfield& ground, double x, double z, double heading, double height,
                  double ramp, double landing, double width) {
  const double fx = libm::sin(heading);
  const double fz = libm::cos(heading);
  const double edge = R::kickers::edge;
  const double reach = num::max(ramp, landing) + width / 2 + edge;
  const double cell = ground.cell;
  const int c0 = int(num::max(0.0, std::floor((x - reach) / cell)));
  const int c1 = int(num::min(double(ground.cols - 1), std::ceil((x + reach) / cell)));
  const int r0 = int(num::max(0.0, std::floor((z - reach) / cell)));
  const int r1 = int(num::min(double(ground.rows - 1), std::ceil((z + reach) / cell)));
  for (int r = r0; r <= r1; r++) {
    for (int c = c0; c <= c1; c++) {
      const double dx = c * cell - x;
      const double dz = r * cell - z;
      const double u = dx * fx + dz * fz;
      const double v = std::abs(dx * fz - dz * fx);
      const double across = 1 - smoothstep(width / 2, width / 2 + edge, v);
      if (across <= 0) continue;
      float& h = ground.data[size_t(r) * size_t(ground.cols) + size_t(c)];
      h = static_cast<float>(h + across * kicker_profile(height, ramp, landing, u));
    }
  }
}

bool on_kicker(std::span<const Kicker> kickers, double x, double z, double margin) {
  for (const Kicker& k : kickers) {
    const double dx = x - k.x;
    const double dz = z - k.z;
    const double fx = libm::sin(k.heading);
    const double fz = libm::cos(k.heading);
    const double u = dx * fx + dz * fz;
    const double v = std::abs(dx * fz - dz * fx);
    if (u > -k.ramp - margin && u < k.landing + margin &&
        v < k.width / 2 + R::kickers::edge + margin) {
      return true;
    }
  }
  return false;
}

}  // namespace ge::mountain::mapgen
