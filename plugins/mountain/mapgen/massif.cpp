// The massif, dealt or laid off a real face: its plan, its profile tables and its bake.
//
// ge:summary The massif's plan draws, its profile tables and its row-by-row bake
// ge:keywords massif plan-massif bake-massif ridge-share steep-share bench-share massif-row
// ge:keywords height-at read-face face-lift brow
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include "mountain/mapgen/massif.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/noise.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/grades.hpp"
#include "mountain/mapgen/real_face.hpp"
#include "mountain/mapgen/resort_rules.hpp"
#include "mountain/mapgen/rules.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace R = rules;
namespace RR = resort_rules;
using ge::core::sample_noise;
using ge::core::smoothstep;

namespace {

using Table = std::array<double, kProfileSamples + 1>;

struct BenchCut {
  double u, w, depth;
};

// A smooth bell, one at 0 falling toward nothing (the ratio squared, as a product).
double bell(double d, double sigma) {
  const double q = d / sigma;
  return libm::exp(-0.5 * (q * q));
}

// The grade's shape at u, with the bench taken out of it.
double shape_at(const MassifShape& p, double u, const BenchCut* bench) {
  const double shoulder = p.shoulder + (1 - p.shoulder) * smoothstep(0, p.shoulder_run, u);
  const double ease = libm::pow(num::max(0.0, 1 - u), p.ease);
  double g = shoulder * (ease * (1 - p.runout) + p.runout);
  if (bench) {
    const double d = (u - bench->u) / bench->w;
    if (std::abs(d) < 1) {
      const double c = libm::cos((d * ge::core::kPi) / 2);
      g *= 1 - bench->depth * (c * c);
    }
  }
  return g;
}

// The share of the drop still below u, 1 at the ridge and 0 at the floor, off the shape
// integrated by the trapezium.
Table tabulate(const MassifShape& p, const BenchCut* bench) {
  constexpr int n = kProfileSamples;
  Table table{};
  double sum = 0;
  for (int i = n - 1; i >= 0; i--) {
    sum += (shape_at(p, double(i) / n, bench) + shape_at(p, double(i + 1) / n, bench)) / 2 / n;
    table[static_cast<size_t>(i)] = sum;
  }
  for (double& v : table) v /= sum;
  return table;
}

double sample(const Table& table, double u) {
  if (u <= 0) return 1;
  if (u >= 1) return 0;
  const double t = u * kProfileSamples;
  const double i = std::floor(t);
  const auto k = static_cast<size_t>(i);
  return table[k] + (table[k + 1] - table[k]) * (t - i);
}

// A 1-D noise along the ridge, 0..1, wavelength `massif.ridgeScale`. The hash's
// `k * 374761393 + seed * 668265263` is a DOUBLE sum (the seed's product passes 2^53) before
// its truncation to 32 bits, so it is computed in doubles here too.
double ridge_noise(double seed, double x) {
  const double L = RR::massif::ridge_scale;
  const double i = std::floor(x / L);
  const double t = x / L - i;
  const auto h = [seed](double k) {
    int32_t v = num::to_int32(k * 374761393 + seed * 668265263);
    v = num::imul(uint32_t(v ^ int32_t(uint32_t(v) >> 13)), 1274126177);
    return double(uint32_t(v ^ int32_t(uint32_t(v) >> 16))) / 4294967296;
  };
  const double w = t * t * (3 - 2 * t);
  return h(i) + (h(i + 1) - h(i)) * w;
}

// R25 — the summit ridge's share of the vertical at x: one at the peak, the shoulder's share
// on the far side, a little noise along it.
double ridge_share(const Massif& m, double x) {
  const double peak = bell(x - m.peak_x, m.peak_spread);
  // The shoulder is a rise of its own, so the ridge past it falls again.
  const double shoulder = bell(x - m.shoulder_x, m.peak_spread * 1.3);
  const double base = RR::massif::ridge_floor;
  const double share =
      num::max(base + (m.shoulder_share - base) * shoulder, base) * (1 - peak) + peak;
  const double wander = (ridge_noise(m.ridge_seed, x / m.wide) - 0.5) * RR::massif::ridge_wander;
  return num::min(1.0, num::max(base, share + wander * (1 - peak)));
}

double steep_share(const Massif& m, double x) { return bell(x - m.peak_x, m.steep_spread); }

double bench_share(const Massif& m, double x) {
  return RR::massif::bench::floor +
         (1 - RR::massif::bench::floor) * bell(x - m.bench_x, m.bench_spread);
}

// Everything the height reads off z alone, read once a row: each is the very expression the
// height was written with, so a row read once is the row read every time, to the bit.
struct MassifRow {
  double z;
  double g, s, gb, sb;  // the four profile tables at the row's descent
  bool behind;          // behind the summit ridge…
  double back;          // …and how far it falls away there
  double floor;         // the folds' fade on the valley floor
  double ridge;         // the crests' fade down the face
  double open;          // how open the side ridges are at this descent
};

MassifRow massif_row(const TerrainPlan& plan, const Massif& m, double z) {
  const double u = descent_at(plan, z);
  const double crest = z - plan.summit_z;
  return MassifRow{
      z,
      sample(m.tables[0], u),
      sample(m.tables[1], u),
      sample(m.tables[2], u),
      sample(m.tables[3], u),
      z < plan.summit_z,
      (plan.summit_z - z) * R::mountain::back,
      1 - (1 - R::face::hills::floor * RR::massif::floor_relief) * smoothstep(0.88, 1.02, u),
      crest < 0 ? 1 : 1 - smoothstep(0, R::mountain::crest_spread, crest),
      flank_open(plan, z),
  };
}

// How much of a headwall's drop the face at x takes: the steep sector's share, faded across
// the wall's own reach.
double wall_drop(const Headwall& w, double x, double st) {
  const double across = w.x ? bell(x - *w.x, w.spread.value_or(300)) : 1;
  return w.drop * st * across;
}

// R25 — the ground at (x, z) of a real face laid 1:1: the real height over the face's floor,
// metre for metre, everywhere but a hollow below the valley floor's level and the valley floor
// itself, where the relief across the face fades out onto the face's own profile so the village
// and the hub stand level. The dealt descent is not read: the face is the mountain.
double face_lift(const TerrainPlan& plan, const PlanFace& real, double x, double z) {
  constexpr int n = kFaceN;
  const double t = num::min(double(n - 1), num::max(0.0, z / kFaceCell));
  const int r = static_cast<int>(num::min(double(n - 2), std::floor(t)));
  const auto ri = static_cast<size_t>(r);
  const double row = real.rows[ri] + (real.rows[ri + 1] - real.rows[ri]) * (t - r);
  // A hollow on the face deeper than the valley floor (a lake's basin, a side valley's bottom)
  // is filled to the floor's level: the runs come down to the hub without climbing out of it.
  const double real0 = face_height(*real.grid, x, z) - real.floor;
  const double lifted = z > plan.summit_z ? num::max(real0, real.rows[size_t(n - 1)]) : real0;
  const Band fade = RR::massif::real::floor;
  const double floor = 1 - smoothstep(fade.min, fade.max, descent_at(plan, z));
  return row + (lifted - row) * floor;
}

// R25, R3 — the massif's height at a plan point, with the shares across x and each headwall's
// drop at x already read (`walls`, from `walls_at` on).
double height_at(const TerrainPlan& plan, CountryFields& f, double x, const MassifRow& row,
                 double share, double st, double bn, const double* walls) {
  const double z = row.z;
  const size_t k = plan.headwalls.size();
  double drops = 0;
  for (size_t i = 0; i < k; i++) drops += walls[i];
  const double plain = row.g * (1 - st) + row.s * st;
  const double benched = row.gb * (1 - st) + row.sb * st;
  double h = (plan.vertical * share - drops) * (plain * (1 - bn) + benched * bn);
  if (plan.face) h = face_lift(plan, *plan.face, x, z);
  for (size_t i = 0; i < k; i++) {
    const Headwall& w = plan.headwalls[i];
    const double drop = walls[i];
    const double dz = z - w.z;
    if (dz < -w.run) {
      h += drop;
      continue;
    }
    if (dz > w.run) continue;
    const double wander = (sample_noise(f.headwalls[i], x, 0) * 2 - 1) * R::face::headwalls::wander;
    h += drop * (1 - smoothstep(-w.run / 2, w.run / 2, dz + wander));
  }
  if (row.behind && !plan.face) h -= row.back;
  const double wx = x + (sample_noise(f.warp_x, x, z) * 2 - 1) * 70;
  const double wz = z + (sample_noise(f.warp_z, x, z) * 2 - 1) * 70;
  const double flank = flank_across(plan, sample_noise(f.flank, x, z), x, row.open);
  // The folds, quieter on the valley floor where the village stands, and the spurs and
  // gullies strongest on the steep sector.
  const double floor = row.floor;
  h += fbm(f.hills, wx, wz) * plan.hills * floor;
  const double folds = RR::massif::gentle_folds + (1 - RR::massif::gentle_folds) * st;
  h += (ridged(f.ridges, wx, wz / R::face::ridges::stretch) - 0.5) * plan.ridges * folds *
       (1 - flank) * floor;
  if (plan.rollers > 0) {
    const double rx = wx * 0.8 + wz * 0.6;
    const double rz = wz * 0.8 - wx * 0.6;
    h += (ridged(f.rollers, rx, rz) - 0.5) * plan.rollers * floor;
  }
  for (const Bowl& b : plan.bowls) {
    const double ex = x - b.x;
    const double ez = z - b.z;
    const double d2 = (ex * ex + ez * ez) / (b.r * b.r);
    if (d2 < 1) h -= b.depth * (1 - d2) * (1 - d2);
  }
  const double ridge = row.ridge;
  if (flank > 0 || ridge > 0) {
    const double rx = wx * 0.866 - wz * 0.5;
    const double rz = wx * 0.5 + wz * 0.866;
    const double crests =
        plan.crests * (ridged(f.crests, rx, rz) * 0.75 + ridged(f.crests_fine, rz, rx) * 0.25);
    h += plan.flank * flank * std::sqrt(flank) + crests * num::max(flank, ridge);
  }
  return h;
}

// R25 — what a real face stands in for: where its peak, its shoulder and its village are (each
// kept in the band a dealt massif's is, on the side the real one stands), its summit ridge, its
// valley floor's real height (the map's nought) and its own profile down the fall line.
struct FaceRead {
  double summit_z, side, peak_x, shoulder_x, village_x;
  std::shared_ptr<PlanFace> face;
};

FaceRead read_face(std::shared_ptr<const RealFace> grid, double size, double dealt_summit_z,
                   double base_z, double wide) {
  namespace M = RR::massif;
  const RealFace& g = *grid;
  const double cx = size / 2;
  const double reach = M::real::reach;
  const double mean = face_mean(g, cx - reach, cx + reach, base_z - 60, size);
  // THE SUMMIT RIDGE where the real one stands: the highest row of the face's mean profile from
  // the rule book's ridge row down to `real::ridge` m. A crop whose top is a plateau or the back
  // of the ridge has its summit ridge lower down the map, and the lifts top out and the runs
  // leave there rather than on ground that does not fall.
  double summit_z = dealt_summit_z;
  double best = -std::numeric_limits<double>::infinity();
  for (int j = 0; dealt_summit_z + j * kFaceCell <= M::real::ridge; j++) {
    const double z = dealt_summit_z + j * kFaceCell;
    const double h = face_mean(g, cx - reach, cx + reach, z, z);
    if (h > best) {
      best = h;
      summit_z = z;
    }
  }
  // …and on down to its BROW: a fell's ridge is a plateau that falls a few metres over hundreds,
  // and its lifts top out where the face starts to fall.
  const double brow = best - M::real::brow::share * (best - mean);
  const double ridge_z = summit_z;
  for (int j = 1; ridge_z + j * kFaceCell <= M::real::brow::most; j++) {
    const double z = ridge_z + j * kFaceCell;
    if (face_mean(g, cx - reach, cx + reach, z, z) < brow) break;
    summit_z = z;
  }
  const double real_peak = face_extreme(g, cx - reach, cx + reach, summit_z - 60, summit_z + 120);
  const double side = real_peak >= cx ? 1 : -1;
  const auto keep = [cx](double x, Band band, double s) {
    const double d = num::min(band.max, num::max(band.min, (x - cx) * s));
    return cx + d * s;
  };
  const double peak_x = keep(real_peak, scale_band(M::peak::across, wide), side);
  const double o0 = side > 0 ? cx - reach : cx;
  const double o1 = side > 0 ? cx : cx + reach;
  const double shoulder_x = keep(face_extreme(g, o0, o1, summit_z - 60, summit_z + 120),
                                 scale_band(M::shoulder::across, wide), -side);
  // The valley at its lowest point, in the dealt band.
  const double village_x = keep(face_extreme(g, o0, o1, base_z - 60, size, true),
                                scale_band(M::village::across, wide), -side);
  // The face's own profile: its mean height across the playable face, a row at a time, over its
  // floor. Below the ridge it never climbs, so a valley floor whose far side rises is laid level
  // rather than as the foot of the next slope; below the base row it is the valley floor, level,
  // however the real valley goes on falling. The face's floor is that level.
  constexpr int n = kFaceN;
  auto face = std::make_shared<PlanFace>();
  face->grid = std::move(grid);
  std::vector<double>& rows = face->rows;
  rows.resize(size_t(n));
  for (size_t r = 0; r < size_t(n); r++) {
    const double z = double(r) * kFaceCell;
    rows[r] = face_mean(g, cx - reach, cx + reach, z, z) - mean;
    if (z > summit_z && r > 0) rows[r] = num::min(rows[r], rows[r - 1]);
  }
  const int base = std::min(n - 1, static_cast<int>(num::round(base_z / kFaceCell)));
  const double level = rows[size_t(base)];
  for (size_t r = 0; r < size_t(n); r++) rows[r] = int(r) > base ? 0 : rows[r] - level;
  face->floor = mean + level;
  return FaceRead{summit_z, side, peak_x, shoulder_x, village_x, std::move(face)};
}

}  // namespace

