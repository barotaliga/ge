// The navigation graph's build: the floor columns, the candidates, the proofs through player_move,
// the movers' tags; the nearest node, the route over ai::Search, the reach and the hash.
//
// ge:summary The nav graph's build, proofs, route, nearest node, reach and hash
// ge:keywords nav build_nav prove_link nav_route nav_nearest nav_reach nav_hash floor-column
// ge:keywords gap candidate edge detour fnv
// ge:skill shooter bots
// ge:test tests/shooter_nav_test.cpp
#include "shooter/nav.hpp"

#include <cstring>
#include <iterator>

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::shooter {

namespace {

constexpr float kStepHeight = 18;  // the move climbs a step this high without a jump
constexpr float kArriveUp = 2;     // a proof arrives within this up or down, units
constexpr float kStall_s = 0.25f;  // a proof getting no nearer for this long is refused
constexpr float kNearestUp = 64;   // nav_nearest's reach up and down, units
constexpr float kGapUp = 24;   // a floor this near the straight line between two nodes bridges it
constexpr float kJumpUp = 64;  // the most a jump link is tried upward, units (the apex is 45.6)
constexpr float kUpAcross = 2.5f;  // a jump up is tried only this many columns across
constexpr float kJumpShares[3] = {1, 0.5f, 0.25f};  // a jump's speeds, of maxspeed
constexpr double kDetour = 1.5;  // a walk this many times a straight run's time needs no jump
constexpr double kDetourSlack_s = 0.25;
constexpr float kFloorNormal = 0.7f;
constexpr float kScanStep = 8;  // the column's search for free space below a floor, units

float across(Vec3f a, Vec3f b) {
  const float dx = b.x - a.x, dy = b.y - a.y;
  return root_of(dx * dx + dy * dy);
}

// The view's yaw at `to` from `from`, degrees.
float yaw_at(Vec3f from, Vec3f to) {
  const double r =
      core::libm::atan2(static_cast<double>(to.y - from.y), static_cast<double>(to.x - from.x));
  return static_cast<float>(r * (180.0 / core::kPi));
}

bool boxes_meet(Vec3f alo, Vec3f ahi, Vec3f blo, Vec3f bhi) {
  return alo.x <= bhi.x && alo.y <= bhi.y && alo.z <= bhi.z && blo.x <= ahi.x && blo.y <= ahi.y &&
         blo.z <= ahi.z;
}

int32_t mover_at(std::span<const NavMover> movers, Vec3f lo, Vec3f hi) {
  for (const NavMover& m : movers)
    if (boxes_meet(lo, hi, m.absmin, m.absmax)) return m.ent;
  return -1;
}

// The mover holding a node standing at `p`: a ridden one only under the node's own origin (a
// player beside a plat is not on it), any other wherever its box meets the player's.
int32_t mover_holding(std::span<const NavMover> movers, Vec3f p) {
  for (const NavMover& m : movers) {
    if (m.model != nullptr) {
      if (p.x > m.absmin.x && p.x < m.absmax.x && p.y > m.absmin.y && p.y < m.absmax.y &&
          p.z + kPlayerMins.z <= m.absmax.z + 1 && p.z + kPlayerMaxs.z >= m.absmin.z)
        return m.ent;
    } else if (boxes_meet(p + kPlayerMins, p + kPlayerMaxs, m.absmin, m.absmax)) {
      return m.ent;
    }
  }
  return -1;
}

// Free, level floor straight back from `from`, away from `to`, for `dist` units.
bool room_to_run_up(const MoveWorld& world, Vec3f from, Vec3f to, float dist) {
  Vec3f dir{from.x - to.x, from.y - to.y, 0};
  if (normalize(dir) == 0) return false;
  const Vec3f back = mad(from, dist, dir);
  const HullTrace along = player_trace(world, from, back);
  if (along.fraction < 1 || along.startsolid) return false;
  const HullTrace down = player_trace(world, back, {back.x, back.y, back.z - 2});
  return down.fraction < 1 && !down.startsolid && down.plane.normal.z >= kFloorNormal &&
         down.endpos.z >= from.z - 1 && down.endpos.z <= from.z + 1;
}

// The floors down one grid column at (x, y), top first.
void column_floors(const MoveWorld& world, float x, float y, float top, float bottom,
                   std::vector<Vec3f>& out) {
  out.clear();
  float z = top;
  while (z > bottom) {
    const Vec3f start{x, y, z};
    if (!box_free(world, start)) {
      z -= kScanStep;
      continue;
    }
    const HullTrace tr = player_trace(world, start, {x, y, bottom});
    if (tr.startsolid || tr.fraction == 1) return;
    if (tr.plane.normal.z >= kFloorNormal) out.push_back(tr.endpos);
    // Below this floor: past the player's box under it, then scan for free space again.
    z = tr.endpos.z - (kPlayerMaxs.z - kPlayerMins.z) - 1;
  }
}

}  // namespace

const char* nav_kind_name(NavKind k) {
  switch (k) {
    case NavKind::walk:
      return "walk";
    case NavKind::drop:
      return "drop";
    case NavKind::jump:
      return "jump";
    case NavKind::ride:
      return "ride";
  }
  return "?";
}

void map_world(const BrushModel& map, MoveWorld& world) {
  world.count = 1;
  Solid& s = world.solids[0];
  s = Solid{};
  s.kind = SolidKind::brush;
  s.point_hull = map.hulls[0];
  s.clip_hull = map.hulls[1];
  s.mins = map.mins;
  s.maxs = map.maxs;
}

NavProof prove_link(const MoveWorld& world, const MoveSettings& move, Vec3f from, Vec3f to,
                    NavKind kind, float speed, const NavSettings& set, std::vector<Vec3f>* path) {
  NavProof p;
  MoveState s;
  s.origin = from;
  s.maxspeed = move.maxspeed;
  float yaw = yaw_at(from, to);
  if (kind == NavKind::jump) {
    Vec3f dir{to.x - from.x, to.y - from.y, 0};
    normalize(dir);
    s.velocity = dir * speed;
  }
  const bool jump = kind == NavKind::jump;
  const float step_s = static_cast<float>(set.msec) * 0.001f;
  const float floor_z = (from.z < to.z ? from.z : to.z) - kArriveUp;
  float best = across(from, to);
  float best_s = 0;
  p.lo = from + kPlayerMins;
  p.hi = from + kPlayerMaxs;
  if (path) path->push_back(from);
  for (int i = 0;; i++) {
    const float t = static_cast<float>(i + 1) * step_s;
    if (across(s.origin, to) > 0.01f) yaw = yaw_at(s.origin, to);
    MoveCmd c;
    c.msec = static_cast<uint8_t>(set.msec);
    c.angles = {0, yaw, 0};
    c.forward = static_cast<int16_t>(move.maxspeed);
    c.buttons = jump && i == 0 ? kButtonJump : 0;
    player_move(s, c, move, world);
    p.commands++;
    if (path) path->push_back(s.origin);
    const Vec3f blo = s.origin + kPlayerMins, bhi = s.origin + kPlayerMaxs;
    for (int a = 0; a < 3; a++) {
      if (blo[a] < p.lo[a]) p.lo[a] = blo[a];
      if (bhi[a] > p.hi[a]) p.hi[a] = bhi[a];
    }
    p.end = s.origin;
    p.time_s = t;
    const float h = across(s.origin, to);
    const float dz = s.origin.z - to.z;
    const bool grounded = s.ground >= 0;
    if (grounded && h <= set.arrive && dz <= kArriveUp && dz >= -kArriveUp) {
      p.reached = true;
      return p;
    }
    if (grounded && s.origin.z < floor_z) return p;  // fell below both ends: missed
    if (jump && grounded) return p;  // a jump that did not leave the ground, or landed short
    if (t >= set.limit_s) return p;
    // Getting no nearer counts only on the ground: a player in the air overshoots and comes back.
    if (h < best - 0.5f || !grounded) {
      if (h < best) best = h;
      best_s = t;
    } else if (t - best_s > kStall_s) {
      return p;
    }
  }
}

void build_nav(const MoveWorld& world, std::span<const NavMover> movers, const MoveSettings& move,
               const NavSettings& set, NavGraph& g) {
  g = NavGraph{};
  g.set = set;
  const Solid& w = world.solids[0];
  g.lo = w.mins;
  g.hi = w.maxs;
  g.cols = static_cast<int32_t>((g.hi.x - g.lo.x) / set.spacing);
  g.rows = static_cast<int32_t>((g.hi.y - g.lo.y) / set.spacing);
  const size_t ncol = static_cast<size_t>(g.cols) * static_cast<size_t>(g.rows);
  g.col_first.assign(ncol, 0);
  g.col_n.assign(ncol, 0);

  // The nodes, column by column.
  const float top = g.hi.z - kPlayerMaxs.z - 1;
  const float bottom = g.lo.z;
  std::vector<Vec3f> floors;
  for (int32_t i = 0; i < g.cols; i++)
    for (int32_t j = 0; j < g.rows; j++) {
      const float x = g.lo.x + set.spacing * (static_cast<float>(i) + 0.5f);
      const float y = g.lo.y + set.spacing * (static_cast<float>(j) + 0.5f);
      column_floors(world, x, y, top, bottom, floors);
      const size_t c =
          static_cast<size_t>(i) * static_cast<size_t>(g.rows) + static_cast<size_t>(j);
      g.col_first[c] = static_cast<int32_t>(g.nodes.size());
      g.col_n[c] = static_cast<int32_t>(floors.size());
      g.counts.columns++;
      for (const Vec3f& f : floors) {
        NavNode n;
        n.origin = f;
        n.col = i;
        n.row = j;
        n.mover = mover_holding(movers, f);
        g.nodes.push_back(n);
      }
    }

  // Whether a column at (x, y) holds a floor within kGapUp of height z.
  auto floor_near = [&](float x, float y, float z) {
    const int32_t i = static_cast<int32_t>((x - g.lo.x) / set.spacing);
    const int32_t j = static_cast<int32_t>((y - g.lo.y) / set.spacing);
    if (i < 0 || j < 0 || i >= g.cols || j >= g.rows) return false;
    const size_t c = static_cast<size_t>(i) * static_cast<size_t>(g.rows) + static_cast<size_t>(j);
    for (int32_t k = 0; k < g.col_n[c]; k++) {
      const float dz =
          g.nodes[static_cast<size_t>(g.col_first[c]) + static_cast<size_t>(k)].origin.z - z;
      if (dz <= kGapUp && dz >= -kGapUp) return true;
    }
    return false;
  };
  // A column between a and b with no floor near the height of either, open at the higher one: a
  // gap (a ledge stepped down from is no gap, its foot at the lower end's height; nor is a wall).
  auto gap_between = [&](Vec3f a, Vec3f b) {
    const float d = across(a, b);
    const int steps = static_cast<int>(d / (set.spacing * 0.5f));
    for (int k = 1; k < steps; k++) {
      const float f = static_cast<float>(k) / static_cast<float>(steps);
      const float x = a.x + (b.x - a.x) * f, y = a.y + (b.y - a.y) * f;
      if (!floor_near(x, y, a.z) && !floor_near(x, y, b.z) &&
          box_free(world, {x, y, a.z > b.z ? a.z : b.z}))
        return true;
    }
    return false;
  };

  // An EDGE node: a neighbouring column without a floor near its height (a ledge's lip, a pit's
  // rim, a wall's foot). Gap jumps are tried only from an edge to an edge.
  std::vector<uint8_t> edge(g.nodes.size(), 0);
  for (size_t a = 0; a < g.nodes.size(); a++) {
    const NavNode& n = g.nodes[a];
    for (int32_t di = -1; di <= 1 && !edge[a]; di++)
      for (int32_t dj = -1; dj <= 1 && !edge[a]; dj++)
        if ((di != 0 || dj != 0) &&
            !floor_near(n.origin.x + set.spacing * static_cast<float>(di),
                        n.origin.y + set.spacing * static_cast<float>(dj), n.origin.z))
          edge[a] = 1;
  }

  // One candidate's tries, in order, until one reaches; the link kept, or the refusal counted.
  struct Try {
    NavKind kind;
    float speed;
  };
  auto prove_in = [&](const MoveWorld& on, size_t a, int32_t b, const Try* tries, int n,
                      std::vector<NavLink>& out) {
    const Vec3f from = g.nodes[a].origin, to = g.nodes[static_cast<size_t>(b)].origin;
    for (int q = 0; q < n; q++) {
      const NavProof pr = prove_link(on, move, from, to, tries[q].kind, tries[q].speed, set);
      g.counts.proofs++;
      g.counts.commands += pr.commands;
      if (!pr.reached) continue;
      if (tries[q].kind == NavKind::jump && !room_to_run_up(on, from, to, set.run_up)) {
        g.counts.no_run_up++;
        continue;
      }
      NavKind kind = tries[q].kind;
      if (kind == NavKind::walk && to.z < from.z - kStepHeight) kind = NavKind::drop;
      out.push_back({b, kind, pr.time_s, mover_at(movers, pr.lo, pr.hi), tries[q].speed});
      g.counts.by_kind[static_cast<int>(kind)]++;
      return;
    }
    g.counts.refused++;
  };
  auto prove = [&](size_t a, int32_t b, const Try* tries, int n, std::vector<NavLink>& out) {
    prove_in(world, a, b, tries, n, out);
  };
  Try jumps[std::size(kJumpShares)];
  int njumps = 0;
  for (const float share : kJumpShares) jumps[njumps++] = {NavKind::jump, move.maxspeed * share};
  const Try walk{NavKind::walk, 0};

  // Every node's links, by node until they are laid out flat.
  std::vector<std::vector<NavLink>> out(g.nodes.size());
  auto each_column = [&](const NavNode& na, int32_t span, auto&& fn) {
    for (int32_t i = na.col - span; i <= na.col + span; i++) {
      if (i < 0 || i >= g.cols) continue;
      for (int32_t j = na.row - span; j <= na.row + span; j++) {
        if (j < 0 || j >= g.rows) continue;
        const size_t c =
            static_cast<size_t>(i) * static_cast<size_t>(g.rows) + static_cast<size_t>(j);
        for (int32_t k = 0; k < g.col_n[c]; k++) fn(i, j, g.col_first[c] + k);
      }
    }
  };

  // PASS 1: the neighbours, walked (or jumped onto when higher than a step).
  for (size_t a = 0; a < g.nodes.size(); a++) {
    const NavNode& na = g.nodes[a];
    each_column(na, 1, [&](int32_t, int32_t, int32_t b) {
      if (static_cast<size_t>(b) == a) return;
      const float rise = g.nodes[static_cast<size_t>(b)].origin.z - na.origin.z;
      if (rise > kJumpUp) return;
      Try tries[1 + std::size(jumps)];
      int n = 0;
      tries[n++] = walk;
      if (rise > kStepHeight)
        for (int q = 0; q < njumps; q++) tries[n++] = jumps[q];
      prove(a, b, tries, n, out[a]);
    });
  }

  // PASS 2: further jumps, only to a node the walks of pass 1 do not reach in about the time a
  // straight run would take (kDetour of it): onto something higher than a step within
  // kUpAcross columns, or across a gap from an edge to an edge.
  const int32_t span = static_cast<int32_t>(set.reach / set.spacing) + 1;
  const double limit = kDetour * set.reach / move.maxspeed + kDetourSlack_s;
  ai::Search walked(g.nodes.size());
  for (size_t a = 0; a < g.nodes.size(); a++) {
    const NavNode& na = g.nodes[a];
    walked.reset(g.nodes.size());
    walked.seed(static_cast<int32_t>(a));
    walked.run(
        [&](int32_t n) {
          for (const NavLink& l : out[static_cast<size_t>(n)]) {
            const double cost = walked.cost(n) + l.time_s;
            if (cost <= limit && walked.improves(l.to, cost)) walked.reach(l.to, cost, n);
          }
        },
        [](int32_t) { return false; });
    each_column(na, span, [&](int32_t i, int32_t j, int32_t b) {
      if (static_cast<size_t>(b) == a) return;
      if (i - na.col <= 1 && na.col - i <= 1 && j - na.row <= 1 && na.row - j <= 1) return;
      const Vec3f to = g.nodes[static_cast<size_t>(b)].origin;
      const float d = across(na.origin, to);
      const float rise = to.z - na.origin.z;
      if (d > set.reach || rise > kJumpUp) return;
      if (walked.reached(b) && walked.cost(b) <= kDetour * d / move.maxspeed + kDetourSlack_s)
        return;
      const bool up = rise > kStepHeight && d <= set.spacing * kUpAcross;
      if (!up && (!edge[a] || !edge[static_cast<size_t>(b)] || !gap_between(na.origin, to))) return;
      prove(a, b, jumps, njumps, out[a]);
    });
  }

  // RIDES: each ridden mover placed at the far end of its travel, the columns it held at rest
  // traced again for the floor on its top, a ride up to each, and the ways off it proven there.
  g.movers.assign(movers.begin(), movers.end());
  for (const NavMover& m : movers) {
    if (m.model == nullptr) continue;
    MoveWorld there = world;
    if (there.count >= kMaxSolids) continue;
    Solid& s = there.solids[there.count++];
    s = Solid{};
    s.kind = SolidKind::brush;
    s.origin = m.rest + m.travel;
    s.point_hull = m.model->hulls[0];
    s.clip_hull = m.model->hulls[1];
    s.mins = m.model->mins;
    s.maxs = m.model->maxs;
    const size_t rest = g.nodes.size();
    for (size_t a = 0; a < rest; a++) {
      const NavNode& na = g.nodes[a];
      if (na.mover != m.ent) continue;
      const float x = na.origin.x, y = na.origin.y;
      // Ridden only where the player's whole box stands on the top: a ride from a column on its
      // edge ends half over the drop.
      if (x - m.absmin.x < kPlayerMaxs.x || m.absmax.x - x < kPlayerMaxs.x ||
          y - m.absmin.y < kPlayerMaxs.y || m.absmax.y - y < kPlayerMaxs.y)
        continue;
      // The mover's top in this column: the highest floor above a step and below its travel's
      // reach plus the player's height (a ceiling's own floor above is not it).
      const float above = na.origin.z + m.travel.z + (kPlayerMaxs.z - kPlayerMins.z);
      column_floors(there, x, y, top, bottom, floors);
      const Vec3f* on = nullptr;
      for (const Vec3f& f : floors)
        if (f.z > na.origin.z + kStepHeight && f.z <= above && (on == nullptr || f.z > on->z))
          on = &f;
      if (on == nullptr) continue;
      NavNode t;
      t.origin = *on;
      t.col = na.col;
      t.row = na.row;
      t.mover = m.ent;
      t.ride = m.ent;
      const int32_t ti = static_cast<int32_t>(g.nodes.size());
      g.nodes.push_back(t);
      edge.push_back(0);
      out.emplace_back();
      out[a].push_back({ti, NavKind::ride, m.ride_s, m.ent, 0});
      g.counts.by_kind[static_cast<int>(NavKind::ride)]++;
      // Off the top: to the nodes beside it that the mover does not hold.
      const Vec3f tz = t.origin;
      each_column(t, 1, [&](int32_t, int32_t, int32_t b) {
        const NavNode& nb = g.nodes[static_cast<size_t>(b)];
        if (nb.mover == m.ent) return;
        const float rise = nb.origin.z - tz.z;
        if (rise > kJumpUp) return;
        Try tries[1 + std::size(jumps)];
        int n = 0;
        tries[n++] = walk;
        if (rise > kStepHeight)
          for (int q = 0; q < njumps; q++) tries[n++] = jumps[q];
        prove_in(there, static_cast<size_t>(ti), b, tries, n, out[static_cast<size_t>(ti)]);
      });
    }
  }

  for (size_t a = 0; a < g.nodes.size(); a++) {
    g.nodes[a].first_link = static_cast<int32_t>(g.links.size());
    g.nodes[a].links = static_cast<int32_t>(out[a].size());
    g.links.insert(g.links.end(), out[a].begin(), out[a].end());
  }
}

int32_t nav_nearest(const NavGraph& g, Vec3f p) {
  if (g.cols == 0 || g.rows == 0) return -1;
  const int32_t ci = static_cast<int32_t>((p.x - g.lo.x) / g.set.spacing);
  const int32_t cj = static_cast<int32_t>((p.y - g.lo.y) / g.set.spacing);
  int32_t best = -1;
  float best_d = 0;
  for (int32_t i = ci - 1; i <= ci + 1; i++) {
    if (i < 0 || i >= g.cols) continue;
    for (int32_t j = cj - 1; j <= cj + 1; j++) {
      if (j < 0 || j >= g.rows) continue;
      const size_t c =
          static_cast<size_t>(i) * static_cast<size_t>(g.rows) + static_cast<size_t>(j);
      for (int32_t k = 0; k < g.col_n[c]; k++) {
        const int32_t n = g.col_first[c] + k;
        const Vec3f d = g.nodes[static_cast<size_t>(n)].origin - p;
        if (d.z > kNearestUp || d.z < -kNearestUp) continue;
        const float dd = dot(d, d);
        if (best < 0 || dd < best_d) {
          best = n;
          best_d = dd;
        }
      }
    }
  }
  // A ridden mover's top nodes come after the columns' (build_nav).
  const size_t tops =
      g.col_n.empty() ? g.nodes.size() : static_cast<size_t>(g.col_first.back() + g.col_n.back());
  for (size_t n = tops; n < g.nodes.size(); n++) {
    const Vec3f d = g.nodes[n].origin - p;
    if (d.z > kNearestUp || d.z < -kNearestUp) continue;
    const float dd = dot(d, d);
    if (best < 0 || dd < best_d) {
      best = static_cast<int32_t>(n);
      best_d = dd;
    }
  }
  return best;
}

NavRoute nav_route(const NavGraph& g, int32_t from, int32_t to, ai::Search& search,
                   float mover_penalty_s, std::span<const int32_t> barred) {
  auto is_barred = [&](int32_t m) {
    if (m < 0) return false;
    for (const int32_t b : barred)
      if (b == m) return true;
    return false;
  };
  NavRoute r;
  if (from < 0 || to < 0) return r;
  // The fastest any link crosses ground, so the heuristic never overestimates.
  double fastest = 0;
  for (size_t a = 0; a < g.nodes.size(); a++) {
    const NavNode& n = g.nodes[a];
    for (int32_t k = 0; k < n.links; k++) {
      const NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
      const double v = across(n.origin, g.nodes[static_cast<size_t>(l.to)].origin) / l.time_s;
      if (v > fastest) fastest = v;
    }
  }
  const Vec3f goal = g.nodes[static_cast<size_t>(to)].origin;
  auto heuristic = [&](int32_t n) {
    return fastest > 0 ? across(g.nodes[static_cast<size_t>(n)].origin, goal) / fastest : 0.0;
  };
  search.reset(g.nodes.size());
  search.seed(from, 0, heuristic(from));
  const int32_t found = search.run(
      [&](int32_t a) {
        const NavNode& n = g.nodes[static_cast<size_t>(a)];
        for (int32_t k = 0; k < n.links; k++) {
          const NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
          if (is_barred(l.mover)) continue;
          const double cost = search.cost(a) + l.time_s + (l.mover >= 0 ? mover_penalty_s : 0);
          if (search.improves(l.to, cost)) search.reach(l.to, cost, a, heuristic(l.to));
        }
      },
      [&](int32_t n) { return n == to; }, heuristic);
  r.expanded = search.expanded();
  if (found < 0) return r;
  r.nodes = search.path_to(found);
  for (size_t i = 0; i + 1 < r.nodes.size(); i++) {
    const NavNode& n = g.nodes[static_cast<size_t>(r.nodes[i])];
    for (int32_t k = 0; k < n.links; k++) {
      const NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
      if (l.to == r.nodes[i + 1]) {
        r.time_s += l.time_s;
        break;
      }
    }
  }
  return r;
}

int32_t nav_reach(const NavGraph& g, int32_t from, ai::Search& search) {
  search.reset(g.nodes.size());
  if (from < 0) return 0;
  search.seed(from);
  search.run_all([&](int32_t a) {
    const NavNode& n = g.nodes[static_cast<size_t>(a)];
    for (int32_t k = 0; k < n.links; k++) {
      const NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
      const double cost = search.cost(a) + l.time_s;
      if (search.improves(l.to, cost)) search.reach(l.to, cost, a);
    }
  });
  int32_t count = 0;
  for (size_t a = 0; a < g.nodes.size(); a++)
    if (search.reached(static_cast<int32_t>(a))) count++;
  return count;
}

void nav_add_links(NavGraph& g, std::span<const NavExtraLink> extra) {
  if (extra.empty()) return;
  std::vector<NavLink> links;
  links.reserve(g.links.size() + extra.size());
  for (size_t a = 0; a < g.nodes.size(); a++) {
    NavNode& n = g.nodes[a];
    const int32_t first = static_cast<int32_t>(links.size());
    for (int32_t k = 0; k < n.links; k++)
      links.push_back(g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)]);
    for (const NavExtraLink& e : extra) {
      if (e.from != static_cast<int32_t>(a) || e.to < 0 ||
          static_cast<size_t>(e.to) >= g.nodes.size())
        continue;
      NavLink l;
      l.to = e.to;
      l.kind = e.kind;
      l.time_s = e.time_s;
      links.push_back(l);
    }
    n.first_link = first;
    n.links = static_cast<int32_t>(links.size()) - first;
  }
  g.links = std::move(links);
}

