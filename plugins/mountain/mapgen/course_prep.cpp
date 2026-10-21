// The course's preparation behind course_prep.hpp: the cuts (the kickers levelled, the start
// drop), the combing, the grooming and the cleared woods, each worked a box of the course at a
// time so a long course on the skew never walks the whole map's grid.
//
// ge:summary The course's preparation: cuts, combing, grooming and the cleared woods
// ge:keywords course-prep start-drop comb groom level-kickers clear-trees finish-arena
// ge:skill mountain
// ge:test tests/mountain_slalom_test.cpp
#include "mountain/mapgen/course_prep.hpp"

#include <cmath>
#include <functional>
#include <limits>
#include <optional>

#include "core/heightfield.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/kickers.hpp"
#include "mountain/mapgen/query.hpp"
#include "mountain/mapgen/rules.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
using ge::core::clamp;
using ge::core::Heightfield;
using ge::core::smoothstep;

namespace {

// How far down the course one box of cells reaches, m.
constexpr double kBox = 16;

// A cut down into the snow along the piste: how deep, m, `u` m down it from `at`, between `from`
// and `to`, its banks `ease` m wide either side.
struct Cut {
  double at;
  double from;
  double to;
  double ease;
  std::function<double(double)> depth;
};

// The cells of `field` round the piste between arcs `s0` and `s1` (sampled every 2 m), each
// station's box `pad` m past its half-width, clamped `margin` cells inside the grid.
struct Cells {
  int c0, c1, r0, r1;
};
Cells cells_round(const Level& level, const Heightfield& field, double s0, double s1, double pad,
                  int margin = 0) {
  double x0 = std::numeric_limits<double>::infinity();
  double x1 = -x0;
  double z0 = x0;
  double z1 = -x0;
  for (int k = 0; s0 + 2.0 * k <= s1; k++) {
    const TrackPoint p = track_point_at(level.track, s0 + 2.0 * k);
    const double r = p.width / 2 + pad;
    x0 = num::min(x0, p.x - r);
    x1 = num::max(x1, p.x + r);
    z0 = num::min(z0, p.z - r);
    z1 = num::max(z1, p.z + r);
  }
  const auto col = [&](double v, bool up) {
    const double f = (v - field.origin_x) / field.cell;
    return static_cast<int>(
        clamp(up ? std::ceil(f) : std::floor(f), margin, field.cols - 1 - margin));
  };
  const auto row = [&](double v, bool up) {
    const double f = (v - field.origin_z) / field.cell;
    return static_cast<int>(
        clamp(up ? std::ceil(f) : std::floor(f), margin, field.rows - 1 - margin));
  };
  return {col(x0, false), col(x1, true), row(z0, false), row(z1, true)};
}

double half_width_at(const Level& level, const TrackHit& hit) {
  return level.track.points[static_cast<size_t>(hit.index)].width / 2;
}

// THE START DROP below the stretch's wand (`P.drop`): as deep as steepens its first metres to the
// rule's grade (a smoothstep's steepest is 1.5 times its mean), or nothing where the hill is
// already that steep.
std::optional<Cut> start_drop(const Level& level, const Stretch& stretch, const CoursePrep& P) {
  const CoursePrep::Drop& D = P.drop;
  const double top = track_point_at(level.track, stretch.from + D.lip).y;
  const double low = track_point_at(level.track, stretch.from + D.lip + D.length).y;
  const double natural = (top - low) / D.length;
  const double deep = num::min(D.most, (num::max(0.0, D.grade - natural) * D.length) / 1.5);
  if (deep < 0.05) return std::nullopt;
  const double knee = D.lip + D.length;
  return Cut{stretch.from, D.lip, knee + D.ease, D.shoulder, [deep, D, knee](double u) {
               return u < knee ? deep * smoothstep(D.lip, knee, u)
                               : deep * (1 - smoothstep(knee, knee + D.ease, u));
             }};
}

// One cut taken out of the piste's line and the ground under it.
void apply_cut(const Level& level, const Cut& cut, Heightfield& ground,
               std::vector<TrackPoint>& points) {
  for (TrackPoint& p : points) {
    const double u = p.s - cut.at;
    if (u > cut.from && u < cut.to) p.y -= cut.depth(u);
  }
  const Cells b = cells_round(level, ground, cut.at + cut.from, cut.at + cut.to, cut.ease + 2);
  for (int r = b.r0; r <= b.r1; r++) {
    for (int c = b.c0; c <= b.c1; c++) {
      const double x = ground.origin_x + c * ground.cell;
      const double z = ground.origin_z + r * ground.cell;
      const TrackHit hit = nearest_track_point(level.track, x, z);
      const double u = hit.s - cut.at;
      if (u <= cut.from || u >= cut.to) continue;
      const double half = half_width_at(level, hit);
      const double weight = 1 - smoothstep(half, half + cut.ease, hit.distance);
      if (weight <= 0) continue;
      float& h = ground.data[static_cast<size_t>(r) * static_cast<size_t>(ground.cols) +
                             static_cast<size_t>(c)];
      h = static_cast<float>(h - weight * cut.depth(u));
    }
  }
}

// THE HILL COMBED SMOOTH: no race is set on a mogul field. Some faces (a black's, most of all)
// carry a lip every 5–7 m down the piste, and at a slalom's pace each one throws a racer off the
// snow. So the ground under the course and its banks, from the hut to halfway down the run-out,
// is taken to its own mean over `P.comb.reach` metres either side of each cell (a box twice a
// lip's length: the lips go and the pitch, a straight slope under any mean, stays), eased out
// over `P.comb.ease` past the piste's edge and over its first and last metres down it. The
// piste's own heights follow the ground under them.
void comb_stretch(const Level& level, const Stretch& stretch, const CoursePrep& P,
                  Heightfield& ground, std::vector<TrackPoint>& points) {
  const std::vector<float> src = ground.data;
  const int reach = static_cast<int>(num::max(1.0, num::round(P.comb.reach / ground.cell)));
  const double lo = stretch.from - P.stand;
  const double hi = stretch.to + P.outrun_length / 2;
  const double fade = P.comb.ease;
  std::vector<uint8_t> seen(ground.data.size(), 0);
  const double area = double((2 * reach + 1) * (2 * reach + 1));
  for (int box = 0; lo + kBox * box < hi; box++) {
    const double s0 = lo + kBox * box;
    const Cells b = cells_round(level, ground, s0, num::min(hi, s0 + kBox), P.comb.ease + 2, reach);
    for (int r = b.r0; r <= b.r1; r++) {
      for (int c = b.c0; c <= b.c1; c++) {
        const size_t i =
            static_cast<size_t>(r) * static_cast<size_t>(ground.cols) + static_cast<size_t>(c);
        if (seen[i] == 1) continue;
        seen[i] = 1;
        const TrackHit hit = nearest_track_point(level.track, ground.origin_x + c * ground.cell,
                                                 ground.origin_z + r * ground.cell);
        if (hit.s <= lo || hit.s >= hi) continue;
        const double half = half_width_at(level, hit);
        const double weight = (1 - smoothstep(half, half + P.comb.ease, hit.distance)) *
                              smoothstep(lo, lo + fade, hit.s) *
                              (1 - smoothstep(hi - fade, hi, hit.s));
        if (weight <= 0) continue;
        double sum = 0;
        for (int dr = -reach; dr <= reach; dr++) {
          const size_t row = static_cast<size_t>(r + dr) * static_cast<size_t>(ground.cols) +
                             static_cast<size_t>(c);
          for (int dc = -reach; dc <= reach; dc++) sum += src[row + static_cast<size_t>(dc)];
        }
        const double mean = sum / area;
        ground.data[i] = static_cast<float>(src[i] + weight * (mean - src[i]));
      }
    }
  }
  Heightfield before = ground;
  before.data = src;
  for (TrackPoint& p : points) {
    if (p.s <= lo || p.s >= hi) continue;
    p.y += ge::core::sample_field(ground, p.x, p.z) - ge::core::sample_field(before, p.x, p.z);
  }
}

// THE COURSE PREPARED HARD: a race is run on a piste groomed and set firm from the start hut to
// the end of the run-out, whatever drift (R17) the wind laid across it: the packed field (R10)
// brought back to the groomer's across the piste and its shoulders over that stretch, and the
// rest of the mountain left as it lies. Nothing when the course already lies on the groomer.
std::optional<std::vector<float>> groom_stretch(const Level& level, const Stretch& stretch,
                                                const CoursePrep& P) {
  const Heightfield& field = level.packed;
  if (field.data.empty()) return std::nullopt;
  const double fade = rules::track::shoulder_packed;
  const double lo = stretch.from - P.stand - 4;
  const double hi = stretch.to + P.outrun_length;
  std::optional<std::vector<float>> data;
  for (int box = 0; lo + kBox * box < hi; box++) {
    const double s0 = lo + kBox * box;
    const Cells b = cells_round(level, field, s0, num::min(hi, s0 + kBox), fade + 1);
    for (int r = b.r0; r <= b.r1; r++) {
      for (int c = b.c0; c <= b.c1; c++) {
        const TrackHit hit = nearest_track_point(level.track, field.origin_x + c * field.cell,
                                                 field.origin_z + r * field.cell);
        if (hit.s < lo || hit.s > hi) continue;
        const double half = half_width_at(level, hit);
        const double firm = 1 - smoothstep(half, half + fade, hit.distance);
        const size_t i =
            static_cast<size_t>(r) * static_cast<size_t>(field.cols) + static_cast<size_t>(c);
        if (firm <= (data ? (*data)[i] : field.data[i])) continue;
        if (!data) data = field.data;
        (*data)[i] = static_cast<float>(firm);
      }
    }
  }
  return data;
}

}  // namespace

