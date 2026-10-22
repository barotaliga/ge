// The definitions behind terrain.hpp (the single piste's half).
//
// ge:summary The mountain's plan draws and the bake of every noise layer onto the grid
// ge:keywords terrain fbm ridged bake-country parallel
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#include "mountain/mapgen/terrain.hpp"

#include <cmath>
#include <limits>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "core/parallel.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace R = rules;
using ge::core::noise_field;
using ge::core::NoiseField;
using ge::core::sample_noise;
using ge::core::smoothstep;

namespace {

using ProfileTable = std::array<double, kProfileSamples + 1>;

// The grade's shape down the fall line, before it is scaled to the vertical.
double grade_shape(const ProfileShape& p, double u) {
  const double shoulder = p.shoulder + (1 - p.shoulder) * smoothstep(0, p.shoulder_run, u);
  const double ease = libm::pow(num::max(0.0, 1 - u), p.ease);
  return shoulder * (ease * (1 - p.runout) + p.runout);
}

// The share of the drop still below u, off the shape integrated by the trapezium.
ProfileTable tabulate_profile(const ProfileShape& p) {
  constexpr int n = kProfileSamples;
  ProfileTable table{};
  double sum = 0;
  for (int i = n - 1; i >= 0; i--) {
    sum += (grade_shape(p, double(i) / n) + grade_shape(p, double(i + 1) / n)) / 2 / n;
    table[static_cast<size_t>(i)] = sum;
  }
  for (double& v : table) v /= sum;
  return table;
}

// One table a grade row, worked out once.
const ProfileTable* profile_for(const GradeRow& grade) {
  static const std::array<ProfileTable, 5> tables{
      tabulate_profile(kUngraded.profile), tabulate_profile(grade_row(PisteGrade::green).profile),
      tabulate_profile(grade_row(PisteGrade::blue).profile),
      tabulate_profile(grade_row(PisteGrade::red).profile),
      tabulate_profile(grade_row(PisteGrade::black).profile)};
  return &tables[grade.id ? 1 + static_cast<size_t>(*grade.id) : 0];
}

template <size_t N>
std::array<NoiseField, N> fbm_fields(double scale, double seed) {
  std::array<NoiseField, N> fields{};
  double s = scale;
  for (size_t o = 0; o < N; o++) {
    fields[o] = noise_field(s, seed + double(o) * 7919);
    s *= 0.5;
  }
  return fields;
}

RidgedFields ridged_fields(double scale, double seed) {
  return {noise_field(scale, seed), noise_field(scale * 0.5, seed + 31)};
}

double flank_of(const TerrainPlan& plan, double noise, double x, double z) {
  return flank_across(plan, noise, x, flank_open(plan, z));
}

// The profile's share of the vertical still to fall at u, 1 above the summit, 0 past the base.
double profile_at(const TerrainPlan& plan, double u) {
  if (u <= 0) return 1;
  if (u >= 1) return 0;
  const double t = u * kProfileSamples;
  const double i = std::floor(t);
  const auto& p = *plan.profile;
  const double a = p[static_cast<size_t>(i)];
  return a + (p[static_cast<size_t>(i) + 1] - a) * (t - i);
}

// R2, R3 — the untouched mountain's height at a plan point, m.
double country_at(const TerrainPlan& plan, CountryFields& f, double x, double z) {
  const double u = descent_at(plan, z);
  // THE PROFILE, the headwalls' drops taken out and put back as wandering bands.
  double drops = 0;
  for (const Headwall& w : plan.headwalls) drops += w.drop;
  double h = (plan.vertical - drops) * profile_at(plan, u);
  for (size_t i = 0; i < plan.headwalls.size(); i++) {
    const Headwall& w = plan.headwalls[i];
    const double dz = z - w.z;
    if (dz < -w.run) {
      h += w.drop;
      continue;
    }
    if (dz > w.run) continue;
    const double wander = (sample_noise(f.headwalls[i], x, 0) * 2 - 1) * R::face::headwalls::wander;
    h += w.drop * (1 - smoothstep(-w.run / 2, w.run / 2, dz + wander));
  }
  // Behind the ridge the ground falls away toward the top edge, so the ridge is a crest.
  if (z < plan.summit_z) h -= (plan.summit_z - z) * R::mountain::back;
  // A slow domain warp, so the folds are not laid out on the noise's own lattice.
  const double wx = x + (sample_noise(f.warp_x, x, z) * 2 - 1) * 70;
  const double wz = z + (sample_noise(f.warp_z, x, z) * 2 - 1) * 70;
  const double flank = flank_of(plan, sample_noise(f.flank, x, z), x, z);
  // THE FACE: broad rolls, quieter on the valley floor; spurs and gullies stretched down the
  // fall line and gone on the flanks; rollers on a lattice turned off the others'.
  const double floor = 1 - (1 - R::face::hills::floor) * smoothstep(0.9, 1.05, u);
  h += fbm(f.hills, wx, wz) * plan.hills * floor;
  h += (ridged(f.ridges, wx, wz / R::face::ridges::stretch) - 0.5) * plan.ridges * (1 - flank) *
       floor;
  if (plan.rollers > 0) {
    const double rx = wx * 0.8 + wz * 0.6;
    const double rz = wz * 0.8 - wx * 0.6;
    h += (ridged(f.rollers, rx, rz) - 0.5) * plan.rollers * floor;
  }
  for (const Bowl& b : plan.bowls) {
    const double ex = x - b.x;
    const double ez = z - b.z;
    const double d2 = (ex * ex + ez * ez) / (b.r * b.r);  // a square is x * x, never pow
    if (d2 < 1) h -= b.depth * (1 - d2) * (1 - d2);
  }
  // THE SIDE RIDGES with crests of their own, and the summit ridge's crests fading down.
  const double crest = z - plan.summit_z;
  const double ridge = crest < 0 ? 1 : 1 - smoothstep(0, R::mountain::crest_spread, crest);
  if (flank > 0 || ridge > 0) {
    const double rx = wx * 0.866 - wz * 0.5;
    const double rz = wx * 0.5 + wz * 0.866;
    const double crests =
        plan.crests * (ridged(f.crests, rx, rz) * 0.75 + ridged(f.crests_fine, rz, rx) * 0.25);
    h += plan.flank * flank * std::sqrt(flank) + crests * num::max(flank, ridge);
  }
  return h;
}

}  // namespace

