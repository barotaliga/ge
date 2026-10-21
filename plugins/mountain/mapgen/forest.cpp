// The definitions behind forest.hpp.
//
// ge:summary Grows the single piste's woods: the clumps first, then the scan
// ge:keywords forest clump lanes
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#include "mountain/mapgen/forest.hpp"

#include <array>
#include <cmath>
#include <unordered_map>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/noise.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/cliffs.hpp"
#include "mountain/mapgen/kickers.hpp"
#include "mountain/mapgen/query.hpp"
#include "mountain/mapgen/regions.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace F = rules::forest;
using ge::core::cell_key;
using ge::core::hash2;
using ge::core::hypot;
using ge::core::kPi;
using ge::core::sample_field;
using ge::core::smoothstep;
using ge::core::value_noise;

double tree_age(double grew, double x, double z, double seed) {
  namespace A = F::age;
  const double h = num::min(grew, A::top * 0.97);
  const double typical = -libm::log(1 - h / A::top) / A::rate;
  const double ix = std::floor(x * 8);
  const double iz = std::floor(z * 8);
  const double normal =
      (hash2(ix, iz, seed + 41) + hash2(ix, iz, seed + 42) + hash2(ix, iz, seed + 43) - 1.5) * 2;
  double age = typical * libm::exp(A::spread * normal);
  if (hash2(ix, iz, seed + 44) < A::veterans) age *= A::veteran;
  return num::min(A::max, age);
}

double trunk_radius(double age) {
  namespace T = F::trunk;
  return T::floor + T::top * libm::pow(1 - libm::exp(-T::rate * num::max(0.0, age)), T::shape);
}