TerrainPlan plan_massif(ge::core::Rng& rng, const Region& region,
                        const std::shared_ptr<const RealFace>& face, int attempt) {
  namespace M = RR::massif;
  namespace F = R::face;
  namespace Q = RR::massif::relief;
  const auto& K = region.relief;
  // R1 — a resort's square is wider than the rule book's.
  const double size = M::size;
  const double cx = size / 2;
  const double summit_z = size * R::mountain::summit;
  const double base_z = size * R::mountain::base;
  const double vertical = in_band(rng, M::vertical);
  // Everything across the face is stretched with the square.
  const double wide = size / R::world::size;
  const auto across = [wide](Band band) { return scale_band(band, wide); };
  const double side = ge::core::chance(rng, 0.5) ? 1 : -1;
  const double peak_x = cx + side * in_band(rng, across(M::peak::across));
  const double peak_spread = in_band(rng, across(M::peak::spread));
  const double shoulder_x = cx - side * in_band(rng, across(M::shoulder::across));
  const double shoulder_share = in_band(rng, M::shoulder::share);
  const double steep_spread = in_band(rng, across(M::sector));
  const double bench_u = in_band(rng, M::bench::at);
  const double bench_width = in_band(rng, M::bench::width);
  const double bench_depth = in_band(rng, M::bench::depth);
  const double village_x = cx - side * in_band(rng, across(M::village::across));
  const double bench_toward = in_band(rng, M::bench::toward);
  const double bench_x = village_x + (peak_x - village_x) * bench_toward;
  const double bench_spread = in_band(rng, across(M::bench::spread));
  const double flank_inner = M::flank::inner * wide;
  const double flank_outer = M::flank::outer * wide;
  const double flank = in_band(rng, scale_band(R::mountain::flank::height, K.flank * Q::flank));
  const double hills = in_band(rng, scale_band(F::hills::amplitude, K.hills * Q::hills));
  const double ridges =
      in_band(rng, scale_band(F::ridges::amplitude, K.ridges * M::ridges * Q::ridges));
  std::vector<Bowl> bowls;
  const Band n_bowls = scale_count(F::bowls::count, K.bowls.count);
  const int bowl_count = ge::core::int_in(rng, int32_t(n_bowls.min), int32_t(n_bowls.max));
  for (int i = 0; i < bowl_count; i++) {
    const double u = in_band(rng, F::bowls::at);
    const double off = ge::core::range(rng, -1, 1) * (flank_inner - 200);
    const double r = in_band(rng, scale_band(F::bowls::radius, K.bowls.radius));
    const double depth = in_band(rng, scale_band(F::bowls::depth, K.bowls.depth * Q::bowls));
    bowls.push_back({cx + off, summit_z + (base_z - summit_z) * u, r, depth});
  }
  std::vector<Headwall> headwalls;
  const Band n_walls = scale_count(F::headwalls::count, K.headwalls.count);
  const int wall_count = ge::core::int_in(rng, int32_t(n_walls.min), int32_t(n_walls.max));
  for (int i = 0; i < wall_count; i++) {
    const double u = in_band(rng, M::headwalls::at);
    const double drop =
        in_band(rng, scale_band(F::headwalls::drop, K.headwalls.drop * Q::headwalls));
    const double run = in_band(rng, F::headwalls::run);
    const double x = peak_x + ge::core::range(rng, -1, 1) * M::headwalls::across * wide;
    const double spread = in_band(rng, across(M::headwalls::spread));
    headwalls.push_back({summit_z + (base_z - summit_z) * u, drop, run, x, spread});
  }
  const double dealt_base = in_band(rng, region.altitude.base);
  const double dealt_line = in_band(rng, region.altitude.tree_line);
  const auto seed = [&rng]() { return double(ge::core::int_in(rng, 1, 0x7ffffff0)); };
  TerrainPlan plan{};
  plan.seeds.warp = seed();
  plan.seeds.flank = seed();
  plan.seeds.hills = seed();
  plan.seeds.ridges = seed();
  plan.seeds.rollers = seed();
  plan.seeds.crests = seed();
  plan.seeds.headwalls = seed();
  const BenchCut bench{bench_u, bench_width, bench_depth};
  // A REAL FACE: every draw above is made all the same, so the stream the rest of the ski area
  // is dealt off runs on as it would; what the face stands in for is then read off it. Where the
  // real peak, shoulder and valley stand is kept for the first `real::placed` attempts; a face
  // whose stations will not take them is given the dealt ones after that, its relief laid over
  // them all the same.
  const std::optional<FaceRead> real =
      face ? std::optional<FaceRead>(read_face(face, size, summit_z, base_z, wide)) : std::nullopt;
  const FaceRead* placed = real && attempt < RR::massif::real::placed ? &*real : nullptr;
  auto massif = std::make_shared<Massif>(Massif{
      placed ? placed->side : side,
      placed ? placed->peak_x : peak_x,
      peak_spread,
      placed ? placed->shoulder_x : shoulder_x,
      shoulder_share,
      steep_spread,
      bench_u,
      bench_width,
      bench_depth,
      // The bench (and the gondola's top on it) between the village and the peak, wherever a
      // real face stands them.
      placed ? placed->village_x + (placed->peak_x - placed->village_x) * bench_toward : bench_x,
      bench_spread,
      placed ? placed->village_x : village_x,
      {tabulate(M::gentle_profile, nullptr), tabulate(M::steep_profile, nullptr),
       tabulate(M::gentle_profile, &bench), tabulate(M::steep_profile, &bench)},
      0,
      wide,
  });
  massif->ridge_seed = seed();
  // R21 — the tree line: the floor a few metres over the sea, and the line the same SHARE of
  // the mountain over it as the region's bands give a mountain of the region's own height.
  const double sea = in_band(rng, M::sea);
  const double above = dealt_line - dealt_base;
  const double region_vertical =
      ((RR::tree_line_massif_vertical.min + RR::tree_line_massif_vertical.max) / 2) * K.vertical;
  plan.vertical = vertical;
  plan.altitude = sea;
  plan.tree_line = sea + (above * vertical) / region_vertical;
  plan.size = size;
  plan.summit_z = summit_z;
  plan.base_z = base_z;
  plan.flank = flank;
  plan.crests = R::mountain::crests * K.crests;
  plan.hills = hills;
  plan.ridges = ridges;
  plan.rollers = F::rollers::amplitude * K.rollers * Q::rollers;
  plan.bowls = std::move(bowls);
  plan.headwalls = std::move(headwalls);
  if (real) {
    // A REAL FACE is laid 1:1: the map's height is the real one less the face's floor, so the
    // floor's real altitude is the map's nought, the tree line is the region's in metres over
    // the sea, and the vertical is the real peak's over the floor. Its side ridges, crests,
    // bowls and headwalls are its own, and the hills and spurs are laid only under the scale the
    // model sees.
    plan.summit_z = real->summit_z;
    plan.vertical = face_height(*face, massif->peak_x, real->summit_z) - real->face->floor;
    plan.altitude = real->face->floor;
    plan.tree_line = dealt_line;
    plan.flank = 0;
    plan.crests = 0;
    plan.hills = hills * RR::massif::real::folds;
    plan.ridges = ridges * RR::massif::real::folds;
    plan.bowls.clear();
    plan.headwalls.clear();
    plan.face = real->face;
  }
  plan.profile = &massif->tables[0];
  plan.region = &region;
  plan.grade = &kUngraded;
  plan.sea = sea;
  plan.fold = Q::scale;
  plan.flank_band = {flank_inner, flank_outer};
  plan.massif = std::move(massif);
  return plan;
}

