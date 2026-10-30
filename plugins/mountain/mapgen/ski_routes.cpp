// The definitions behind ski_routes.hpp: the grid, the two searches, the steep squares, the
// judging, and the threading of a wild line through the trunks.
//
// ge:summary R42 the ski routes' search, judging, stations and threading
// ge:keywords ski-route route-search dijkstra steep-square route-threading viterbi drops-on
// ge:skill mountain
// ge:test tests/mountain_ski_routes_test.cpp
#include "mountain/mapgen/ski_routes.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ai/path.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "core/parallel.hpp"
#include "mountain/mapgen/resort_rules.hpp"
#include "mountain/mapgen/rules.hpp"

namespace ge::mountain::mapgen {

namespace {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace K = resort_rules::route;
namespace W = resort_rules::route::wild;
using ge::core::hypot;
using ge::core::kPi;
using ge::core::sample_field;

constexpr double kInf = std::numeric_limits<double>::infinity();

// The neighbours a square steps to: the eight round it and the eight a knight's move off, so a
// line may run at any of sixteen bearings; then a wild line's DROPS, two squares straight or
// across over ground past the cliff's pitch.
constexpr std::array<std::array<int, 2>, 24> kMoves{{
    {1, 0}, {-1, 0}, {0, 1},  {0, -1},  {1, 1}, {1, -1}, {-1, 1}, {-1, -1},
    {2, 1}, {2, -1}, {-2, 1}, {-2, -1}, {1, 2}, {1, -2}, {-1, 2}, {-1, -2},
    {2, 0}, {-2, 0}, {0, 2},  {0, -2},  {2, 2}, {2, -2}, {-2, 2}, {-2, -2},
}};
constexpr size_t kSteps = 16;

bool is_drop(int di, int dj) {
  const int ai = di < 0 ? -di : di;
  const int aj = dj < 0 ? -dj : dj;
  return std::max(ai, aj) == 2 && (di == 0 || dj == 0 || (ai == 2 && aj == 2));
}

struct Plan {
  double x;
  double z;
};

// The walked squares as stations: the line through their middles smoothed (`passes` times, its
// ends held) and laid every `route::station` m, with its arc. Where each stands on the snow, the
// marked corridor's width there and its heading come after (`on_snow`, `head`), so a line refused
// for its length never pays for them.
std::vector<TrackPoint> stations_of(const std::vector<Plan>& raw, int passes) {
  std::vector<Plan> pts = raw;
  for (int pass = 0; pass < passes; pass++) {
    const std::vector<Plan> was = pts;
    for (size_t i = 1; i + 1 < was.size(); i++) {
      pts[i] = {(was[i - 1].x + 2 * was[i].x + was[i + 1].x) / 4,
                (was[i - 1].z + 2 * was[i].z + was[i + 1].z) / 4};
    }
  }
  std::vector<TrackPoint> out;
  const auto at = [&](double x, double z, double s) { out.push_back({x, z, 0, s, 0, 0}); };
  at(pts[0].x, pts[0].z, 0);
  double s = 0;
  double next = K::station;
  for (size_t i = 1; i < pts.size(); i++) {
    const Plan& a = pts[i - 1];
    const Plan& c = pts[i];
    const double len = hypot(c.x - a.x, c.z - a.z);
    while (next <= s + len) {
      const double u = (next - s) / len;
      at(a.x + (c.x - a.x) * u, a.z + (c.z - a.z) * u, next);
      next += K::station;
    }
    s += len;
  }
  return out;
}

// Each station's height on the snow and the corridor's width there.
template <typename WidthAt>
void on_snow(std::vector<TrackPoint>& points, const ge::core::Heightfield& g, WidthAt&& width_at) {
  for (TrackPoint& p : points) {
    p.y = sample_field(g, p.x, p.z);
    p.width = width_at(p.x, p.z);
  }
}

// Each station's heading, from the one before it to the one after.
void head(std::vector<TrackPoint>& out) {
  for (size_t i = 0; i < out.size(); i++) {
    const TrackPoint& a = out[i == 0 ? 0 : i - 1];
    const TrackPoint& c = out[std::min(out.size() - 1, i + 1)];
    out[i].heading = libm::atan2(c.x - a.x, c.z - a.z);
  }
}

// A drop step's plan ends: from (x0, z0) over to (x1, z1).
struct DropStep {
  double x0, z0, x1, z1;
};

// The DROPS a wild line goes over: each drop step's top and every cliff's face it crosses, by its
// arc and how far it falls; those within `kDropSpan` of each other are one.
std::vector<SkiRoute::Drop> drops_on(std::span<const TrackPoint> points,
                                     std::span<const DropStep> steps,
                                     std::span<const Cliff> cliffs) {
  const auto nearest = [&](double x, double z) {
    size_t at = 0;
    double d = kInf;
    for (size_t i = 0; i < points.size(); i++) {
      const double e = hypot(points[i].x - x, points[i].z - z);
      if (e < d) {
        d = e;
        at = i;
      }
    }
    return at;
  };
  std::vector<SkiRoute::Drop> found;
  for (const DropStep& st : steps) {
    const size_t a = nearest(st.x0, st.z0);
    const size_t c = std::max(a, nearest(st.x1, st.z1));
    found.push_back({points[a].s, num::max(0.0, points[a].y - points[c].y)});
  }
  for (const Cliff& cl : cliffs) {
    const double fx = libm::sin(cl.heading);
    const double fz = libm::cos(cl.heading);
    for (size_t i = 1; i < points.size(); i++) {
      const double u0 = (points[i - 1].x - cl.x) * fx + (points[i - 1].z - cl.z) * fz;
      const double u1 = (points[i].x - cl.x) * fx + (points[i].z - cl.z) * fz;
      const double v = std::fabs((points[i].x - cl.x) * fz - (points[i].z - cl.z) * fx);
      if (u0 < 0 && u1 >= 0 && v < cl.width / 2) found.push_back({points[i - 1].s, cl.drop});
    }
  }
  std::stable_sort(found.begin(), found.end(),
                   [](const SkiRoute::Drop& a, const SkiRoute::Drop& c) { return a.s < c.s; });
  std::vector<SkiRoute::Drop> out;
  for (const SkiRoute::Drop& d : found) {
    if (!out.empty() && d.s - out.back().s < kDropSpan) {
      out.back().height = num::max(out.back().height, d.height);
    } else {
      out.push_back(d);
    }
  }
  return out;
}

// The offsets across a line its threading tries, m, and what moving it across costs a station
// and from one station to the next.
constexpr int kAcross = 6;
constexpr double kCentre = 0.03;
constexpr double kSway = 0.4;

// A line THREADED through the trunks round it: each station laid up to `kAcross` m across it, a
// metre at most from the last, to stand as clear of the bark as it can (a Viterbi walk over the
// offsets), the line's own middle and a steady line costing a little. Both ends stay pinned.
// The trunks by `kTreeCell`-m square, each square's in the trees' order: filed once a map, and
// read by every line threaded through them.
constexpr double kTreeCell = 8;
using TreeGrid = std::unordered_map<int64_t, std::vector<size_t>>;

int64_t tree_square(int i, int j) {
  return int64_t(uint64_t(uint32_t(i)) << 32 | uint64_t(uint32_t(j)));
}

TreeGrid tree_grid(std::span<const TreeDef> trees) {
  TreeGrid grid;
  for (size_t t = 0; t < trees.size(); t++) {
    grid[tree_square(int(std::floor(trees[t].x / kTreeCell)),
                     int(std::floor(trees[t].z / kTreeCell)))]
        .push_back(t);
  }
  return grid;
}

std::vector<Plan> thread_trees(std::span<const TrackPoint> points, std::span<const TreeDef> trees,
                               const TreeGrid& grid) {
  constexpr double kG = kTreeCell;
  const auto bark = [&](double x, double z) {
    double cost = 0;
    const int i = int(std::floor(x / kG));
    const int j = int(std::floor(z / kG));
    for (int dj = -1; dj <= 1; dj++) {
      for (int di = -1; di <= 1; di++) {
        const auto it = grid.find(tree_square(i + di, j + dj));
        if (it == grid.end()) continue;
        for (const size_t k : it->second) {
          const TreeDef& t = trees[k];
          const double into = K::open + t.radius + 1 - hypot(t.x - x, t.z - z);
          if (into > 0) cost += 4 * into;
        }
      }
    }
    return cost;
  };
  constexpr int kM = 2 * kAcross + 1;
  const size_t n = points.size();
  std::vector<double> cost(n * kM);
  std::vector<uint8_t> from(n * kM, 0);
  const auto at = [&](size_t i, int o) {
    const TrackPoint& p = points[i];
    const double nx = libm::cos(p.heading);
    const double nz = -libm::sin(p.heading);
    return Plan{p.x + nx * double(o - kAcross), p.z + nz * double(o - kAcross)};
  };
  for (size_t i = 0; i < n; i++) {
    const bool pinned = i < 3 || i + 4 > n;
    for (int o = 0; o < kM; o++) {
      double here = kInf;
      if (!pinned || o == kAcross) {
        const Plan p = at(i, o);
        here = bark(p.x, p.z) + kCentre * double((o - kAcross) * (o - kAcross));
      }
      if (i == 0) {
        cost[size_t(o)] = here;
        continue;
      }
      double best = kInf;
      int arg = o;
      for (int q = std::max(0, o - 1); q <= std::min(kM - 1, o + 1); q++) {
        const double c = cost[(i - 1) * kM + size_t(q)] + kSway * double(q > o ? q - o : o - q);
        if (c < best) {
          best = c;
          arg = q;
        }
      }
      cost[i * kM + size_t(o)] = best + here;
      from[i * kM + size_t(o)] = static_cast<uint8_t>(arg);
    }
  }
  std::vector<Plan> out(n);
  int o = kAcross;
  for (size_t i = n; i-- > 0;) {
    out[i] = at(i, o);
    o = from[i * kM + size_t(o)];
  }
  return out;
}

}  // namespace

double steepest_along(std::span<const TrackPoint> points, double span,
                      std::span<const SkiRoute::Drop> drops) {
  double best = 0;
  size_t j = 0;
  for (size_t i = 0; i < points.size(); i++) {
    while (j < points.size() && points[j].s - points[i].s < span) j++;
    if (j >= points.size()) break;
    const double s0 = points[i].s;
    const double s1 = points[j].s;
    const bool excused = std::any_of(drops.begin(), drops.end(), [&](const SkiRoute::Drop& d) {
      return s1 > d.s - kDropSpan && s0 < d.s + kDropSpan;
    });
    if (excused) continue;
    best = num::max(best, (points[i].y - points[j].y) / (s1 - s0));
  }
  return best;
}

std::vector<SkiRoute> lay_ski_routes(const RouteGround& area, bool wild) {
  const ge::core::Heightfield& ground = area.ground;
  const auto g = [&](double x, double z) { return sample_field(ground, x, z); };
  const double cell = K::cell;
  const int n = int(std::floor(area.size / cell));
  const int big_n = n * n;
  const auto N = size_t(big_n);
  const auto cx = [&](int k) { return double(k % n) * cell + cell / 2; };
  const auto cz = [&](int k) {
    const int row = k / n;
    return double(row) * cell + cell / 2;
  };
  const auto square = [&](double x, double z) {
    return int(std::floor(z / cell)) * n + int(std::floor(x / cell));
  };
  // Every square's reading below is its own, so the grids are read side by side.
  std::vector<double> y(N);
  ge::core::parallel_bands(N, [&](size_t lo, size_t hi) {
    for (size_t k = lo; k < hi; k++) y[k] = g(cx(int(k)), cz(int(k)));
  });

  // THE TREES on a fine grid: every two-metre square a trunk's bark comes within `route::open` of.
  constexpr double kFine = 2;
  const int fn = int(std::ceil(area.size / kFine));
  std::vector<uint8_t> wooded(size_t(fn) * size_t(fn), 0);
  for (const TreeDef& t : area.trees) {
    const double reach = K::open + t.radius;
    const int i0 = std::max(0, int(std::floor((t.x - reach) / kFine)));
    const int i1 = std::min(fn - 1, int(std::floor((t.x + reach) / kFine)));
    const int j0 = std::max(0, int(std::floor((t.z - reach) / kFine)));
    const int j1 = std::min(fn - 1, int(std::floor((t.z + reach) / kFine)));
    for (int j = j0; j <= j1; j++) {
      for (int i = i0; i <= i1; i++) {
        if (hypot((i + 0.5) * kFine - t.x, (j + 0.5) * kFine - t.z) <= reach) {
          wooded[size_t(j) * size_t(fn) + size_t(i)] = 1;
        }
      }
    }
  }
  const auto in_trees = [&](double x, double z) {
    const int i = int(std::floor(x / kFine));
    const int j = int(std::floor(z / kFine));
    return i < 0 || j < 0 || i >= fn || j >= fn || wooded[size_t(j) * size_t(fn) + size_t(i)] == 1;
  };

  // THE RUNS: every square a run's corridor covers, which run and at what arc of it; and every
  // square within `route::clear` of one.
  std::vector<int> run_of(N, -1);
  std::vector<double> run_at(N, 0);
  std::vector<uint8_t> near_run(N, 0);
  for (size_t ri = 0; ri < area.runs.size(); ri++) {
    for (const TrackPoint& p : area.runs[ri].points) {
      const double reach = p.width / 2 + K::clear;
      const int i0 = std::max(0, int(std::floor((p.x - reach) / cell)));
      const int i1 = std::min(n - 1, int(std::floor((p.x + reach) / cell)));
      const int j0 = std::max(0, int(std::floor((p.z - reach) / cell)));
      const int j1 = std::min(n - 1, int(std::floor((p.z + reach) / cell)));
      for (int j = j0; j <= j1; j++) {
        for (int i = i0; i <= i1; i++) {
          const int k = j * n + i;
          const double d = hypot(cx(k) - p.x, cz(k) - p.z);
          if (d <= reach) near_run[size_t(k)] = 1;
          if (d <= p.width / 2 && run_of[size_t(k)] < 0) {
            run_of[size_t(k)] = int(ri);
            run_at[size_t(k)] = p.s;
          }
        }
      }
    }
  }

  // THE GROUND A SKIER MAY STAND ON: inside the map's margin, off the cliffs, out from under every
  // lift's rope, and on a straight line out of the trees; a wild line goes through them, felling
  // what stands on it.
  const auto under_lift = [&](double x, double z) {
    return std::any_of(area.lifts.begin(), area.lifts.end(), [&](const Lift& l) {
      const double dx = l.top.x - l.bottom.x;
      const double dz = l.top.z - l.bottom.z;
      double len2 = dx * dx + dz * dz;
      if (len2 == 0) len2 = 1;
      const double u =
          ge::core::clamp(((x - l.bottom.x) * dx + (z - l.bottom.z) * dz) / len2, 0.0, 1.0);
      return hypot(x - l.bottom.x - dx * u, z - l.bottom.z - dz * u) < K::lift;
    });
  };
  std::vector<uint8_t> open(N, 0);
  std::vector<uint8_t> crag(N, 0);
  std::vector<double> fall_x(N, 0);
  std::vector<double> fall_z(N, 0);
  std::vector<double> fall_m(N, 0);
  const double b = K::baseline / 2;
  const auto fall_at = [&](double x, double z) {
    return std::pair{(g(x + b, z) - g(x - b, z)) / (2 * b), (g(x, z + b) - g(x, z - b)) / (2 * b)};
  };
  ge::core::parallel_bands(N, [&](size_t lo, size_t hi) {
    for (size_t k = lo; k < hi; k++) {
      const double x = cx(int(k));
      const double z = cz(int(k));
      if (x < K::edge || z < K::edge || x > area.size - K::edge || z > area.size - K::edge) {
        continue;
      }
      const auto [gx, gz] = fall_at(x, z);
      fall_x[k] = gx;
      fall_z[k] = gz;
      fall_m[k] = hypot(gx, gz);
      if (under_lift(x, z)) continue;
      if (fall_m[k] > K::cliff) {
        crag[k] = 1;
        continue;
      }
      if (!wild && in_trees(x, z)) continue;
      open[k] = 1;
    }
  });

  // WHAT A WILD LINE IS DRAWN TO: the WOODS, and the EDGES (a square beside ground past the
  // cliff's pitch, or on a cliff's shelf near its edge), and over a cliff's FACE, a drop. Each
  // square's cost per metre.
  std::vector<uint8_t> woods(N, 0);
  std::vector<uint8_t> edge(N, 0);
  std::vector<uint8_t> face(N, 0);
  std::vector<double> share(N, 1);
  if (wild) {
    for (const TreeDef& t : area.trees) {
      const int i0 = std::max(0, int(std::floor((t.x - W::near) / cell)));
      const int i1 = std::min(n - 1, int(std::floor((t.x + W::near) / cell)));
      const int j0 = std::max(0, int(std::floor((t.z - W::near) / cell)));
      const int j1 = std::min(n - 1, int(std::floor((t.z + W::near) / cell)));
      for (int j = j0; j <= j1; j++) {
        for (int i = i0; i <= i1; i++) {
          const int k = j * n + i;
          if (hypot(cx(k) - t.x, cz(k) - t.z) <= W::near) woods[size_t(k)] = 1;
        }
      }
    }
    ge::core::parallel_bands(N, [&](size_t lo, size_t hi) {
      for (size_t k = lo; k < hi; k++) {
        if (!open[k]) continue;
        const int i = int(k) % n;
        const int j = int(k) / n;
        for (int dj = -1; dj <= 1 && !edge[k]; dj++) {
          for (int di = -1; di <= 1; di++) {
            const int ii = i + di;
            const int jj = j + dj;
            if (ii >= 0 && jj >= 0 && ii < n && jj < n &&
                crag[size_t(jj) * size_t(n) + size_t(ii)]) {
              edge[k] = 1;
            }
          }
        }
      }
    });
    for (const Cliff& c : area.cliffs) {
      const double fx = libm::sin(c.heading);
      const double fz = libm::cos(c.heading);
      const double reach = num::max(W::shelf, c.face + 4) + c.width / 2;
      const int i0 = std::max(0, int(std::floor((c.x - reach) / cell)));
      const int i1 = std::min(n - 1, int(std::floor((c.x + reach) / cell)));
      const int j0 = std::max(0, int(std::floor((c.z - reach) / cell)));
      const int j1 = std::min(n - 1, int(std::floor((c.z + reach) / cell)));
      for (int j = j0; j <= j1; j++) {
        for (int i = i0; i <= i1; i++) {
          const int k = j * n + i;
          const double dx = cx(k) - c.x;
          const double dz = cz(k) - c.z;
          const double u = dx * fx + dz * fz;
          const double v = std::fabs(dx * fz - dz * fx);
          if (v > c.width / 2) continue;
          if (u >= -W::shelf && u < 0) {
            edge[size_t(k)] = 1;
          } else if (u >= 0 && u <= c.face + cell / 2) {
            face[size_t(k)] = 1;
          }
        }
      }
    }
    for (size_t k = 0; k < N; k++) {
      if (face[k]) {
        share[k] = W::huck;
      } else if (edge[k]) {
        share[k] = W::edge;
      } else if (woods[k]) {
        share[k] = W::woods;
      }
    }
  }
  // Each move's length in squares and whether it is a drop, worked once.
  std::array<double, kMoves.size()> unit{};
  std::array<bool, kMoves.size()> drops_move{};
  for (size_t m = 0; m < kMoves.size(); m++) {
    unit[m] = hypot(double(kMoves[m][0]), double(kMoves[m][1]));
    drops_move[m] = is_drop(kMoves[m][0], kMoves[m][1]);
  }
  // Whether square c, `len` m from square a, is open ground below it: no climb, no pitch past the
  // cliff's.
  const auto downhill = [&](int a, int c, double len) {
    if (!open[size_t(c)]) return false;
    const double drop = y[size_t(a)] - y[size_t(c)];
    return drop >= 0 && drop / len <= K::cliff;
  };
  // Metres of the step from square a to square c a trunk would be felled for, and whether the
  // trees stand clear all along it.
  const auto walk_felled = [&](int a, int c, double len) {
    double m = 0;
    const double x0 = cx(a);
    const double z0 = cz(a);
    const double x1 = cx(c);
    const double z1 = cz(c);
    for (int t = 1; double(t) * kFine < len; t++) {
      const double u = double(t) * kFine / len;
      if (in_trees(x0 + (x1 - x0) * u, z0 + (z1 - z0) * u)) m += kFine;
    }
    return m;
  };
  // What a step costs: its length on a straight line; on a wild one, by the squares it crosses,
  // dearer down the fall line, and dearer for every metre of trunk it fells.
  const auto cost_of = [&](int a, int c, int di, int dj, double len, size_t move, double felled) {
    if (!wild) return len;
    const double gx = fall_x[size_t(a)];
    const double gz = fall_z[size_t(a)];
    const double m = fall_m[size_t(a)];
    const double down = m > 1e-6 ? num::max(0.0, -(gx * di + gz * dj) / (m * unit[move])) : 0.0;
    const double pull = 1 + W::fall * down * down * num::min(1.0, m / 0.3);
    return (share[size_t(a)] + share[size_t(c)]) / 2 * pull * len + W::fell * felled;
  };
  // Whether a wild line may DROP from a over the square between to c: both open, ground past the
  // cliff's pitch between, falling no more than `wild::drop`.
  const auto drop_over = [&](int a, int c, int mid) {
    if (!open[size_t(a)] || !open[size_t(c)] || !crag[size_t(mid)]) return false;
    const double drop = y[size_t(a)] - y[size_t(c)];
    return drop >= 0 && drop <= W::drop;
  };
  const size_t moves = wild ? kMoves.size() : kSteps;
  // Both searches weigh the same steps again and again, so every step's cost is worked once, by
  // its square and its move, the squares side by side, and kept; infinite where a skier may not
  // take it: a step not downhill, a straight line's through the trees, a drop that is not one.
  // The cost kept is the one the searches would have worked, so keeping it changes no bits.
  std::vector<double> step_cost(N * moves);
  ge::core::parallel_bands(N, [&](size_t lo, size_t hi) {
    for (size_t k = lo; k < hi; k++) {
      const int i = int(k) % n;
      const int j = int(k) / n;
      for (size_t m = 0; m < moves; m++) {
        double& out = step_cost[k * moves + m];
        out = kInf;
        const int di = kMoves[m][0];
        const int dj = kMoves[m][1];
        const int ii = i + di;
        const int jj = j + dj;
        if (ii < 0 || jj < 0 || ii >= n || jj >= n) continue;
        const int c = jj * n + ii;
        const double len = unit[m] * cell;
        if (drops_move[m]) {
          if (drop_over(int(k), c, (j + dj / 2) * n + (i + di / 2))) out = len * W::huck;
          continue;
        }
        if (!downhill(int(k), c, len)) continue;
        const double felled = walk_felled(int(k), c, len);
        if (!wild && felled != 0) continue;
        out = cost_of(int(k), c, di, dj, len, m, felled);
      }
    }
  });

  // The two searches read the grids above and write only their own, so they run side by side.
  // FROM THE TOPS: the cheapest way down to every square, and its top.
  ge::ai::Search from(N);
  std::vector<int> from_top(N, -1);
  std::vector<uint8_t> dropped_in(N, 0);
  const auto from_tops = [&] {
    for (size_t li = 0; li < area.lifts.size(); li++) {
      const Lift& lift = area.lifts[li];
      if (lift.kind == LiftKind::drag) continue;
      for (int i = 0; i < K::bearings; i++) {
        const double a = double(i) / K::bearings * 2 * kPi;
        const double x = lift.top.x + libm::sin(a) * K::rim;
        const double z = lift.top.z + libm::cos(a) * K::rim;
        const int k = square(x, z);
        if (k < 0 || k >= big_n || !open[size_t(k)] || run_of[size_t(k)] >= 0 ||
            from.cost(k) == 0) {
          continue;
        }
        from.seed(k);
        from_top[size_t(k)] = int(li);
      }
    }
    // A square left again at a dearer cost than it was reached at since is skipped by the search:
    // every step from it was weighed at that cheaper cost already, and none of them is cheaper now.
    from.run_all([&](int a) {
      const int i = a % n;
      const int j = a / n;
      for (size_t m = 0; m < moves; m++) {
        const int di = kMoves[m][0];
        const int dj = kMoves[m][1];
        const int ii = i + di;
        const int jj = j + dj;
        if (ii < 0 || jj < 0 || ii >= n || jj >= n) continue;
        const int c = jj * n + ii;
        // A way off a top never crosses a run: it leaves the top on its own.
        if (run_of[size_t(c)] >= 0) continue;
        const double step = step_cost[size_t(a) * moves + m];
        if (step == kInf) continue;
        const bool drop = drops_move[m];
        const int mid = (j + dj / 2) * n + (i + di / 2);
        if (drop && run_of[size_t(mid)] >= 0) continue;
        const double cost = from.cost(a) + step;
        if (!from.improves(c, cost)) continue;
        from.reach(c, cost, a);
        from_top[size_t(c)] = from_top[size_t(a)];
        dropped_in[size_t(c)] = drop ? 1 : 0;
      }
    });
  };

  // TO THE RUNS: the cheapest way down onto a run from every square, read backwards from every
  // square of a run's corridor.
  ge::ai::Search to(N);
  std::vector<uint8_t> drops_out(N, 0);
  const auto to_runs = [&] {
    for (int k = 0; k < big_n; k++) {
      if (run_of[size_t(k)] < 0) continue;
      to.seed(k);
    }
    to.run_all([&](int c) {
      const int i = c % n;
      const int j = c / n;
      for (size_t m = 0; m < moves; m++) {
        const int di = kMoves[m][0];
        const int dj = kMoves[m][1];
        const int ii = i - di;
        const int jj = j - dj;
        if (ii < 0 || jj < 0 || ii >= n || jj >= n) continue;
        const int a = jj * n + ii;
        if (run_of[size_t(a)] >= 0) continue;
        const double step = step_cost[size_t(a) * moves + m];
        if (step == kInf) continue;
        const double cost = to.cost(c) + step;
        if (!to.improves(a, cost)) continue;
        to.reach(a, cost, c);
        drops_out[size_t(a)] = drops_move[m] ? 1 : 0;
      }
    });
  };
  ge::core::parallel_bands(2, [&](size_t lo, size_t hi) {
    for (size_t k = lo; k < hi; k++) k == 0 ? from_tops() : to_runs();
  });

  // THE STEEP SQUARES a route may be skied through: its fall line falling past a black's ceiling,
  // no more than a route's, over a colour window.
  const double half = rules::track::colour_window / 2;
  std::vector<uint8_t> is_steep(N, 0);
  ge::core::parallel_bands(N, [&](size_t lo, size_t hi) {
    for (size_t q = lo; q < hi; q++) {
      const int k = int(q);
      if (!from.reached(k) || !to.reached(k)) continue;
      const double x = cx(k);
      const double z = cz(k);
      const auto [gx, gz] = fall_at(x, z);
      const double m = hypot(gx, gz);
      if (m < 1e-6) continue;
      const double dx = -gx / m;
      const double dz = -gz / m;
      const double pitch =
          (g(x - dx * half, z - dz * half) - g(x + dx * half, z + dz * half)) / (2 * half);
      if (pitch > rules::track::max_grade && pitch <= K::steepest) is_steep[q] = 1;
    }
  });
  std::vector<int> steep;
  for (int k = 0; k < big_n; k++) {
    if (is_steep[size_t(k)]) steep.push_back(k);
  }

  struct Found {
    SkiRoute route;
    double score;
    int top;
    int end;
    std::vector<DropStep> steps;
  };
  // A line as a route held to R42, and its score; or nothing.
  const auto judge = [&](std::vector<TrackPoint> points, const std::vector<DropStep>& steps,
                         int top, int end) -> std::optional<Found> {
    const double length = points.back().s;
    const double vertical = points.front().y - points.back().y;
    if (length < K::length.min || length > K::length.max || vertical < K::vertical) {
      return std::nullopt;
    }
    const double steepest = steepest_along(points, rules::track::colour_window);
    if (steepest <= rules::track::max_grade || steepest > K::steepest) return std::nullopt;
    std::vector<SkiRoute::Drop> drops;
    if (wild) drops = drops_on(points, steps, area.cliffs);
    if (drops.size() > size_t(W::drops)) return std::nullopt;
    if (steepest_along(points, K::pitch_window, drops) > K::cliff) return std::nullopt;
    if (!wild && std::any_of(points.begin(), points.end(),
                             [&](const TrackPoint& p) { return in_trees(p.x, p.z); })) {
      return std::nullopt;
    }
    double beside = 0;
    double on_steep = 0;
    double in_woods = 0;
    double on_edge = 0;
    for (size_t i = 1; i < points.size(); i++) {
      const TrackPoint& p = points[i];
      const int sq = square(p.x, p.z);
      const double ds = p.s - points[i - 1].s;
      if (sq >= 0 && sq < big_n) {
        const auto k = size_t(sq);
        if (near_run[k] && run_of[k] < 0) beside += ds;
        if (woods[k]) in_woods += ds;
        if (edge[k] || face[k]) on_edge += ds;
      }
      const double drop = points[i - 1].y - p.y;
      if (drop / ds >= rules::grade::band_red) on_steep += drop;
    }
    if (beside > K::beside) return std::nullopt;
    const double score =
        wild ? on_steep + 0.05 * length + 0.4 * in_woods + 0.5 * on_edge + 30 * double(drops.size())
             : on_steep + 0.05 * length;
    head(points);
    SkiRoute route;
    route.points = std::move(points);
    route.length = length;
    route.from = area.lifts[size_t(top)].id;
    route.into = {area.runs[size_t(run_of[size_t(end)])].id, run_at[size_t(end)]};
    route.steepest = steepest;
    if (wild) {
      route.wild = true;
      route.drops = std::move(drops);
      route.woods = in_woods;
      route.edge = on_edge;
    }
    return Found{std::move(route), score, top, end, steps};
  };
  const auto width_at = [&](double x, double z) {
    const int sq = square(x, z);
    return wild && sq >= 0 && sq < big_n && woods[size_t(sq)] ? W::width : K::width;
  };
  // The line through a steep square, judged: a pure function of the square, so the squares are
  // judged side by side, and only each top's best is laid again to keep.
  const auto through = [&](int k) -> std::optional<Found> {
    std::vector<int> cells;
    std::vector<std::pair<int, int>> dropped;
    for (int a = k; a >= 0; a = from.prev(a)) {
      cells.push_back(a);
      if (dropped_in[size_t(a)] && from.prev(a) >= 0) {
        dropped.emplace_back(from.prev(a), a);
      }
    }
    std::reverse(cells.begin(), cells.end());
    for (int c = k; to.prev(c) >= 0; c = to.prev(c)) {
      cells.push_back(to.prev(c));
      if (drops_out[size_t(c)]) dropped.emplace_back(c, to.prev(c));
    }
    const int end = cells.back();
    if (run_of[size_t(end)] < 0) return std::nullopt;
    std::vector<Plan> raw;
    raw.reserve(cells.size());
    for (const int c : cells) raw.push_back({cx(c), cz(c)});
    std::vector<TrackPoint> points = stations_of(raw, wild ? W::smooth : K::smooth);
    // A line refused for its length is refused before it is laid on the snow.
    if (points.back().s < K::length.min || points.back().s > K::length.max) return std::nullopt;
    on_snow(points, ground, width_at);
    const int top = from_top[size_t(cells.front())];
    std::vector<DropStep> steps;
    steps.reserve(dropped.size());
    for (const auto& [a, c] : dropped) steps.push_back({cx(a), cz(a), cx(c), cz(c)});
    return judge(std::move(points), steps, top, end);
  };
  struct Judged {
    bool ok = false;
    double score = 0;
    int top = 0;
  };
  std::vector<Judged> judged(steep.size());
  ge::core::parallel_bands(steep.size(), [&](size_t lo, size_t hi) {
    for (size_t q = lo; q < hi; q++) {
      if (const std::optional<Found> f = through(steep[q])) judged[q] = {true, f->score, f->top};
    }
  });
  // Each top's best, the first of the highest score in the squares' order.
  std::map<int, size_t> winner;
  for (size_t q = 0; q < judged.size(); q++) {
    if (!judged[q].ok) continue;
    const auto had = winner.find(judged[q].top);
    if (had != winner.end() && judged[had->second].score >= judged[q].score) continue;
    winner.insert_or_assign(judged[q].top, q);
  }
  std::map<int, Found> best;
  for (const auto& [top, q] : winner) best.insert_or_assign(top, std::move(*through(steep[q])));
  std::vector<Found*> ranked;
  ranked.reserve(best.size());
  for (auto& [top, f] : best) ranked.push_back(&f);
  std::stable_sort(ranked.begin(), ranked.end(), [](const Found* a, const Found* c) {
    return a->score != c->score ? a->score > c->score : a->top < c->top;
  });
  std::vector<Found*> kept;
  for (Found* f : ranked) {
    if (kept.size() >= size_t(K::count.max)) break;
    const bool apart = std::all_of(kept.begin(), kept.end(), [&](const Found* o) {
      return std::all_of(f->route.points.begin(), f->route.points.end(), [&](const TrackPoint& p) {
        return p.s < K::lead || std::all_of(o->route.points.begin(), o->route.points.end(),
                                            [&](const TrackPoint& q) {
                                              return hypot(p.x - q.x, p.z - q.z) > K::apart;
                                            });
      });
    });
    if (apart) kept.push_back(f);
  }
  // A wild line THREADS the woods: laid across its corridor to stand clear of the trunks where
  // it can, kept where it still holds. Each line is threaded on its own, so side by side.
  std::vector<std::optional<Found>> threaded(kept.size());
  if (wild) {
    const TreeGrid grid = tree_grid(area.trees);
    ge::core::parallel_bands(kept.size(), [&](size_t lo, size_t hi) {
      for (size_t q = lo; q < hi; q++) {
        const Found* f = kept[q];
        const std::vector<Plan> raw = thread_trees(f->route.points, area.trees, grid);
        const std::vector<TrackPoint>& was = f->route.points;
        std::vector<TrackPoint> points = stations_of(raw, 0);
        on_snow(points, ground, [&](double x, double z) {
          double width = was.front().width;
          double d = kInf;
          for (const TrackPoint& p : was) {
            const double e = (p.x - x) * (p.x - x) + (p.z - z) * (p.z - z);
            if (e < d) {
              d = e;
              width = p.width;
            }
          }
          return width;
        });
        threaded[q] = judge(std::move(points), f->steps, f->top, f->end);
      }
    });
  }
  std::vector<SkiRoute> out;
  for (size_t q = 0; q < kept.size(); q++) {
    SkiRoute route = threaded[q] ? std::move(threaded[q]->route) : kept[q]->route;
    route.id = "SR" + std::to_string(out.size() + 1);
    out.push_back(std::move(route));
  }
  return out;
}

}  // namespace ge::mountain::mapgen