std::vector<TreeDef> grow_forest(ge::core::Rng& rng, const TerrainPlan& plan,
                                 const ge::core::Heightfield& ground, const Track& piste,
                                 std::span<const Kicker> kickers, std::span<const Cliff> cliffs,
                                 double line_y, const ForestOptions* resort) {
  const auto& W = plan.region->forest;
  const double density = F::density * W.density * F::open;
  const double meadow = num::min(1.0, F::meadow * W.meadow);
  // The krummholz band under the tree line, and the valley floor, as heights.
  const double stunt_y = line_y - F::krummholz;
  const double base_y = line_y - (plan.tree_line - plan.altitude);
  const double seed = ge::core::int_in(rng, 1, 1 << 30);
  struct Clearing {
    double x, z, r;
  };
  std::vector<Clearing> clearings;
  const int n_clear =
      ge::core::int_in(rng, int32_t(F::clearings_count.min), int32_t(F::clearings_count.max));
  for (int i = 0; i < n_clear; i++) {
    const double x = ge::core::range(rng, 200, plan.size - 200);
    const double z = ge::core::range(rng, 200, plan.size - 200);
    const double r = in_band(rng, F::clearings_radius);
    clearings.push_back({x, z, r});
  }
  // How wooded a spot is (0 glade … 1 the thick of a wood), and how far out of a clearing
  // (0 in one … 1 clear of them all).
  struct Woods {
    double woods, clear;
  };
  const auto woods_at = [&](double x, double z) {
    const double n = value_noise(x, z, F::scale, seed) * 0.7 +
                     value_noise(x, z, F::scale * 0.35, seed + 101) * 0.3;
    double clear = 1;
    for (const Clearing& cl : clearings) {
      const double d = hypot(x - cl.x, z - cl.z);
      if (d < cl.r + 12) clear = num::min(clear, smoothstep(cl.r * 0.8, cl.r + 12, d));
    }
    return Woods{smoothstep(0.42, 0.58, n), clear};
  };
  // THE LANES: each family a set of parallel lines at a heading of its own, bent by a slow
  // noise; the distance to a line is the across-lane coordinate's distance to a multiple of
  // the spacing over that coordinate's gradient.
  namespace L = F::lanes;
  struct Lane {
    double cos, sin, offset, noise;
  };
  std::array<Lane, L::families> lanes{};
  for (int j = 0; j < L::families; j++) {
    lanes[size_t(j)] = {libm::cos(hash2(j, 1, seed) * kPi), libm::sin(hash2(j, 1, seed) * kPi),
                        hash2(j, 2, seed) * L::spacing, seed + 300 + j};
  }
  const auto bend = [](double x, double z, double noise) {
    return (value_noise(x, z, L::scale, noise) - 0.5) * 2 * L::swing;
  };
  const auto on_lane = [&](double x, double z) {
    for (const Lane& lane : lanes) {
      const double w = bend(x, z, lane.noise);
      const double u = x * lane.cos + z * lane.sin + w - lane.offset;
      const double d = std::abs(u - L::spacing * num::round(u / L::spacing));
      if (d > L::width) continue;
      const double gx = lane.cos + (bend(x + 1, z, lane.noise) - w);
      const double gz = lane.sin + (bend(x, z + 1, lane.noise) - w);
      if (d / num::max(0.2, hypot(gx, gz)) < L::width / 2) return true;
    }
    return false;
  };
  std::vector<TreeDef> trees;
  TrackHit hit;
  const double reach = rules::track::width.max / 2 + F::corridor;
  // Whether no tree may stand here at all; `y` is the ground's height.
  const auto refused = [&](double x, double z, double y) {
    if (y > line_y) return true;
    const ge::core::FieldSlope g = ge::core::field_gradient(ground, x, z);
    if (hypot(g.gx, g.gz) > F::max_slope) return true;
    if (resort) {
      if (resort->clear(x, z)) return true;
    } else {
      nearest_within(piste, x, z, reach, hit);
      if (hit.distance < piste.points[size_t(hit.index)].width / 2 + F::corridor) return true;
    }
    if (on_kicker(kickers, x, z, 4)) return true;
    if (on_cliff(cliffs, x, z, 4)) return true;
    // On a resort the runs are the cuts through the woods.
    return resort ? false : on_lane(x, z);
  };
  // THE GAP: every tree kept is filed in a hash of `gap`-sized buckets, so a candidate asks
  // only the nine round it whether one stands too near.
  const double gap2 = F::gap * F::gap;
  const double clump_gap2 = F::clumps::gap * F::clumps::gap;
  std::unordered_map<int64_t, std::vector<uint32_t>> buckets;
  const auto bucket_of = [](double v) { return static_cast<int32_t>(std::floor(v / F::gap)); };
  const auto crowded = [&](double x, double z, int32_t clump) {
    const int32_t bx = bucket_of(x);
    const int32_t bz = bucket_of(z);
    for (int dz = -1; dz <= 1; dz++) {
      for (int dx = -1; dx <= 1; dx++) {
        const auto it = buckets.find(cell_key(bx + dx, bz + dz));
        if (it == buckets.end()) continue;
        for (const uint32_t i : it->second) {
          const TreeDef& t = trees[i];
          const double need = clump >= 0 && t.clump == clump ? clump_gap2 : gap2;
          const double ex = t.x - x;
          const double ez = t.z - z;
          if (ex * ex + ez * ez < need) return true;
        }
      }
    }
    return false;
  };
  const auto stand = [&](double x, double z, double y, double height, int32_t clump, double kx,
                         double kz) {
    // Aged off the height it would have grown to unstunted: a krummholz tree is as old as the
    // tall one, only beaten down.
    const double stunt = 1 - (1 - F::krummholz_height) * smoothstep(stunt_y, line_y, y);
    const double age = tree_age(height / num::max(1e-6, stunt), x, z, seed);
    trees.push_back(TreeDef{x, z, y, height, trunk_radius(age), age,
                            num::min(F::crown_max, F::crown * height),
                            tree_kind_at(*plan.region, kx, kz), clump});
    buckets[cell_key(bucket_of(x), bucket_of(z))].push_back(uint32_t(trees.size() - 1));
  };
  // Tall in the thick of a wood and low down, short at its edge and up the mountain, stunted
  // in the krummholz band.
  const auto height_of = [&](double woods, double size, double y) {
    const double up = num::max(0.0, num::min(1.0, (y - base_y) / (line_y - base_y)));
    const double tall = resort ? resort->tall(y) * (0.7 + 0.2 * woods + 0.1 * size)
                               : 0.35 + 0.45 * woods + 0.2 * size - 0.45 * up;
    const double stunt = 1 - (1 - F::krummholz_height) * smoothstep(stunt_y, line_y, y);
    const double h = F::height.min + (F::height.max - F::height.min) *
                                         num::max(0.0, num::min(1.0, tall)) * W.height;
    return num::max(F::height.min, h * stunt);
  };

  // THE CLUMPS, before the scan: one chance a cell, off the forest's seed.
  namespace C = F::clumps;
  const int clump_cells = static_cast<int>(std::floor(plan.size / C::spacing));
  int32_t id = 0;
  for (int r = 0; r < clump_cells; r++) {
    for (int c = 0; c < clump_cells; c++) {
      const double cx = (c + 0.2 + 0.6 * hash2(c, r, seed + 11)) * C::spacing;
      const double cz = (r + 0.2 + 0.6 * hash2(c, r, seed + 12)) * C::spacing;
      const Woods wc = woods_at(cx, cz);
      double woods = wc.woods;
      double odds = 1;
      if (resort) {
        const WoodsCover cover = resort->cover(sample_field(ground, cx, cz));
        woods = wc.woods + (1 - wc.woods) * cover.close;
        odds = cover.keep;
      }
      odds *= (C::meadow + (C::woods - C::meadow) * woods) * wc.clear * W.density;
      if (hash2(c, r, seed + 13) >= odds) continue;
      const double want =
          C::trees.min + std::floor(hash2(c, r, seed + 14) * (C::trees.max - C::trees.min + 1));
      const double size = hash2(c, r, seed + 15);
      double grown = 0;
      // Tries round the centre, nearest first, until the clump is whole.
      for (double k = 0; k < want * 4 && grown < want; k++) {
        const double a = hash2(k, id, seed + 16) * kPi * 2;
        const double d = C::radius * std::sqrt((k + hash2(k, id, seed + 17)) / (want * 4));
        const double x = cx + libm::cos(a) * d;
        const double z = cz + libm::sin(a) * d;
        const double y = sample_field(ground, x, z);
        if (crowded(x, z, id) || refused(x, z, y)) continue;
        const double own = size * 0.8 + hash2(k, id, seed + 18) * 0.2;
        stand(x, z, y, height_of(woods, own, y), id, cx, cz);
        grown++;
      }
      if (grown > 0) id++;
    }
  }

  // THE SCAN: a candidate a cell, four draws a cell whatever happens.
  const int cells = static_cast<int>(std::floor(plan.size / F::spacing));
  for (int r = 0; r < cells; r++) {
    for (int c = 0; c < cells; c++) {
      const double jx = ge::core::next(rng);
      const double jz = ge::core::next(rng);
      const double keep = ge::core::next(rng);
      const double size = ge::core::next(rng);
      const double x = (c + 0.1 + jx * 0.8) * F::spacing;
      const double z = (r + 0.1 + jz * 0.8) * F::spacing;
      const Woods wc = woods_at(x, z);
      const double y = sample_field(ground, x, z);
      double woods = wc.woods;
      double share = 0;
      if (resort) {
        // R14 on a resort: the woods by height, closed low down, thinning through the ecotone
        // to the tree line.
        const WoodsCover cover = resort->cover(y);
        woods = wc.woods + (1 - wc.woods) * cover.close;
        share = density * (meadow + (1 - meadow) * woods * wc.clear) * cover.keep;
      } else {
        share = density * (meadow + (1 - meadow) * wc.woods * wc.clear);
        // The krummholz thins toward the line as well as stunting.
        share *= 1 - 0.6 * smoothstep(stunt_y, line_y, y);
      }
      if (keep >= share) continue;
      // Both refusals are pure, so the cheap one asks first.
      if (crowded(x, z, -1)) continue;
      if (refused(x, z, y)) continue;
      stand(x, z, y, height_of(woods, size, y), -1, x, z);
    }
  }
  return trees;
}

}  // namespace ge::mountain::mapgen