ge::core::Heightfield bake_massif(const TerrainPlan& plan) {
  const Massif& m = *plan.massif;
  const double cell = R::world::cell;
  const int n = static_cast<int>(num::round(plan.size / cell)) + 1;
  const auto nn = static_cast<size_t>(n);
  const size_t k = plan.headwalls.size();
  std::vector<double> share(nn);
  std::vector<double> st(nn);
  std::vector<double> bn(nn);
  std::vector<double> walls(nn * k);
  for (size_t c = 0; c < nn; c++) {
    const double x = double(c) * cell;
    share[c] = ridge_share(m, x);
    st[c] = steep_share(m, x);
    bn[c] = bench_share(m, x);
    for (size_t i = 0; i < k; i++) walls[c * k + i] = wall_drop(plan.headwalls[i], x, st[c]);
  }
  ge::core::Heightfield field = ge::core::create_heightfield(0, 0, cell, n, n);
  float* d = field.data.data();
  CountryFields fields = country_fields(plan);
  for (size_t r = 0; r < nn; r++) {
    const MassifRow row = massif_row(plan, m, double(r) * cell);
    for (size_t c = 0; c < nn; c++) {
      d[r * nn + c] = static_cast<float>(height_at(plan, fields, double(c) * cell, row, share[c],
                                                   st[c], bn[c], walls.data() + c * k));
    }
  }
  return field;
}

}  // namespace ge::mountain::mapgen
