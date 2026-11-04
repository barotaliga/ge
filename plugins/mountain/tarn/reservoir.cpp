// The valley's reservoir found and laid (reservoir.hpp).
//
// ge:summary find_reservoir and lay_reservoir: the dam sites tried, the flood, the shore, the bank
// ge:keywords reservoir find_reservoir lay_reservoir flood dam crest shore-outline clear-mask
// ge:skill mountain
// ge:test tests/mountain_reservoir_test.cpp
#include "mountain/tarn/reservoir.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <vector>

#include "core/heightfield.hpp"
#include "core/math.hpp"
#include "mountain/tarn/tarn.hpp"

namespace ge::mountain::tarn {

namespace {

namespace mg = mapgen;

constexpr double kPi = 3.141592653589793;
constexpr double kStep = 4;  // m between the crest's samples along it

double ground(const mg::Level& level, double x, double z) {
  return core::sample_field(level.ground, x, z);
}

// The map sampled every `kFloodCell` m: each sample's ground, and whether it lies within
// `kDamClear` m of what a reservoir must keep clear of.
struct Grid {
  int n = 0;
  std::vector<double> y;
  std::vector<uint8_t> kept;  // 1: must stay dry
  size_t at(int i, int j) const { return static_cast<size_t>(j) * n + static_cast<size_t>(i); }
  bool inside(int i, int j) const { return i >= 0 && j >= 0 && i < n && j < n; }
  double x(int i) const { return i * kFloodCell; }
};

void keep_round(Grid& g, double x, double z, double reach) {
  const int i0 = std::max(0, static_cast<int>(std::floor((x - reach) / kFloodCell)));
  const int i1 = std::min(g.n - 1, static_cast<int>(std::ceil((x + reach) / kFloodCell)));
  const int j0 = std::max(0, static_cast<int>(std::floor((z - reach) / kFloodCell)));
  const int j1 = std::min(g.n - 1, static_cast<int>(std::ceil((z + reach) / kFloodCell)));
  for (int j = j0; j <= j1; ++j) {
    for (int i = i0; i <= i1; ++i) {
      const double dx = g.x(i) - x, dz = g.x(j) - z;
      if (dx * dx + dz * dz <= reach * reach) g.kept[g.at(i, j)] = 1;
    }
  }
}

void keep_line(Grid& g, double ax, double az, double bx, double bz, double reach) {
  const double len = core::hypot(bx - ax, bz - az);
  const int steps = std::max(1, static_cast<int>(std::ceil(len / kFloodCell)));
  for (int k = 0; k <= steps; ++k) {
    const double t = static_cast<double>(k) / steps;
    keep_round(g, ax + t * (bx - ax), az + t * (bz - az), reach);
  }
}

Grid grid_of(const mg::Level& level) {
  Grid g;
  g.n = static_cast<int>(std::floor(level.size / kFloodCell)) + 1;
  g.y.resize(static_cast<size_t>(g.n) * g.n);
  g.kept.assign(g.y.size(), 0);
  for (int j = 0; j < g.n; ++j) {
    for (int i = 0; i < g.n; ++i) g.y[g.at(i, j)] = ground(level, g.x(i), g.x(j));
  }
  // Every 4th point of a line every 2 m is enough for a reach of 30 m.
  const auto keep_points = [&](const std::vector<mg::TrackPoint>& pts) {
    for (size_t k = 0; k < pts.size(); k += 4) keep_round(g, pts[k].x, pts[k].z, kDamClear);
    if (!pts.empty()) keep_round(g, pts.back().x, pts.back().z, kDamClear);
  };
  keep_points(level.track.points);
  if (level.resort) {
    const mg::ResortMap& area = *level.resort;
    for (const mg::Run& run : area.runs) keep_points(run.points);
    for (const mg::SkiRoute& route : area.routes) keep_points(route.points);
    for (const mg::Lift& lift : area.lifts)
      keep_line(g, lift.bottom.x, lift.bottom.z, lift.top.x, lift.top.z, kDamClear);
    // The hub and the village below it.
    double top = 1e30;
    for (const double z : area.hub.top) top = std::min(top, z);
    for (int j = 0; j < g.n; ++j) {
      if (g.x(j) < top - kDamClear) continue;
      for (int i = 0; i < g.n; ++i) g.kept[g.at(i, j)] = 1;
    }
  }
  for (const mg::MapLake& l : level.lakes) keep_round(g, l.x, l.z, 2 * l.radius + kDamClear);
  return g;
}

// One dam tried: its middle, its fall, its crest's two reaches along the contour, its height.
struct Try {
  double x, z, dx, dz;  // the middle and the fall (a unit vector)
  double left, right;   // m of crest each way along (-dz, dx) and (dz, -dx)
  double crest, height;
};

// The flood behind a dam: the samples under the water, or none where it leaks, runs off the
// map, reaches what must stay dry or outgrows the most a reservoir holds.
bool flood(const Grid& g, const Try& t, std::vector<uint8_t>& wet, std::vector<int>& cells) {
  const double level = t.crest - kFreeboard;
  const double ax = -t.dz, az = t.dx;  // along the crest, toward `left`
  wet.assign(g.y.size(), 0);
  cells.clear();
  const auto start_i = static_cast<int>(std::lround((t.x - t.dx * 2 * kFloodCell) / kFloodCell));
  const auto start_j = static_cast<int>(std::lround((t.z - t.dz * 2 * kFloodCell) / kFloodCell));
  if (!g.inside(start_i, start_j) || g.y[g.at(start_i, start_j)] >= level) return false;
  const size_t most = static_cast<size_t>(kMostReservoir / (kFloodCell * kFloodCell));
  std::vector<std::pair<int, int>> todo{{start_i, start_j}};
  wet[g.at(start_i, start_j)] = 1;
  while (!todo.empty()) {
    const auto [i, j] = todo.back();
    todo.pop_back();
    if (g.kept[g.at(i, j)]) return false;
    const double px = g.x(i) - t.x, pz = g.x(j) - t.z;
    const double down = px * t.dx + pz * t.dz, along = px * ax + pz * az;
    if (down > -0.5 * kCrestWidth) {
      // At or past the dam's line: held where the dam stands, a leak round its ends.
      if (along <= t.left + kDamTie && along >= -t.right - kDamTie) continue;
      return false;
    }
    cells.push_back(j * g.n + i);
    if (cells.size() > most) return false;
    constexpr int kDi[4] = {1, -1, 0, 0}, kDj[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; ++k) {
      const int ni = i + kDi[k], nj = j + kDj[k];
      if (!g.inside(ni, nj)) return false;  // off the map
      const size_t at = g.at(ni, nj);
      if (wet[at] || g.y[at] >= level) continue;
      wet[at] = 1;
      todo.push_back({ni, nj});
    }
  }
  return true;
}

// The samples' outline: every edge between a wet sample's square and a dry one's, chained into
// rings; the longest ring, through the squares' corners.
std::vector<mg::PlanPoint> outline(const Grid& g, const std::vector<int>& cells) {
  std::vector<uint8_t> in(g.y.size(), 0);
  for (const int c : cells) in[static_cast<size_t>(c)] = 1;
  const int m = g.n + 1;  // corners a side; corner (i, j) at the square's low x and low z
  std::multimap<int, int> next;
  const auto wet = [&](int i, int j) { return g.inside(i, j) && in[g.at(i, j)]; };
  for (const int c : cells) {
    const int i = c % g.n, j = c / g.n;
    const int c00 = j * m + i, c10 = j * m + i + 1, c01 = (j + 1) * m + i,
              c11 = (j + 1) * m + i + 1;
    // Round each wet square the same way, the dry side outward.
    if (!wet(i, j - 1)) next.insert({c00, c10});
    if (!wet(i + 1, j)) next.insert({c10, c11});
    if (!wet(i, j + 1)) next.insert({c11, c01});
    if (!wet(i - 1, j)) next.insert({c01, c00});
  }
  std::vector<int> best;
  while (!next.empty()) {
    std::vector<int> ring{next.begin()->first};
    int at = next.begin()->second;
    next.erase(next.begin());
    while (at != ring.front()) {
      ring.push_back(at);
      const auto it = next.find(at);
      if (it == next.end()) break;
      at = it->second;
      next.erase(it);
    }
    if (ring.size() > best.size()) best = std::move(ring);
  }
  std::vector<mg::PlanPoint> out;
  out.reserve(best.size());
  for (const int c : best) {
    out.push_back({(c % m - 0.5) * kFloodCell, (c / m - 0.5) * kFloodCell});
  }
  return out;
}

// A line thinned to the bends that keep it within `tol` m (Douglas–Peucker).
void thin(const std::vector<mg::PlanPoint>& p, size_t a, size_t b, double tol,
          std::vector<mg::PlanPoint>& out) {
  const double dx = p[b].x - p[a].x, dz = p[b].z - p[a].z;
  const double len = std::max(1e-9, core::hypot(dx, dz));
  double worst = 0;
  size_t at = a;
  for (size_t k = a + 1; k < b; ++k) {
    const double d = std::abs((p[k].x - p[a].x) * dz - (p[k].z - p[a].z) * dx) / len;
    if (d > worst) {
      worst = d;
      at = k;
    }
  }
  if (worst <= tol) {
    out.push_back(p[a]);
    return;
  }
  thin(p, a, at, tol, out);
  thin(p, at, b, tol, out);
}

std::vector<mg::PlanPoint> thin_ring(const std::vector<mg::PlanPoint>& ring) {
  if (ring.size() < 8) return ring;
  std::vector<mg::PlanPoint> closed = ring;
  closed.push_back(ring.front());
  const size_t half = ring.size() / 2;
  std::vector<mg::PlanPoint> out;
  thin(closed, 0, half, kShoreThin, out);
  thin(closed, half, closed.size() - 1, kShoreThin, out);
  return out;
}

// Whether (x, z) lies inside the shore or within `pad` m of it.
bool near_shore(const std::vector<mg::PlanPoint>& s, double x, double z, double pad) {
  if (inside_shore(s, x, z)) return true;
  for (size_t i = 0, j = s.size() - 1; i < s.size(); j = i++) {
    const double ex = s[i].x - s[j].x, ez = s[i].z - s[j].z;
    const double l2 = std::max(1e-12, ex * ex + ez * ez);
    const double u = std::clamp(((x - s[j].x) * ex + (z - s[j].z) * ez) / l2, 0.0, 1.0);
    const double dx = x - s[j].x - u * ex, dz = z - s[j].z - u * ez;
    if (dx * dx + dz * dz < pad * pad) return true;
  }
  return false;
}

// The crest's reach from the middle along (ux, uz) until the ground rises to `crest`, m, or none.
std::optional<double> reach_along(const mg::Level& level, const Try& t, double ux, double uz,
                                  double crest) {
  for (double s = kStep; s <= kDamLongest; s += kStep) {
    const double x = t.x + ux * s, z = t.z + uz * s;
    if (x < 0 || z < 0 || x > level.size || z > level.size) return std::nullopt;
    if (ground(level, x, z) >= crest) return s;
  }
  return std::nullopt;
}

}  // namespace

std::optional<Reservoir> find_reservoir(const mg::Level& level) {
  const Grid g = grid_of(level);
  const double low = level.mountain.base.y, high = level.mountain.summit.y;
  const double y_high = low + kDamHigh * (high - low);
  const int steps = static_cast<int>(level.size / kDamGrid);
  std::vector<uint8_t> wet;
  std::vector<int> cells, best_cells;
  double best = 0;
  Try best_try{};
  for (int row = 1; row < steps; ++row) {
    for (int col = 1; col < steps; ++col) {
      Try t{col * kDamGrid, row * kDamGrid, 0, 0, 0, 0, 0, 0};
      const int gi = static_cast<int>(std::lround(t.x / kFloodCell));
      const int gj = static_cast<int>(std::lround(t.z / kFloodCell));
      if (!g.inside(gi, gj) || g.kept[g.at(gi, gj)]) continue;
      const double y0 = ground(level, t.x, t.z);
      if (y0 < low || y0 > y_high) continue;
      // The fall: down the ground's slope over 24 m each way.
      constexpr double e = 24;
      const double gx = ground(level, t.x + e, t.z) - ground(level, t.x - e, t.z);
      const double gz = ground(level, t.x, t.z + e) - ground(level, t.x, t.z - e);
      const double len = core::hypot(gx, gz);
      if (len < 0.04 * 2 * e) continue;  // flatter than 1 in 25: no valley's fall to close
      t.dx = -gx / len;
      t.dz = -gz / len;
      for (const double h : kDamHeights) {
        const double crest = y0 + h;
        const std::optional<double> left = reach_along(level, t, -t.dz, t.dx, crest);
        if (!left) continue;
        const std::optional<double> right = reach_along(level, t, t.dz, -t.dx, crest);
        if (!right || *left + *right > kDamLongest) continue;
        t.left = *left;
        t.right = *right;
        t.crest = crest;
        t.height = h;
        if (!flood(g, t, wet, cells)) continue;
        const double area = static_cast<double>(cells.size()) * kFloodCell * kFloodCell;
        if (area < kLeastReservoir) continue;
        if (area <= best) continue;
        best = area;
        best_try = t;
        best_cells = cells;
      }
    }
  }
  if (best <= 0) return std::nullopt;
  Reservoir r;
  const Try& t = best_try;
  r.lake.shore = thin_ring(outline(g, best_cells));
  double a = 0, cx = 0, cz = 0;
  const std::vector<mg::PlanPoint>& s = r.lake.shore;
  for (size_t i = 0, j = s.size() - 1; i < s.size(); j = i++) {
    const double c = s[j].x * s[i].z - s[i].x * s[j].z;
    a += c;
    cx += (s[j].x + s[i].x) * c;
    cz += (s[j].z + s[i].z) * c;
  }
  r.lake.x = cx / (3 * a);
  r.lake.z = cz / (3 * a);
  r.lake.radius = std::sqrt(std::abs(a / 2) / kPi);
  r.lake.level = t.crest - kFreeboard;
  double floor = r.lake.level;
  for (const int c : best_cells) floor = std::min(floor, g.y[static_cast<size_t>(c)]);
  r.lake.depth = r.lake.level - floor;
  r.dam.a = {t.x - t.dz * t.left, t.z + t.dx * t.left};
  r.dam.b = {t.x + t.dz * t.right, t.z - t.dx * t.right};
  r.dam.downhill = {t.dx, t.dz};
  r.dam.crest = t.crest;
  r.dam.height = t.height;
  return r;
}

void lay_reservoir(mg::Level& level, Reservoir r) {
  mg::MapLake& lake = r.lake;
  mg::MapDam& dam = r.dam;
  core::Heightfield& g = level.ground;
  // THE DAM: the bank's crest along a-b, run on into the sides, its faces falling to the ground.
  const double ax = dam.b.x - dam.a.x, az = dam.b.z - dam.a.z;
  const double len = std::max(1e-9, core::hypot(ax, az));
  const double ux = ax / len, uz = az / len;
  const double reach = 0.5 * kCrestWidth + dam.height / kDamFace + kDamTie;
  const double x0 = std::min(dam.a.x, dam.b.x) - reach, x1 = std::max(dam.a.x, dam.b.x) + reach;
  const double z0 = std::min(dam.a.z, dam.b.z) - reach, z1 = std::max(dam.a.z, dam.b.z) + reach;
  const int c0 = std::max(0, static_cast<int>(std::floor((x0 - g.origin_x) / g.cell)));
  const int c1 = std::min(g.cols - 1, static_cast<int>(std::ceil((x1 - g.origin_x) / g.cell)));
  const int r0 = std::max(0, static_cast<int>(std::floor((z0 - g.origin_z) / g.cell)));
  const int r1 = std::min(g.rows - 1, static_cast<int>(std::ceil((z1 - g.origin_z) / g.cell)));
  const auto on_dam = [&](double x, double z, double& top) {
    const double along = (x - dam.a.x) * ux + (z - dam.a.z) * uz;
    if (along < -kDamTie || along > len + kDamTie) return false;
    const double across = std::abs((x - dam.a.x) * uz - (z - dam.a.z) * ux);
    top = dam.crest - kDamFace * std::max(0.0, across - 0.5 * kCrestWidth);
    return true;
  };
  for (int row = r0; row <= r1; ++row) {
    for (int col = c0; col <= c1; ++col) {
      const double x = g.origin_x + col * g.cell, z = g.origin_z + row * g.cell;
      double top = 0;
      if (!on_dam(x, z, top)) continue;
      float& y =
          g.data[static_cast<size_t>(row) * static_cast<size_t>(g.cols) + static_cast<size_t>(col)];
      y = static_cast<float>(std::max<double>(y, top));
    }
  }
  // THE WATER: ice that bears fills the shore flat at the surface.
  lake.ice = tarn_ice(level, lake);
  lake.thaw = lake.ice > 0 ? thaw_of(level) : 0;
  if (bears(lake)) {
    lake.depth = 0;
    double sx0 = 1e30, sx1 = -1e30, sz0 = 1e30, sz1 = -1e30;
    for (const mg::PlanPoint& p : lake.shore) {
      sx0 = std::min(sx0, p.x);
      sx1 = std::max(sx1, p.x);
      sz0 = std::min(sz0, p.z);
      sz1 = std::max(sz1, p.z);
    }
    const int a0 = std::max(0, static_cast<int>(std::floor((sx0 - g.origin_x) / g.cell)));
    const int a1 = std::min(g.cols - 1, static_cast<int>(std::ceil((sx1 - g.origin_x) / g.cell)));
    const int b0 = std::max(0, static_cast<int>(std::floor((sz0 - g.origin_z) / g.cell)));
    const int b1 = std::min(g.rows - 1, static_cast<int>(std::ceil((sz1 - g.origin_z) / g.cell)));
    for (int row = b0; row <= b1; ++row) {
      for (int col = a0; col <= a1; ++col) {
        const double x = g.origin_x + col * g.cell, z = g.origin_z + row * g.cell;
        if (!inside_shore(lake.shore, x, z)) continue;
        float& y = g.data[static_cast<size_t>(row) * static_cast<size_t>(g.cols) +
                          static_cast<size_t>(col)];
        y = static_cast<float>(std::max<double>(y, lake.level));
      }
    }
  }
  std::erase_if(level.trees, [&](const mg::TreeDef& tree) {
    double top = 0;
    if (on_dam(tree.x, tree.z, top) && top > ground(level, tree.x, tree.z) - 0.5) return true;
    return near_shore(lake.shore, tree.x, tree.z, 4);
  });
  for (mg::TreeDef& tree : level.trees) {
    if (tree.x >= x0 && tree.x <= x1 && tree.z >= z0 && tree.z <= z1)
      tree.y = ground(level, tree.x, tree.z);
  }
  dam.lake = static_cast<int>(level.lakes.size());
  level.lakes.push_back(std::move(lake));
  level.dams.push_back(dam);
}

}  // namespace ge::mountain::tarn
