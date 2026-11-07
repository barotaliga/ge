// The way out of the water (swim.hpp): the wet cells, the exits, the breadth-first field.
//
// ge:summary The swim field's build and its query: wet cells, exit seeds, BFS, the next waypoint
// ge:keywords swim water pool escape flow-field build_swim swim_next bfs
// ge:skill shooter bots
// ge:test tests/shooter_swim_test.cpp
#include "shooter/swim.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/pmove.hpp"

namespace ge::shooter {

namespace {

bool wet_at(const MoveWorld& world, Vec3f p) {
  return box_free(world, p) && water_contents(world, p) <= kContentsWater;
}

// A climb out from `from` onto the node at `to`, run on the move: its time when there within
// `seconds`, else -1.
float out_time(const MoveWorld& world, Vec3f from, Vec3f to, float seconds) {
  MoveState s;
  s.origin = from;
  const MoveSettings set;
  const int steps = static_cast<int>(seconds * 1000 / 13);
  for (int i = 0; i < steps; i++) {
    run_command(s, swim_out(s, to, 13), set, world);
    const Vec3f d = s.origin - to;
    if (d.x * d.x + d.y * d.y < 24 * 24 && std::fabs(d.z) < 24)
      return static_cast<float>(i + 1) * 0.013f;
  }
  return -1;
}

// A walk off a dry node at `from` toward the water at `to` (below it), run on the move: its time
// when it is swimming (waist deep or more) within `near` across of `to` within `seconds`, else -1.
float fall_in_time(const MoveWorld& world, Vec3f from, Vec3f to, float near, float seconds) {
  MoveState s;
  s.origin = from;
  s.ground = 0;  // standing on the world
  const MoveSettings set;
  const int steps = static_cast<int>(seconds * 1000 / 13);
  const double rad =
      core::libm::atan2(static_cast<double>(to.y - from.y), static_cast<double>(to.x - from.x));
  MoveCmd c;
  c.msec = 13;
  c.angles = {0, static_cast<float>(rad * (180.0 / core::kPi)), 0};
  for (int i = 0; i < steps; i++) {
    const Vec3f d = s.origin - to;
    c.forward = d.x * d.x + d.y * d.y > 12 * 12 ? 320 : 0;  // over the water: let it fall
    run_command(s, c, set, world);
    const Vec3f e = s.origin - to;
    if (s.waterlevel >= 2 && e.x * e.x + e.y * e.y < near * near)
      return static_cast<float>(i + 1) * 0.013f;
  }
  return -1;
}

bool proven_out(const MoveWorld& world, Vec3f from, Vec3f to, float seconds) {
  return out_time(world, from, to, seconds) >= 0;
}

}  // namespace

MoveCmd swim_out(const MoveState& s, Vec3f exit, int msec) {
  MoveCmd c;
  c.msec = static_cast<uint8_t>(msec);
  const float dx = exit.x - s.origin.x, dy = exit.y - s.origin.y;
  const double rad = core::libm::atan2(static_cast<double>(dy), static_cast<double>(dx));
  c.angles = {s.waterlevel >= 3   ? -30.f
              : s.waterlevel <= 1 ? 25.f
                                  : 0.f,
              static_cast<float>(rad * (180.0 / core::kPi)), 0};
  // In the water: head under, up; chest deep, level (the move's water jump takes him over the
  // edge); feet alone wet, down a little to chest deep again. Out of it: at the node.
  if (s.waterlevel == 0) c.angles.x = 0;
  c.forward = 320;
  if (s.waterlevel >= 3) c.buttons = kButtonJump;
  return c;
}

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
          if (f.exit_node[c] >= 0 && dd >= seed_d[c]) continue;
          if (set.prove_s > 0 && !proven_out(world, centre(i, j, k), o, set.prove_s)) continue;
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

SwimNavCounts nav_add_swim(const MoveWorld& world, NavGraph& g, std::span<const uint8_t> wet_floor,
                           const SwimNavSettings& set) {
  SwimNavCounts out;
  const float cell = set.cell;
  const int32_t nx = static_cast<int32_t>(std::ceil((g.hi.x - g.lo.x) / cell)) + 1;
  const int32_t ny = static_cast<int32_t>(std::ceil((g.hi.y - g.lo.y) / cell)) + 1;
  const int32_t nz = static_cast<int32_t>(std::ceil((g.hi.z - g.lo.z) / cell)) + 1;
  if (nx <= 0 || ny <= 0 || nz <= 0 || g.swim_first >= 0) return out;
  const size_t n = static_cast<size_t>(nx) * static_cast<size_t>(ny) * static_cast<size_t>(nz);
  const auto index = [&](int32_t i, int32_t j, int32_t k) {
    return (static_cast<size_t>(k) * static_cast<size_t>(ny) + static_cast<size_t>(j)) *
               static_cast<size_t>(nx) +
           static_cast<size_t>(i);
  };
  const auto centre = [&](int32_t i, int32_t j, int32_t k) {
    return Vec3f{g.lo.x + (static_cast<float>(i) + 0.5f) * cell,
                 g.lo.y + (static_cast<float>(j) + 0.5f) * cell,
                 g.lo.z + (static_cast<float>(k) + 0.5f) * cell};
  };
  const auto cell_of = [&](Vec3f p) -> int64_t {
    const auto i = static_cast<int32_t>(std::floor((p.x - g.lo.x) / cell));
    const auto j = static_cast<int32_t>(std::floor((p.y - g.lo.y) / cell));
    const auto k = static_cast<int32_t>(std::floor((p.z - g.lo.z) / cell));
    if (i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz) return -1;
    return static_cast<int64_t>(index(i, j, k));
  };
  const size_t floors = g.nodes.size();
  for (size_t a = 0; a < floors && a < wet_floor.size(); a++) g.nodes[a].wet = wet_floor[a] != 0;
  g.swim_lo = g.lo;
  g.swim_cell = cell;
  g.swim_nx = nx;
  g.swim_ny = ny;
  g.swim_nz = nz;
  g.swim_node.assign(n, -1);
  g.swim_first = static_cast<int32_t>(floors);
  // The swim nodes, in cell order.
  for (int32_t k = 0; k < nz; k++)
    for (int32_t j = 0; j < ny; j++)
      for (int32_t i = 0; i < nx; i++) {
        const Vec3f c = centre(i, j, k);
        if (!wet_at(world, c)) continue;
        NavNode node;
        node.origin = c;
        node.col = node.row = -1;
        node.swim = node.wet = true;
        g.swim_node[index(i, j, k)] = static_cast<int32_t>(g.nodes.size());
        g.nodes.push_back(node);
        out.cells++;
      }
  std::vector<NavExtraLink> extra;
  // Cell to cell, the box free half-way.
  static const int32_t kStep[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
  for (int32_t k = 0; k < nz; k++)
    for (int32_t j = 0; j < ny; j++)
      for (int32_t i = 0; i < nx; i++) {
        const int32_t a = g.swim_node[index(i, j, k)];
        if (a < 0) continue;
        for (const auto& st : kStep) {
          const int32_t ii = i + st[0], jj = j + st[1], kk = k + st[2];
          if (ii >= nx || jj >= ny || kk >= nz) continue;
          const int32_t b = g.swim_node[index(ii, jj, kk)];
          if (b < 0) continue;
          const Vec3f p = centre(i, j, k), r = centre(ii, jj, kk);
          if (!box_free(world, {(p.x + r.x) * 0.5f, (p.y + r.y) * 0.5f, (p.z + r.z) * 0.5f}))
            continue;
          extra.push_back({a, b, NavKind::swim, set.cell_s});
          extra.push_back({b, a, NavKind::swim, set.cell_s});
          out.cell_links += 2;
        }
      }
  // A pool's floor and the water it stands in (the cell it is in, else the one above it).
  for (size_t a = 0; a < floors && a < wet_floor.size(); a++) {
    if (!wet_floor[a]) continue;
    const Vec3f o = g.nodes[a].origin;
    int64_t c = cell_of(o);
    if (c < 0 || g.swim_node[static_cast<size_t>(c)] < 0) c = cell_of({o.x, o.y, o.z + cell});
    if (c < 0 || g.swim_node[static_cast<size_t>(c)] < 0) continue;
    const int32_t w = g.swim_node[static_cast<size_t>(c)];
    extra.push_back({static_cast<int32_t>(a), w, NavKind::swim, set.floor_s});
    extra.push_back({w, static_cast<int32_t>(a), NavKind::swim, set.floor_s});
    out.floor_links += 2;
  }
  // A dry node over water further down than a climb out: off its edge and down into it (the
  // water's top cell within `fall_across`, the fall proven by the move). One a node, the nearest.
  const auto across_span = static_cast<int32_t>(std::ceil(set.fall_across / cell)) + 1;
  const auto fall_ins = [&](size_t a, Vec3f o) {
    const auto ci = static_cast<int32_t>(std::floor((o.x - g.lo.x) / cell));
    const auto cj = static_cast<int32_t>(std::floor((o.y - g.lo.y) / cell));
    const auto ck = static_cast<int32_t>(std::floor((o.z - g.lo.z) / cell));
    int32_t best = -1;
    float best_d = 0;
    Vec3f best_at;
    for (int32_t j = cj - across_span; j <= cj + across_span; j++)
      for (int32_t i = ci - across_span; i <= ci + across_span; i++) {
        if (i < 0 || j < 0 || i >= nx || j >= ny) continue;
        // The top wet cell of this column below the node.
        for (int32_t k = std::min(ck, nz - 1); k >= 0; k--) {
          const int32_t w = g.swim_node[index(i, j, k)];
          if (w < 0) continue;
          const Vec3f c = centre(i, j, k);
          const float dx = c.x - o.x, dy = c.y - o.y;
          const float d = std::sqrt(dx * dx + dy * dy);
          if (d <= set.fall_across && o.z - c.z > set.climb && (best < 0 || d < best_d)) {
            best = w;
            best_d = d;
            best_at = c;
          }
          break;
        }
      }
    if (best < 0) return;
    out.proofs++;
    const float s = fall_in_time(world, o, best_at, cell * 1.5f, set.prove_s);
    if (s < 0) return;
    extra.push_back({static_cast<int32_t>(a), best, NavKind::swim, s});
    out.falls++;
  };
  // A dry edge and the water under it: out by the proven climb, in off the edge.
  const auto span = static_cast<int32_t>(std::ceil(set.exit_reach / cell)) + 1;
  struct Near {
    float d;
    int32_t node;
    Vec3f at;
  };
  std::vector<Near> near;
  for (size_t a = 0; a < floors; a++) {
    if (a < wet_floor.size() && wet_floor[a]) continue;
    const Vec3f o = g.nodes[a].origin;
    const auto ci = static_cast<int32_t>(std::floor((o.x - g.lo.x) / cell));
    const auto cj = static_cast<int32_t>(std::floor((o.y - g.lo.y) / cell));
    const auto ck = static_cast<int32_t>(std::floor((o.z - g.lo.z) / cell));
    near.clear();
    for (int32_t k = ck - span; k <= ck; k++)
      for (int32_t j = cj - span; j <= cj + span; j++)
        for (int32_t i = ci - span; i <= ci + span; i++) {
          if (i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz) continue;
          const int32_t w = g.swim_node[index(i, j, k)];
          if (w < 0) continue;
          const Vec3f d = centre(i, j, k) - o;
          if (d.z > 0 || -d.z > set.climb) continue;
          const float dd = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
          if (dd <= set.exit_reach) near.push_back({dd, w, centre(i, j, k)});
        }
    if (near.empty()) {
      fall_ins(a, o);
      continue;
    }
    // The nearest first (the cell order breaks a tie: the same graph every build).
    for (size_t x = 1; x < near.size(); x++)
      for (size_t y = x; y > 0 && (near[y].d < near[y - 1].d ||
                                   (near[y].d == near[y - 1].d && near[y].node < near[y - 1].node));
           y--)
        std::swap(near[y], near[y - 1]);
    for (size_t x = 0; x < near.size() && static_cast<int>(x) < set.exit_tries; x++) {
      out.proofs++;
      const float s = out_time(world, near[x].at, o, set.prove_s);
      if (s < 0) continue;
      extra.push_back({near[x].node, static_cast<int32_t>(a), NavKind::swim, s});
      extra.push_back({static_cast<int32_t>(a), near[x].node, NavKind::swim, set.in_s});
      out.exits++;
      break;
    }
  }
  nav_add_links(g, extra);
  g.counts.by_kind[static_cast<int>(NavKind::swim)] += static_cast<int64_t>(extra.size());
  return out;
}

bool swim_exiting(const SwimField& f, Vec3f p, int32_t& exit_node) {
  exit_node = -1;
  if (f.next.empty()) return false;
  const int32_t i = static_cast<int32_t>(std::floor((p.x - f.lo.x) / f.cell));
  const int32_t j = static_cast<int32_t>(std::floor((p.y - f.lo.y) / f.cell));
  const int32_t k = static_cast<int32_t>(std::floor((p.z - f.lo.z) / f.cell));
  if (i < 0 || j < 0 || k < 0 || i >= f.nx || j >= f.ny || k >= f.nz) return false;
  const size_t c = (static_cast<size_t>(k) * static_cast<size_t>(f.ny) + static_cast<size_t>(j)) *
                       static_cast<size_t>(f.nx) +
                   static_cast<size_t>(i);
  if (f.next[c] != -2) return false;
  exit_node = f.exit_node[c];
  return true;
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