// Fractal value noise centred on zero, roughly −1..1: octaves halving in scale and weight.
double fbm(std::array<NoiseField, 4>& fields, double x, double z) {
  double sum = 0;
  double amp = 1;
  double norm = 0;
  for (NoiseField& f : fields) {
    sum += (sample_noise(f, x, z) * 2 - 1) * amp;
    norm += amp;
    amp *= 0.5;
  }
  return sum / norm;
}

// Ridged noise, 0..1 with sharp crests at 1: v^1.25 without a pow.
double ridged(RidgedFields& f, double x, double z) {
  const double a = 1 - std::abs(sample_noise(f.coarse, x, z) * 2 - 1);
  const double b = 1 - std::abs(sample_noise(f.fine, x, z) * 2 - 1);
  const double v = a * a * 0.7 + b * b * 0.3;
  return v * std::sqrt(std::sqrt(v));
}

CountryFields country_fields(const TerrainPlan& plan) {
  const auto& s = plan.seeds;
  CountryFields f{noise_field(420, s.warp),
                  noise_field(420, s.warp + 17),
                  noise_field(260, s.flank),
                  fbm_fields<4>(R::face::hills::scale * plan.fold, s.hills),
                  ridged_fields(R::face::ridges::scale * plan.fold, s.ridges),
                  ridged_fields(R::face::rollers::scale, s.rollers),
                  ridged_fields(170, s.crests),
                  ridged_fields(60, s.crests + 5),
                  {}};
  for (size_t i = 0; i < plan.headwalls.size(); i++) {
    f.headwalls.push_back(noise_field(160, s.headwalls + double(i) * 101));
  }
  return f;
}

double flank_open(const TerrainPlan& plan, double z) {
  namespace F = R::mountain::flank;
  return 1 - smoothstep(F::open.min, F::open.max, descent_at(plan, z));
}

double flank_across(const TerrainPlan& plan, double noise, double x, double open) {
  namespace F = R::mountain::flank;
  const double warp = (noise * 2 - 1) * F::warp;
  const double across = std::abs(x - plan.size / 2) + warp;
  return smoothstep(plan.flank_band.inner, plan.flank_band.outer, across) * open;
}

