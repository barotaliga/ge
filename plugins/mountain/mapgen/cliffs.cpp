// Port of game5 `engine/mapgen/cliffs.ts`.
//
// ge:summary Lays the cliffs on the face clear of the piste and cuts them into the ground
// ge:keywords cliff layCliffs stampCliff
// ge:oracle game5/engine/mapgen/cliffs.ts
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#include "mountain/mapgen/cliffs.hpp"

#include <cmath>
#include <string>

#include "core/js.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/prng.hpp"
#include "mountain/mapgen/kickers.hpp"
#include "mountain/mapgen/query.hpp"
#include "mountain/mapgen/regions.hpp"

namespace ge::mountain::mapgen {

namespace js = ge::core::js;
namespace libm = ge::core::libm;
namespace R = rules;
using ge::core::hypot;
using ge::core::sample_field;
using ge::core::smoothstep;

namespace {

// Salt on the attempt's sub-seed for the cliffs' own stream.
constexpr uint32_t kCliffSalt = 0x63c1ff5;

// How far past the least clearance a cliff stood BESIDE the piste may stand, m.
constexpr double kBesideReach = 24;

struct Extent {
  double back;
  double ahead;
  double side;
};

// How far across the edge a cliff stands at full height, and how far ahead and behind.
Extent extent(const Cliff& c) {
  return {c.shelf, c.face + c.landing + R::cliff::runout, c.width / 2 + R::cliff::edge};
}

}  // namespace

double cliff_profile(double drop, double shelf, double face, double apron, double landing,
                     double u) {
  const double top = drop + apron;
  if (u <= -shelf || u >= face + landing) return 0;
  if (u <= 0) return top * smoothstep(0, 1, 1 + u / shelf);
  if (u < face) return top - (drop * u) / face;
  const double t = 1 - (u - face) / landing;
  return apron * t * t;
}

std::vector<PlanPoint> cliff_footprint(const Cliff& c, double step) {
  const Extent e = extent(c);
  const double fx = libm::sin(c.heading);
  const double fz = libm::cos(c.heading);
  std::vector<PlanPoint> out;
  const double nu = js::max(1.0, std::ceil((e.back + e.ahead) / step));
  const double nv = js::max(1.0, std::ceil((2 * e.side) / step));
  // Whole counters: the oracle's are JS numbers, but only ever integers, so the bits match.
  for (int i = 0; i <= int(nu); i++) {
    const double u = -e.back + ((e.back + e.ahead) * i) / nu;
    for (int j = 0; j <= int(nv); j++) {
      const double v = -e.side + (2 * e.side * j) / nv;
      out.push_back({c.x + fx * u + fz * v, c.z + fz * u - fx * v});
    }
  }
  return out;
}

bool on_cliff(std::span<const Cliff> cliffs, double x, double z, double margin) {
  for (const Cliff& c : cliffs) {
    const Extent e = extent(c);
    const double dx = x - c.x;
    const double dz = z - c.z;
    const double fx = libm::sin(c.heading);
    const double fz = libm::cos(c.heading);
    const double u = dx * fx + dz * fz;
    const double v = std::abs(dx * fz - dz * fx);
    if (u > -e.back - margin && u < e.ahead + margin && v < e.side + margin) return true;
  }
  return false;
}

std::vector<Cliff> lay_cliffs(uint32_t sub, const TerrainPlan& plan, ge::core::Heightfield& ground,
                              const Track& piste, std::span<const Kicker> kickers,
                              std::span<const Cliff> drops) {
  namespace C = R::cliff;
  const auto& G = plan.grade->cliffs;
  ge::core::Rng rng = ge::core::make_rng(sub ^ kCliffSalt);
  const Band count = scale_count(C::count, plan.region->kickers * G.count);
  const double size = plan.size;
  const double face = R::mountain::flank::inner - 40;
  const double clear = R::track::width.max / 2 + G.clearance;
  std::vector<Cliff> out;
  const double length = piste.length;

  // One cliff stood at a plan point if the face there takes it: cut into the ground and kept.
  const auto try_cliff = [&](double x, double z, double drop, double shelf, double width) {
    const double run = drop * C::face;
    const double apron = drop * C::apron;
    const double landing = apron * C::landing;
    if (descent_at(plan, z) > 1) return false;
    // Faces down the mountain's own fall, read over a wide stencil so a roller's face does
    // not pass for a slope.
    const ge::core::FieldSlope g = ge::core::field_gradient(ground, x, z);
    const double gx = (sample_field(ground, x + 20, z) - sample_field(ground, x - 20, z)) / 40;
    const double gz = (sample_field(ground, x, z + 20) - sample_field(ground, x, z - 20)) / 40;
    const double fall = hypot(gx, gz);
    if (fall < C::fall || hypot(g.gx, g.gz) > 0.6) return false;
    const double heading = libm::atan2(-gx, -gz);
    Cliff cliff{"C" + std::to_string(out.size() + 1),
                x,
                z,
                0,
                heading,
                drop,
                run,
                apron,
                landing,
                shelf,
                width,
                false,
                std::nullopt};
    const std::vector<PlanPoint> foot = cliff_footprint(cliff);
    for (const PlanPoint& p : foot) {
      if (flank_at(plan, p.x, p.z) > 0.02) return false;
    }
    for (const PlanPoint& p : foot) {
      if (nearest_track_point(piste, p.x, p.z).distance < clear) return false;
    }
    for (const PlanPoint& p : foot) {
      if (on_kicker(kickers, p.x, p.z, 6) || on_cliff(out, p.x, p.z, 10) ||
          (!drops.empty() && on_cliff(drops, p.x, p.z, 10))) {
        return false;
      }
    }
    stamp_cliff(ground, cliff);
    out.push_back(std::move(cliff));
    return true;
  };

  // BESIDE THE PISTE: drawn only on a grade that asks for any, so the ungraded row draws what
  // it always drew.
  if (G.beside.max > 0) {
    const int beside = ge::core::int_in(rng, int32_t(G.beside.min), int32_t(G.beside.max));
    for (int tries = 0, laid = 0; tries < beside * 60 && laid < beside; tries++) {
      const TrackPoint p = track_point_at(piste, ge::core::range(rng, 0.08, 0.9) * length);
      const double side = ge::core::chance(rng, 0.5) ? 1 : -1;
      const double width = in_band(rng, C::width);
      const double off = clear + width / 2 + C::edge + ge::core::range(rng, 0, kBesideReach);
      const double x = p.x + libm::cos(p.heading) * side * off;
      const double z = p.z - libm::sin(p.heading) * side * off;
      const double drop = in_band(rng, C::drop);
      const double shelf = in_band(rng, C::shelf);
      if (try_cliff(x, z, drop, shelf, width)) laid++;
    }
  }
  const double want =
      double(out.size()) + ge::core::int_in(rng, int32_t(count.min), int32_t(count.max));
  for (double tries = 0; tries < want * 40 && double(out.size()) < want; tries++) {
    const double x = size / 2 + ge::core::range(rng, -face, face);
    const double z =
        plan.summit_z + (plan.base_z - plan.summit_z) * ge::core::range(rng, 0.06, 0.96);
    const double drop = in_band(rng, C::drop);
    const double shelf = in_band(rng, C::shelf);
    const double width = in_band(rng, C::width);
    try_cliff(x, z, drop, shelf, width);
  }
  // Publish the ground the stamp left at the top of each edge.
  for (Cliff& c : out) c.y = sample_field(ground, c.x, c.z);
  return out;
}

void stamp_cliff(ge::core::Heightfield& ground, const Cliff& c) {
  const Extent e = extent(c);
  const double ahead = c.face + c.landing;
  const double fx = libm::sin(c.heading);
  const double fz = libm::cos(c.heading);
  const double reach = hypot(js::max(e.back, ahead), e.side);
  const double cell = ground.cell;
  const int c0 = int(js::max(0.0, std::floor((c.x - reach) / cell)));
  const int c1 = int(js::min(double(ground.cols - 1), std::ceil((c.x + reach) / cell)));
  const int r0 = int(js::max(0.0, std::floor((c.z - reach) / cell)));
  const int r1 = int(js::min(double(ground.rows - 1), std::ceil((c.z + reach) / cell)));
  for (int r = r0; r <= r1; r++) {
    for (int col = c0; col <= c1; col++) {
      const double dx = col * cell - c.x;
      const double dz = r * cell - c.z;
      const double u = dx * fx + dz * fz;
      const double v = std::abs(dx * fz - dz * fx);
      const double across = 1 - smoothstep(c.width / 2, e.side, v);
      if (across <= 0) continue;
      float& h = ground.data[size_t(r) * size_t(ground.cols) + size_t(col)];
      h = static_cast<float>(h + across *
                                     cliff_profile(c.drop, c.shelf, c.face, c.apron, c.landing, u));
    }
  }
}

}  // namespace ge::mountain::mapgen