Level prepare_course(const Level& level, const Stretch& stretch, const CoursePrep& P) {
  const double lo = stretch.from - P.clearance;
  const double hi = stretch.to + P.clearance + P.outrun_length;
  std::vector<Kicker> kept;
  std::vector<Cut> cuts;
  for (const Kicker& k : level.kickers) {
    const bool on = k.on_track && k.s && *k.s + k.landing > lo && *k.s - k.ramp < hi;
    if (!on || !k.s) {
      kept.push_back(k);
      continue;
    }
    cuts.push_back({*k.s, -k.ramp, k.landing, 6,
                    [k](double u) { return kicker_profile(k.height, k.ramp, k.landing, u); }});
  }
  if (auto drop = start_drop(level, stretch, P)) cuts.push_back(std::move(*drop));
  Level out = level;
  if (auto packed = groom_stretch(level, stretch, P)) out.packed.data = std::move(*packed);
  for (const Cut& cut : cuts) apply_cut(level, cut, out.ground, out.track.points);
  comb_stretch(level, stretch, P, out.ground, out.track.points);
  out.kickers = std::move(kept);
  return out;
}

std::vector<TreeDef> cleared_trees(const Level& level, const Stretch& stretch,
                                   const CoursePrep& P) {
  const CoursePrep::Arena& A = P.arena;
  const TrackPoint fin = track_point_at(level.track, stretch.to);
  const double fx = libm::sin(fin.heading);
  const double fz = libm::cos(fin.heading);
  // The box round the course and its arena: a tree outside it is kept without asking where the
  // piste is.
  double x0 = std::numeric_limits<double>::infinity();
  double x1 = -x0;
  double z0 = x0;
  double z1 = -x0;
  for (int k = 0; stretch.from - 20 + 4.0 * k <= stretch.to + A.past; k++) {
    const TrackPoint p = track_point_at(level.track, stretch.from - 20 + 4.0 * k);
    const double r = num::max(p.width / 2 + P.clear, A.half) + 4;
    x0 = num::min(x0, p.x - r);
    x1 = num::max(x1, p.x + r);
    z0 = num::min(z0, p.z - r);
    z1 = num::max(z1, p.z + r);
  }
  std::vector<TreeDef> kept;
  kept.reserve(level.trees.size());
  for (const TreeDef& t : level.trees) {
    if (t.x < x0 || t.x > x1 || t.z < z0 || t.z > z1) {
      kept.push_back(t);
      continue;
    }
    const double dx = t.x - fin.x;
    const double dz = t.z - fin.z;
    const double along = dx * fx + dz * fz;
    const double side = dx * fz - dz * fx;
    if (along > -A.before && along < A.past && std::abs(side) < A.half) continue;
    const TrackHit hit = nearest_track_point(level.track, t.x, t.z);
    if (hit.s < stretch.from - 20 || hit.s > stretch.to + 10) {
      kept.push_back(t);
      continue;
    }
    if (hit.distance > half_width_at(level, hit) + P.clear) kept.push_back(t);
  }
  return kept;
}

}  // namespace ge::mountain::mapgen