TerrainPlan plan_terrain(ge::core::Rng& rng, const Region& region, const GradeRow& grade) {
  namespace M = R::mountain;
  namespace F = R::face;
  const auto& K = region.relief;
  const auto& G = grade.relief;
  const auto seed = [&rng]() { return double(ge::core::int_in(rng, 1, 0x7ffffff0)); };
  const double size = R::world::size;
  const double summit_z = size * M::summit;
  const double base_z = size * M::base;
  const double cx = size / 2;
  // In this order: the stream a map is always dealt.
  const double vertical = in_band(rng, vertical_band(region, grade));
  const double flank = in_band(rng, scale_band(M::flank::height, K.flank));
  const double hills = in_band(rng, scale_band(F::hills::amplitude, K.hills * G.hills));
  const double ridges = in_band(rng, scale_band(F::ridges::amplitude, K.ridges * G.ridges));
  std::vector<Bowl> bowls;
  const Band n_bowls = scale_count(F::bowls::count, K.bowls.count);
  const int bowl_count = ge::core::int_in(rng, int32_t(n_bowls.min), int32_t(n_bowls.max));
  for (int i = 0; i < bowl_count; i++) {
    const double u = in_band(rng, F::bowls::at);
    const double across = ge::core::range(rng, -1, 1) * (M::flank::inner - 150);
    const double r = in_band(rng, scale_band(F::bowls::radius, K.bowls.radius));
    const double depth = in_band(rng, scale_band(F::bowls::depth, K.bowls.depth));
    bowls.push_back({cx + across, summit_z + (base_z - summit_z) * u, r, depth});
  }
  std::vector<Headwall> headwalls;
  const Band n_walls = scale_count(F::headwalls::count, K.headwalls.count * G.headwalls.count);
  const int wall_count = ge::core::int_in(rng, int32_t(n_walls.min), int32_t(n_walls.max));
  for (int i = 0; i < wall_count; i++) {
    const double u = in_band(rng, F::headwalls::at);
    const double drop =
        in_band(rng, scale_band(F::headwalls::drop, K.headwalls.drop * G.headwalls.drop));
    const double run = in_band(rng, F::headwalls::run);
    headwalls.push_back(
        {summit_z + (base_z - summit_z) * u, drop, run, std::nullopt, std::nullopt});
  }
  const double altitude = in_band(rng, region.altitude.base);
  const double tree_line = in_band(rng, region.altitude.tree_line);
  TerrainPlan plan{vertical,
                   altitude,
                   tree_line,
                   size,
                   summit_z,
                   base_z,
                   flank,
                   M::crests * K.crests,
                   hills,
                   ridges,
                   F::rollers::amplitude * K.rollers * G.rollers,
                   std::move(bowls),
                   std::move(headwalls),
                   profile_for(grade),
                   &region,
                   &grade,
                   {},
                   std::nullopt,
                   1,
                   nullptr,
                   {R::mountain::flank::inner, R::mountain::flank::outer},
                   nullptr};
  plan.seeds.warp = seed();
  plan.seeds.flank = seed();
  plan.seeds.hills = seed();
  plan.seeds.ridges = seed();
  plan.seeds.rollers = seed();
  plan.seeds.crests = seed();
  plan.seeds.headwalls = seed();
  return plan;
}

ge::core::Heightfield bake_country(const TerrainPlan& plan) {
  const double cell = R::world::cell;
  const int n = static_cast<int>(num::round(plan.size / cell)) + 1;
  ge::core::Heightfield field = ge::core::create_heightfield(0, 0, cell, n, n);
  float* d = field.data.data();
  // Every height is a pure function of its cell, so the rows bake in bands side by side.
  ge::core::parallel_bands(size_t(n), [&](size_t r0, size_t r1) {
    CountryFields fields = country_fields(plan);
    for (int r = int(r0); r < int(r1); r++) {
      const double z = r * cell;
      for (int c = 0; c < n; c++) {
        d[static_cast<size_t>(r) * size_t(n) + size_t(c)] =
            static_cast<float>(country_at(plan, fields, c * cell, z));
      }
    }
  });
  return field;
}

double flank_at(const TerrainPlan& plan, double x, double z) {
  return flank_of(plan, ge::core::value_noise(x, z, 260, plan.seeds.flank), x, z);
}

double sea_level_of(const TerrainPlan& plan, const ge::core::Heightfield& ground,
                    const Vec3& village) {
  if (!plan.sea) return village.y - plan.altitude;
  // A plain loop and compare: the samples are floats, never NaN, and the lowest of them is the
  // same whichever comparison finds it.
  double low = std::numeric_limits<double>::infinity();
  for (const float v : ground.data) {
    if (v < low) low = v;
  }
  return low - *plan.sea;
}

}  // namespace ge::mountain::mapgen
