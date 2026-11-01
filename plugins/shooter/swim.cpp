// The way out of the water (swim.hpp): the wet cells, the exits, the breadth-first field.
//
// ge:summary The swim field's build and its query: wet cells, exit seeds, BFS, the next waypoint
// ge:keywords swim water pool escape flow-field build_swim swim_next bfs
// ge:skill shooter bots
// ge:test tests/shooter_swim_test.cpp
#include "shooter/swim.hpp"

#include <cmath>

namespace ge::shooter {

namespace {

bool wet_at(const MoveWorld& world, Vec3f p) {
  return box_free(world, p) && water_contents(world, p) <= kContentsWater;
}

}  // namespace

void build_swim(const MoveWorld& world, const NavGraph& g, std::span<const uint8_t> exits,
                const SwimSettings& set, SwimField& f, const SwimField* same_water) {
  f = SwimField{};
  f.cell = set.cell;
  f.lo = g.lo;
  f.nx = static_cast<int32_t>(std::ceil((g.hi.x - g.lo.x) / f.cell)) + 1;
  f.ny = static_cast<int32_t>(std::ceil((g.hi.y - g.lo.y) / f.cell)) + 1;
  f.nz = static_cast<int32_t>(std::ceil((g.hi.z - g.lo.z) / f.cell)) + 1;
  if (f.nx <= 0 || f.ny <= 0 || f.nz <= 0) return;
  const size_t n =
      static_cast<size_t>(f.nx) * static_cast<size_t>(f.ny) * static_cast<size_t>(f.nz);
  f.next.assign(n, -1);
  f.exit_node.assign(n, -1);
  f.steps.assign(n, 0);
  const auto index = [&](int32_t i, int32_t j, int32_t k) {
    return (static_cast<size_t>(k) * static_cast<size_t>(f.ny) + static_cast<size_t>(j)) *
               static_cast<size_t>(f.nx) +
           static_cast<size_t>(i);
  };
  const auto centre = [&](int32_t i, int32_t j, int32_t k) {
    return Vec3f{f.lo.x + (static_cast<float>(i) + 0.5f) * f.cell,
                 f.lo.y + (static_cast<float>(j) + 0.5f) * f.cell,
                 f.lo.z + (static_cast<float>(k) + 0.5f) * f.cell};
  };
  std::vector<uint8_t>& wet = f.wet_cells;
  if (same_water && same_water->wet_cells.size() == n && same_water->cell == f.cell) {
    wet = same_water->wet_cells;
    f.wet = same_water->wet;
  } else {
    wet.assign(n, 0);
    for (int32_t k = 0; k < f.nz; k++)
      for (int32_t j = 0; j < f.ny; j++)
        for (int32_t i = 0; i < f.nx; i++)
          if (wet_at(world, centre(i, j, k))) {
            wet[index(i, j, k)] = 1;
            f.wet++;
          }
  }

  // The seeds: wet cells near an exit node, each keeping its nearest exit.
  std::vector<int32_t> queue;
  queue.reserve(static_cast<size_t>(f.wet));
  std::vector<float> seed_d(n, 0);
  const int32_t span = static_cast<int32_t>(std::ceil(set.exit_reach / f.cell)) + 1;
  for (size_t a = 0; a < g.nodes.size() && a < exits.size(); a++) {
    if (!exits[a]) continue;
    const Vec3f o = g.nodes[a].origin;
    const int32_t ci = static_cast<int32_t>((o.x - f.lo.x) / f.cell);
    const int32_t cj = static_cast<int32_t>((o.y - f.lo.y) / f.cell);
    const int32_t ck = static_cast<int32_t>((o.z - f.lo.z) / f.cell);
    for (int32_t k = ck - span; k <= ck + span; k++)
      for (int32_t j = cj - span; j <= cj + span; j++)
        for (int32_t i = ci - span; i <= ci + span; i++) {
          if (i < 0 || j < 0 || k < 0 || i >= f.nx || j >= f.ny || k >= f.nz) continue;
          const size_t c = index(i, j, k);
          if (!wet[c]) continue;
          const Vec3f d = centre(i, j, k) - o;
          // A way out goes up, and no higher than a swimmer climbs out of the water onto a ledge.
          if (d.z > 0 || -d.z > set.climb) continue;
          const float dd = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
          if (dd > set.exit_reach) continue;
          if (f.exit_node[c] < 0) {
            queue.push_back(static_cast<int32_t>(c));
          } else if (dd >= seed_d[c]) {
            continue;
          }
          f.exit_node[c] = static_cast<int32_t>(a);
          f.next[c] = -2;
          seed_d[c] = dd;
        }
  }

  // The field: breadth first from the seeds, each cell pointing back the way it was reached.
  static const int32_t kStep[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                      {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
  for (size_t q = 0; q < queue.size(); q++) {
    const size_t c = static_cast<size_t>(queue[q]);
    const int32_t i = static_cast<int32_t>(c % static_cast<size_t>(f.nx));
    const int32_t j =
        static_cast<int32_t>((c / static_cast<size_t>(f.nx)) % static_cast<size_t>(f.ny));
    const int32_t k =
        static_cast<int32_t>(c / (static_cast<size_t>(f.nx) * static_cast<size_t>(f.ny)));
    for (const auto& s : kStep) {
      const int32_t a = i + s[0], b = j + s[1], e = k + s[2];
      if (a < 0 || b < 0 || e < 0 || a >= f.nx || b >= f.ny || e >= f.nz) continue;
      const size_t d = index(a, b, e);
      if (!wet[d] || f.exit_node[d] >= 0) continue;
      const Vec3f p = centre(i, j, k), r = centre(a, b, e);
      if (!box_free(world, {(p.x + r.x) * 0.5f, (p.y + r.y) * 0.5f, (p.z + r.z) * 0.5f})) continue;
      f.next[d] = static_cast<int32_t>(c);
      f.exit_node[d] = f.exit_node[c];
      f.steps[d] = static_cast<uint16_t>(f.steps[c] + 1);
      queue.push_back(static_cast<int32_t>(d));
    }
  }
  f.leading = static_cast<int64_t>(queue.size());
}

bool swim_wet(const SwimField& f, Vec3f p) {
  if (f.next.empty()) return false;
  const int32_t i = static_cast<int32_t>(std::floor((p.x - f.lo.x) / f.cell));
  const int32_t j = static_cast<int32_t>(std::floor((p.y - f.lo.y) / f.cell));
  const int32_t k = static_cast<int32_t>(std::floor((p.z - f.lo.z) / f.cell));
  if (i < 0 || j < 0 || k < 0 || i >= f.nx || j >= f.ny || k >= f.nz) return false;
  const size_t c = (static_cast<size_t>(k) * static_cast<size_t>(f.ny) + static_cast<size_t>(j)) *
                       static_cast<size_t>(f.nx) +
                   static_cast<size_t>(i);
  return f.exit_node[c] >= 0;
}

namespace {

// The cell at `p` with a way out, or its neighbour nearest the exit (-1: none); `steps` the steps
// from there (one more from a neighbour), `here` whether it is the cell at `p` itself.
int64_t way_cell(const SwimField& f, Vec3f p, int32_t& steps, bool& here) {
  if (f.next.empty()) return -1;
  const int32_t ci = static_cast<int32_t>(std::floor((p.x - f.lo.x) / f.cell));
  const int32_t cj = static_cast<int32_t>(std::floor((p.y - f.lo.y) / f.cell));
  const int32_t ck = static_cast<int32_t>(std::floor((p.z - f.lo.z) / f.cell));
  // The cell it is in, or the neighbour nearest its exit (a swimmer at the surface is in a dry
  // cell with the water's top cell under it).
  int64_t best = -1;
  int32_t best_steps = 0;
  for (int32_t k = ck - 1; k <= ck + 1; k++)
    for (int32_t j = cj - 1; j <= cj + 1; j++)
      for (int32_t i = ci - 1; i <= ci + 1; i++) {
        if (i < 0 || j < 0 || k < 0 || i >= f.nx || j >= f.ny || k >= f.nz) continue;
        const size_t c =
            (static_cast<size_t>(k) * static_cast<size_t>(f.ny) + static_cast<size_t>(j)) *
                static_cast<size_t>(f.nx) +
            static_cast<size_t>(i);
        if (f.exit_node[c] < 0) continue;
        const int32_t st = f.steps[c] + ((i == ci && j == cj && k == ck) ? 0 : 1);
        if (best < 0 || st < best_steps) {
          best = static_cast<int64_t>(c);
          best_steps = st;
        }
      }
  if (best < 0) return -1;
  const size_t c = static_cast<size_t>(best);
  here = static_cast<int32_t>(c % static_cast<size_t>(f.nx)) == ci &&
         static_cast<int32_t>((c / static_cast<size_t>(f.nx)) % static_cast<size_t>(f.ny)) == cj &&
         static_cast<int32_t>(c / (static_cast<size_t>(f.nx) * static_cast<size_t>(f.ny))) == ck;
  steps = best_steps;
  return best;
}

}  // namespace

int32_t swim_steps(const SwimField& f, Vec3f p) {
  int32_t steps = 0;
  bool here = false;
  return way_cell(f, p, steps, here) < 0 ? -1 : steps;
}

bool swim_next(const SwimField& f, const NavGraph& g, Vec3f p, Vec3f& to) {
  int32_t steps = 0;
  bool here = false;
  const int64_t best = way_cell(f, p, steps, here);
  if (best < 0) return false;
  const size_t c = static_cast<size_t>(best);
  const size_t go = here && f.next[c] >= 0 ? static_cast<size_t>(f.next[c]) : c;
  if (here && f.next[c] == -2) {
    to = g.nodes[static_cast<size_t>(f.exit_node[c])].origin;
    return true;
  }
  const size_t nx = static_cast<size_t>(f.nx), ny = static_cast<size_t>(f.ny);
  const size_t gi = go % nx, gj = (go / nx) % ny, gk = go / (nx * ny);
  to = {f.lo.x + (static_cast<float>(gi) + 0.5f) * f.cell,
        f.lo.y + (static_cast<float>(gj) + 0.5f) * f.cell,
        f.lo.z + (static_cast<float>(gk) + 0.5f) * f.cell};
  return true;
}

}  // namespace ge::shooter