int64_t nav_close_entries(NavGraph& g, std::span<const uint8_t> closed,
                          std::vector<NavExtraLink>* cut_links) {
  if (closed.size() != g.nodes.size()) return 0;
  std::vector<NavLink> links;
  links.reserve(g.links.size());
  int64_t cut = 0;
  for (size_t a = 0; a < g.nodes.size(); a++) {
    NavNode& n = g.nodes[a];
    const int32_t first = static_cast<int32_t>(links.size());
    for (int32_t k = 0; k < n.links; k++) {
      const NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
      if (!closed[a] && closed[static_cast<size_t>(l.to)]) {
        cut++;
        if (cut_links) cut_links->push_back({static_cast<int32_t>(a), l.to, l.kind, l.time_s});
        continue;
      }
      links.push_back(l);
    }
    n.first_link = first;
    n.links = static_cast<int32_t>(links.size()) - first;
  }
  g.links = std::move(links);
  return cut;
}

uint64_t nav_hash(const NavGraph& g) {
  uint64_t h = 1469598103934665603ull;
  auto mix = [&](const void* p, size_t n) {
    const auto* b = static_cast<const unsigned char*>(p);
    for (size_t i = 0; i < n; i++) {
      h ^= b[i];
      h *= 1099511628211ull;
    }
  };
  for (const NavNode& n : g.nodes) {
    mix(&n.origin.x, 4);
    mix(&n.origin.y, 4);
    mix(&n.origin.z, 4);
    mix(&n.mover, 4);
    if (n.ride >= 0) mix(&n.ride, 4);
    mix(&n.links, 4);
  }
  for (const NavLink& l : g.links) {
    mix(&l.to, 4);
    const uint8_t k = static_cast<uint8_t>(l.kind);
    mix(&k, 1);
    mix(&l.time_s, 4);
    mix(&l.mover, 4);
    mix(&l.speed, 4);
  }
  return h;
}

}  // namespace ge::shooter
