// The navigation graph's build: the floor columns, the candidates, the proofs through player_move,
// the movers' tags; the nearest node, the route over ai::Search, the reach and the hash.
//
// ge:summary The nav graph's build, proofs, route, nearest node, reach and hash
// ge:keywords nav build_nav prove_link nav_route nav_nearest nav_reach nav_hash floor-column
// ge:keywords gap candidate edge detour fnv
// ge:skill shooter bots
// ge:test tests/shooter_nav_test.cpp
#include "shooter/nav.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/slide.hpp"

namespace ge::shooter {

namespace {

constexpr float kStepHeight = 18;  // the move climbs a step this high without a jump
constexpr float kArriveUp = 2;     // a proof arrives within this up or down, units
constexpr float kStall_s = 0.25f;  // a proof getting no nearer for this long is refused
constexpr float kNearestUp = 64;   // nav_nearest's reach up and down, units
constexpr float kGapUp = 24;  // a floor this near the straight line between two nodes bridges it
// A jump may land this short of its target on the target's floor (a stair's tread), units.
constexpr float kLandShort = 24;
constexpr float kStandingJump_s = 0.4f;  // a standing jump's link costs this much more, s
constexpr float kJumpUp = 64;      // the most a jump link is tried upward, units (the apex is 45.6)
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
    case NavKind::swim:
      return "swim";
    case NavKind::rocket:
      return "rocket";
    case NavKind::grenade:
      return "grenade";
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
  bool landed = false;  // a jump down on the target's floor, walking the rest
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
    if (jump && grounded && !landed) {
      // A standing jump landed on the target's own floor just short of it (a stair's tread): the
      // rest is walked, the stall rule holding the walk. Anything else did not leave the ground,
      // or fell short (a moving jump is one flight).
      if (speed > 0 || i == 0 || dz > kArriveUp || dz < -kArriveUp || h > kLandShort) return p;
      landed = true;
    }
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
      if (tries[q].kind == NavKind::jump && tries[q].speed > 0 &&
          !room_to_run_up(on, from, to, set.run_up)) {
        g.counts.no_run_up++;
        continue;
      }
      NavKind kind = tries[q].kind;
      if (kind == NavKind::walk && to.z < from.z - kStepHeight) kind = NavKind::drop;
      // A standing jump is the way of last resort: harder on a person's hands than a run's.
      const float cost = kind == NavKind::jump && tries[q].speed <= 0 ? kStandingJump_s : 0;
      out.push_back({b, kind, pr.time_s + cost, mover_at(movers, pr.lo, pr.hi), tries[q].speed});
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
      Try tries[2 + std::size(jumps)];
      int n = 0;
      tries[n++] = walk;
      if (rise > kStepHeight) {
        for (int q = 0; q < njumps; q++) tries[n++] = jumps[q];
        tries[n++] = {NavKind::jump, 0};  // a standing jump: a stair's tread has no run-up
      }
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
  if ((g.cols == 0 || g.rows == 0) && g.swim_first < 0) return -1;
  const int32_t ci = static_cast<int32_t>((p.x - g.lo.x) / g.set.spacing);
  const int32_t cj = static_cast<int32_t>((p.y - g.lo.y) / g.set.spacing);
  int32_t best = -1;
  float best_d = 0;
  for (int32_t i = ci - 1; i <= ci + 1 && g.set.spacing > 0; i++) {
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
  // A ridden mover's top nodes come after the columns' (build_nav), the swim nodes after them.
  const size_t tops =
      g.col_n.empty() ? g.nodes.size() : static_cast<size_t>(g.col_first.back() + g.col_n.back());
  const size_t tops_end = g.swim_first >= 0 ? static_cast<size_t>(g.swim_first) : g.nodes.size();
  for (size_t n = tops; n < tops_end; n++) {
    const Vec3f d = g.nodes[n].origin - p;
    if (d.z > kNearestUp || d.z < -kNearestUp) continue;
    const float dd = dot(d, d);
    if (best < 0 || dd < best_d) {
      best = static_cast<int32_t>(n);
      best_d = dd;
    }
  }
  // The swim node of the cell `p` is in, when it is the nearer.
  if (g.swim_first >= 0 && g.swim_cell > 0) {
    const auto i = static_cast<int32_t>(std::floor((p.x - g.swim_lo.x) / g.swim_cell));
    const auto j = static_cast<int32_t>(std::floor((p.y - g.swim_lo.y) / g.swim_cell));
    const auto k = static_cast<int32_t>(std::floor((p.z - g.swim_lo.z) / g.swim_cell));
    if (i >= 0 && j >= 0 && k >= 0 && i < g.swim_nx && j < g.swim_ny && k < g.swim_nz) {
      const int32_t n = g.swim_node[(static_cast<size_t>(k) * static_cast<size_t>(g.swim_ny) +
                                     static_cast<size_t>(j)) *
                                        static_cast<size_t>(g.swim_nx) +
                                    static_cast<size_t>(i)];
      if (n >= 0) {
        const Vec3f d = g.nodes[static_cast<size_t>(n)].origin - p;
        const float dd = dot(d, d);
        if (best < 0 || dd < best_d) best = n;
      }
    }
  }
  return best;
}

NavRoute nav_route(const NavGraph& g, int32_t from, int32_t to, ai::Search& search,
                   float mover_penalty_s, std::span<const int32_t> barred,
                   std::span<const NavExtraLink> cut, const NavJumper& jumper) {
  auto is_cut = [&](int32_t a, int32_t b) {
    for (const NavExtraLink& c : cut)
      if (c.from == a && c.to == b) return true;
    return false;
  };
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
          // A shut door is crossed neither by a link it is tagged on nor into a node inside it
          // (a link that crosses two doors carries one of them).
          if (is_barred(l.mover) || is_barred(g.nodes[static_cast<size_t>(l.to)].mover) ||
              (l.mover >= 0 && !cut.empty() && is_cut(a, l.to)) || !jumper.takes(l))
            continue;
          const double cost =
              search.cost(a) + jumper.cost_s(l) + (l.mover >= 0 ? mover_penalty_s : 0);
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

int32_t nav_reach(const NavGraph& g, int32_t from, ai::Search& search, const NavJumper& jumper) {
  search.reset(g.nodes.size());
  if (from < 0) return 0;
  search.seed(from);
  search.run_all([&](int32_t a) {
    const NavNode& n = g.nodes[static_cast<size_t>(a)];
    for (int32_t k = 0; k < n.links; k++) {
      const NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
      if (!jumper.takes(l)) continue;
      const double cost = search.cost(a) + jumper.cost_s(l);
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
  // The extra links by their start, each node's in the order given (a counting sort).
  const size_t nodes = g.nodes.size();
  std::vector<int32_t> start(nodes + 1, 0);
  for (const NavExtraLink& e : extra)
    if (e.from >= 0 && static_cast<size_t>(e.from) < nodes && e.to >= 0 &&
        static_cast<size_t>(e.to) < nodes)
      start[static_cast<size_t>(e.from) + 1]++;
  for (size_t a = 0; a < nodes; a++) start[a + 1] += start[a];
  std::vector<int32_t> order(static_cast<size_t>(start[nodes]));
  std::vector<int32_t> fill(start.begin(), start.end() - 1);
  for (size_t i = 0; i < extra.size(); i++) {
    const NavExtraLink& e = extra[i];
    if (e.from < 0 || static_cast<size_t>(e.from) >= nodes || e.to < 0 ||
        static_cast<size_t>(e.to) >= nodes)
      continue;
    order[static_cast<size_t>(fill[static_cast<size_t>(e.from)]++)] = static_cast<int32_t>(i);
  }
  std::vector<NavLink> links;
  links.reserve(g.links.size() + order.size());
  for (size_t a = 0; a < nodes; a++) {
    NavNode& n = g.nodes[a];
    const int32_t first = static_cast<int32_t>(links.size());
    for (int32_t k = 0; k < n.links; k++)
      links.push_back(g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)]);
    for (int32_t x = start[a]; x < start[a + 1]; x++) {
      const NavExtraLink& e = extra[static_cast<size_t>(order[static_cast<size_t>(x)])];
      NavLink l;
      l.to = e.to;
      l.kind = e.kind;
      l.time_s = e.time_s;
      l.speed = e.speed;
      l.back = e.back;
      l.pitch = e.pitch;
      l.level = e.level;
      l.lead = e.lead;
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

void flip_keys(float way, float view, float& forward, float& side) {
  const ViewAxes ax = angle_vectors({0, view, 0});
  const Vec3f w = angle_vectors({0, way, 0}).forward;
  const float f = dot(w, ax.forward), r = dot(w, ax.right);
  // Each axis pressed when the way lies within 67.5 degrees of it (cos 67.5 = 0.383).
  constexpr float kPress = 0.383f;
  forward = f > kPress ? 1.f : f < -kPress ? -1.f : 0.f;
  side = r > kPress ? 1.f : r < -kPress ? -1.f : 0.f;
}

RocketBurst rocket_burst(const MoveWorld& world, Vec3f origin, Vec3f angles,
                         const RocketJumpSpec& rj) {
  RocketBurst out;
  const Vec3f fwd = angle_vectors(angles).forward;
  const Vec3f start = origin + Vec3f{0, 0, rj.eye} + fwd * 8 - Vec3f{0, 0, 8};
  const Vec3f end = start + fwd * 4096;
  const HullTrace tr = trace_hull(world.solids[0].point_hull, start - world.solids[0].origin,
                                  end - world.solids[0].origin);
  if (tr.fraction >= 1 || tr.startsolid) return out;
  out.ok = true;
  out.at = start + (end - start) * tr.fraction - fwd * rj.back_off;
  out.flight_s = tr.fraction * 4096 / rj.missile_speed;
  return out;
}

float rocket_blast(MoveState& s, Vec3f at, const RocketJumpSpec& rj) {
  const Vec3f centre = s.origin + (kPlayerMins + kPlayerMaxs) * 0.5f;
  const float d = (rj.damage - 0.5f * length(at - centre)) * rj.self_share;
  if (d <= 0) return 0;
  Vec3f away = s.origin - at;
  normalize(away);
  s.velocity = s.velocity + away * (d * rj.push);
  s.ground = -1;
  return d;
}

// A rocket jump's running start keeps at least this share of its speed through the first command.
constexpr float kRunKept = 0.8f;

NavProof prove_rocket_jump(const MoveWorld& world, const MoveSettings& move, Vec3f from, Vec3f to,
                           float speed, const NavSettings& set, const RocketJumpSpec& rj,
                           float* health, std::vector<Vec3f>* path) {
  NavProof p;
  MoveState s;
  s.origin = from;
  s.maxspeed = move.maxspeed;
  Vec3f dir{to.x - from.x, to.y - from.y, 0};
  normalize(dir);
  s.velocity = dir * speed;
  float yaw = yaw_at(from, to);
  const float step_s = static_cast<float>(set.msec) * 0.001f;
  const float floor_z = (from.z < to.z ? from.z : to.z) - kArriveUp;
  p.end = from;  // a refusal ends where it started
  p.lo = from + kPlayerMins;
  p.hi = from + kPlayerMaxs;
  if (path) path->push_back(from);
  bool burst = false;
  float burst_t = -1;
  Vec3f burst_at;
  // The flip's view: behind at the shot, then turned back toward the target in flight.
  float view = rj.back ? yaw + 180 : yaw;
  const float shot_pitch = rj.back ? rj.back_pitch : rj.pitch;
  for (int i = 0;; i++) {
    const float t = static_cast<float>(i + 1) * step_s;
    if (i > 0 && across(s.origin, to) > 0.01f) yaw = yaw_at(s.origin, to);
    if (!rj.back) {
      view = yaw;
    } else if (i > 0) {
      float off = yaw - view;
      while (off > 180) off -= 360;
      while (off < -180) off += 360;
      const float most = rj.turn * step_s;
      view += off > most ? most : off < -most ? -most : off;
    }
    MoveCmd c;
    c.msec = static_cast<uint8_t>(set.msec);
    c.angles = {i == 0 ? shot_pitch : 0, view, 0};
    if (rj.back) {
      // Strafing round: the keys nearest the way to the target in the view's frame.
      float f = 0, r = 0;
      flip_keys(yaw, view, f, r);
      c.forward = static_cast<int16_t>(move.maxspeed * f);
      c.side = static_cast<int16_t>(move.maxspeed * r);
    } else {
      c.forward = static_cast<int16_t>(move.maxspeed);
    }
    c.buttons = i == 0 ? kButtonJump : 0;
    player_move(s, c, move, world);
    // A running start a wall stops at once is not a run (the jumper never has the speed there).
    if (i == 0 && speed > 0 && dot(s.velocity, dir) < kRunKept * speed) return p;
    if (i == 0) {
      // The rocket, fired after the command's move.
      const RocketBurst rb = rocket_burst(world, s.origin, {shot_pitch, view, 0}, rj);
      if (!rb.ok) return p;  // nothing to burst on
      burst_at = rb.at;
      burst_t = t + rb.flight_s;
    }
    if (!burst && t >= burst_t) {
      burst = true;
      const float d = rocket_blast(s, burst_at, rj);
      if (d <= 0) return p;  // too far from the blast: no push
      if (health) *health = d;
    }
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
    if (burst && grounded) {
      // One flight: on the target, or landed elsewhere (`end` tells where).
      p.landed = true;
      p.reached = h <= set.arrive * 3 && dz <= kArriveUp && dz >= -kArriveUp;
      return p;
    }
    if (grounded && s.origin.z < floor_z) return p;
    if (t >= set.limit_s) return p;
  }
}

Vec3f grenade_at(const MoveWorld& world, Vec3f origin, Vec3f angles, float t,
                 const GrenadeJumpSpec& gj, int msec) {
  const Vec3f fwd = angle_vectors(angles).forward;
  Vec3f at = origin + Vec3f{0, 0, gj.blast.eye} + fwd * 8 - Vec3f{0, 0, 8};
  Vec3f v = fwd * gj.speed + Vec3f{0, 0, gj.up};
  const float dt = static_cast<float>(msec) * 0.001f;
  const Solid& w = world.solids[0];
  bool rest = false;
  for (float done = 0; done + dt * 0.5f < t && !rest; done += dt) {
    // The toss: gravity first, then the move, then the bounce off what it met.
    v.z -= gj.gravity * dt;
    const Vec3f end = at + v * dt;
    const HullTrace tr = trace_hull(w.point_hull, at - w.origin, end - w.origin);
    if (tr.startsolid || tr.allsolid) break;
    at = at + (end - at) * tr.fraction;
    if (tr.fraction >= 1) continue;
    v = clip_velocity(v, tr.plane.normal, gj.bounce);
    if (tr.plane.normal.z > 0.7f && v.z < 60) rest = true;
  }
  return at;
}

NavProof prove_grenade_jump(const MoveWorld& world, const MoveSettings& move, Vec3f from, Vec3f to,
                            float lead, const NavSettings& set, const GrenadeJumpSpec& gj,
                            float* health, std::vector<Vec3f>* path) {
  NavProof p;
  MoveState s;
  s.origin = from;
  s.maxspeed = move.maxspeed;
  Vec3f dir{to.x - from.x, to.y - from.y, 0};
  normalize(dir);
  const float aim = yaw_at(from, to);
  const float step_s = static_cast<float>(set.msec) * 0.001f;
  const float floor_z = (from.z < to.z ? from.z : to.z) - kArriveUp;
  p.end = from;
  p.lo = from + kPlayerMins;
  p.hi = from + kPlayerMaxs;
  if (path) path->push_back(from);
  Vec3f burst_at, stand;
  float shot_t = 0;
  bool burst = false, jumped = false;
  const float limit = gj.fuse + set.limit_s;
  for (int i = 0;; i++) {
    const float t = static_cast<float>(i + 1) * step_s;
    MoveCmd c;
    c.msec = static_cast<uint8_t>(set.msec);
    if (i == 0) {
      c.angles = {gj.pitch, aim, 0};  // the shot, standing
    } else if (!jumped && t < shot_t + gj.fuse - gj.jump_before_s) {
      // To the stand past the grenade, slowing into it (the friction stops a player in a few
      // units from a walk).
      const float d = across(s.origin, stand);
      c.angles = {0, d > 1 ? yaw_at(s.origin, stand) : aim, 0};
      const float want = d * 6;
      c.forward = static_cast<int16_t>(d > 2 ? (want < move.maxspeed ? want : move.maxspeed) : 0);
    } else {
      // The jump with the fuse nearly out, then forward at the target through the flight.
      c.angles = {0, across(s.origin, to) > 0.01f ? yaw_at(s.origin, to) : aim, 0};
      c.forward = static_cast<int16_t>(move.maxspeed);
      if (!jumped) c.buttons = kButtonJump;
      jumped = true;
    }
    player_move(s, c, move, world);
    if (i == 0) {
      // The grenade, thrown after the command's move; where it lies when the fuse runs out.
      shot_t = t;
      burst_at = grenade_at(world, s.origin, {gj.pitch, aim, 0}, gj.fuse, gj, set.msec);
      stand = burst_at + dir * lead;
      stand.z = s.origin.z;
    }
    if (!burst && t >= shot_t + gj.fuse) {
      burst = true;
      const float d = rocket_blast(s, burst_at, gj.blast);
      if (d <= 0) return p;  // too far from the blast: no push
      if (health) *health = d;
    }
    p.commands++;
    if (path) path->push_back(s.origin);
    const Vec3f blo = s.origin + kPlayerMins, bhi = s.origin + kPlayerMaxs;
    for (int a = 0; a < 3; a++) {
      if (blo[a] < p.lo[a]) p.lo[a] = blo[a];
      if (bhi[a] > p.hi[a]) p.hi[a] = bhi[a];
    }
    p.end = s.origin;
    p.time_s = t;
    const bool grounded = s.ground >= 0;
    if (burst && grounded) {
      const float h = across(s.origin, to), dz = s.origin.z - to.z;
      p.landed = true;
      p.reached = h <= set.arrive * 3 && dz <= kArriveUp && dz >= -kArriveUp;
      return p;
    }
    if (grounded && s.origin.z < floor_z) return p;
    if (t >= limit) return p;
  }
}

namespace {

// Grows `mark` over the links from every node it marks (a breadth-first search, many sources).
void grow_marks(const NavGraph& g, std::vector<uint8_t>& mark) {
  std::vector<int32_t> queue;
  for (size_t n = 0; n < mark.size(); n++)
    if (mark[n]) queue.push_back(static_cast<int32_t>(n));
  for (size_t q = 0; q < queue.size(); q++) {
    const NavNode& n = g.nodes[static_cast<size_t>(queue[q])];
    for (int32_t k = 0; k < n.links; k++) {
      const int32_t to = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)].to;
      if (mark[static_cast<size_t>(to)]) continue;
      mark[static_cast<size_t>(to)] = 1;
      queue.push_back(to);
    }
  }
}

// The nodes that reach a node `mark` marks (the search over the links backwards), on foot: no
// rocket jump counts as a way.
std::vector<uint8_t> reaching(const NavGraph& g, const std::vector<uint8_t>& mark) {
  std::vector<int32_t> start(g.nodes.size() + 1, 0);
  for (const NavLink& l : g.links)
    if (!nav_blast(l.kind)) start[static_cast<size_t>(l.to) + 1]++;
  for (size_t n = 0; n < g.nodes.size(); n++) start[n + 1] += start[n];
  std::vector<int32_t> from(static_cast<size_t>(start.back())),
      fill(start.begin(), start.end() - 1);
  for (size_t a = 0; a < g.nodes.size(); a++) {
    const NavNode& n = g.nodes[a];
    for (int32_t k = 0; k < n.links; k++) {
      const NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
      if (nav_blast(l.kind)) continue;
      from[static_cast<size_t>(fill[static_cast<size_t>(l.to)]++)] = static_cast<int32_t>(a);
    }
  }
  std::vector<uint8_t> out = mark;
  std::vector<int32_t> queue;
  for (size_t n = 0; n < out.size(); n++)
    if (out[n]) queue.push_back(static_cast<int32_t>(n));
  for (size_t q = 0; q < queue.size(); q++) {
    const auto n = static_cast<size_t>(queue[q]);
    for (int32_t x = start[n]; x < start[n + 1]; x++) {
      const int32_t a = from[static_cast<size_t>(x)];
      if (out[static_cast<size_t>(a)]) continue;
      out[static_cast<size_t>(a)] = 1;
      queue.push_back(a);
    }
  }
  return out;
}

// Pockets: the nodes `outside` marks, grouped by the links among them either way (union-find);
// each node's group, -1 for the rest.
std::vector<int32_t> pockets_of(const NavGraph& g, const std::vector<uint8_t>& outside) {
  std::vector<int32_t> up(g.nodes.size());
  for (size_t n = 0; n < up.size(); n++) up[n] = static_cast<int32_t>(n);
  const auto root = [&](int32_t n) {
    while (up[static_cast<size_t>(n)] != n) {
      up[static_cast<size_t>(n)] = up[static_cast<size_t>(up[static_cast<size_t>(n)])];
      n = up[static_cast<size_t>(n)];
    }
    return n;
  };
  for (size_t a = 0; a < g.nodes.size(); a++) {
    if (!outside[a]) continue;
    const NavNode& n = g.nodes[a];
    for (int32_t k = 0; k < n.links; k++) {
      const int32_t to = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)].to;
      if (!outside[static_cast<size_t>(to)]) continue;
      const int32_t ra = root(static_cast<int32_t>(a)), rb = root(to);
      if (ra != rb) up[static_cast<size_t>(ra < rb ? rb : ra)] = ra < rb ? ra : rb;
    }
  }
  std::vector<int32_t> group(g.nodes.size(), -1);
  for (size_t n = 0; n < group.size(); n++)
    if (outside[n]) group[n] = root(static_cast<int32_t>(n));
  return group;
}

// The floor nodes within `far` across of `p` (the columns round it): a floor node, not a swim
// node, by the grid.
template <class F>
void floor_near(const NavGraph& g, Vec3f p, float far, F&& each) {
  const auto span = static_cast<int32_t>(std::ceil(far / g.set.spacing));
  const auto ci = static_cast<int32_t>((p.x - g.lo.x) / g.set.spacing);
  const auto cj = static_cast<int32_t>((p.y - g.lo.y) / g.set.spacing);
  for (int32_t i = ci - span; i <= ci + span; i++) {
    if (i < 0 || i >= g.cols) continue;
    for (int32_t j = cj - span; j <= cj + span; j++) {
      if (j < 0 || j >= g.rows) continue;
      const size_t c =
          static_cast<size_t>(i) * static_cast<size_t>(g.rows) + static_cast<size_t>(j);
      for (int32_t k = 0; k < g.col_n[c]; k++) each(g.col_first[c] + k);
    }
  }
}

}  // namespace

NavBridgeCounts nav_bridge_pockets(const MoveWorld& world, const MoveSettings& move, NavGraph& g,
                                   std::vector<uint8_t>& main, const NavBridgeSettings& set) {
  NavBridgeCounts out;
  if (main.size() != g.nodes.size()) return out;
  struct Pair {
    float d;
    int32_t from, to;
  };
  const auto avoided = [&](size_t n) { return n < set.avoid.size() && set.avoid[n] != 0; };
  // One way for a pocket: `inward` from the main part into it, else out of it into the main part.
  // True when a link was added to `extra`.
  const std::vector<uint8_t>* landing_back = nullptr;  // the nodes a rocket jump may land on
  const auto bridge = [&](const std::vector<int32_t>& group, int32_t pocket,
                          const std::vector<uint8_t>& land, bool inward, bool rocket,
                          std::vector<NavExtraLink>& extra) {
    const float up = rocket ? set.rocket_up : set.up;
    std::vector<Pair> pairs;
    for (size_t p = 0; p < g.nodes.size(); p++) {
      if (group[p] != pocket || g.nodes[p].swim || g.nodes[p].col < 0) continue;
      const Vec3f po = g.nodes[p].origin;
      floor_near(g, po, set.far, [&](int32_t m) {
        if (!land[static_cast<size_t>(m)] || g.nodes[static_cast<size_t>(m)].mover >= 0 ||
            avoided(static_cast<size_t>(m)))
          return;
        const Vec3f mo = g.nodes[static_cast<size_t>(m)].origin;
        const Vec3f from = inward ? mo : po, to = inward ? po : mo;
        if (to.z - from.z > up || from.z - to.z > set.down || across(from, to) > set.far) return;
        const Vec3f d = to - from;
        pairs.push_back({std::sqrt(dot(d, d)), inward ? m : static_cast<int32_t>(p),
                         inward ? static_cast<int32_t>(p) : m});
      });
    }
    // The nearest first (the node numbers break a tie).
    std::sort(pairs.begin(), pairs.end(), [](const Pair& a, const Pair& b) {
      return a.d != b.d ? a.d < b.d : a.from != b.from ? a.from < b.from : a.to < b.to;
    });
    const auto lands_in = [&](Vec3f end) {
      const int32_t q = nav_nearest(g, end);
      if (q < 0) return -1;
      const bool ok =
          !avoided(static_cast<size_t>(q)) &&
          (inward ? group[static_cast<size_t>(q)] == pocket : land[static_cast<size_t>(q)]);
      const Vec3f o = g.nodes[static_cast<size_t>(q)].origin;
      const float dz = end.z - o.z;
      return ok && across(end, o) <= 24 && dz <= kStepHeight && dz >= -kStepHeight ? q : -1;
    };
    if (rocket) {
      // In tiers, each over the pairs: the flips that land on their target (the skilled jump,
      // placed by its pitch) from the full speed on the nearest pairs with the floor behind for
      // the run and the turn, then from half of it, then standing; then the shots at the feet
      // that do (the nearest pairs); failing all, the best landing of any (`attempt`'s score).
      NavExtraLink first;
      int best = -1;  // the score of `first`, -1: none landed
      // From `at`, by any link but a rocket jump, a node the pocket wants (always, with none
      // wanted).
      const auto leads_to_wanted = [&](int32_t at) {
        if (set.wanted.size() != g.nodes.size()) return true;
        std::vector<uint8_t> seen(g.nodes.size(), 0);
        std::vector<int32_t> queue{at};
        seen[static_cast<size_t>(at)] = 1;
        for (size_t q = 0; q < queue.size(); q++) {
          const size_t n = static_cast<size_t>(queue[q]);
          if (group[n] == pocket && set.wanted[n]) return true;
          for (int32_t k = 0; k < g.nodes[n].links; k++) {
            const NavLink& l =
                g.links[static_cast<size_t>(g.nodes[n].first_link) + static_cast<size_t>(k)];
            if (nav_blast(l.kind) || seen[static_cast<size_t>(l.to)]) continue;
            seen[static_cast<size_t>(l.to)] = 1;
            queue.push_back(l.to);
          }
        }
        return false;
      };
      const auto attempt = [&](size_t x, float share, float pitch, bool back) {
        const Vec3f from = g.nodes[static_cast<size_t>(pairs[x].from)].origin;
        const Vec3f to = g.nodes[static_cast<size_t>(pairs[x].to)].origin;
        const float speed = share * move.maxspeed;
        out.proofs++;
        RocketJumpSpec rj = set.rj;
        rj.back = back;
        (back ? rj.back_pitch : rj.pitch) = pitch;
        const NavProof pr = prove_rocket_jump(world, move, from, to, speed, g.set, rj);
        const int32_t at = pr.reached ? pairs[x].to : lands_in(pr.end);
        // Never onto a node with no way back out (a jumper there would be trapped).
        if (at < 0 || (landing_back && !(*landing_back)[static_cast<size_t>(at)])) return false;
        // On its target, and on the way to what the pocket holds: taken. Any other landing is
        // kept while it is the best yet: leading to what the pocket holds first, then on target
        // (a ledge with nothing on it may be the step to one a later round bridges on foot).
        const int score = (leads_to_wanted(at) ? 2 : 0) + (pr.reached ? 1 : 0);
        if (score > best) {
          best = score;
          first = {pairs[x].from, at,   NavKind::rocket, pr.time_s + set.rocket_cost_s, speed,
                   back,          pitch};
        }
        return score == 3;
      };
      // A running start needs the floor behind to run on, as a jump's does (a flip's the more
      // for its turn).
      const auto room = [&](size_t x, float share, bool back) {
        const Vec3f from = g.nodes[static_cast<size_t>(pairs[x].from)].origin;
        const Vec3f to = g.nodes[static_cast<size_t>(pairs[x].to)].origin;
        return share <= 0 || room_to_run_up(world, from, to,
                                            g.set.run_up + (back ? set.rj.turn_room * share : 0));
      };
      const auto keep = [&] {
        extra.push_back(first);
        out.by_rocket++;
        out.by_flip += first.back;
        return true;
      };
      for (const float share : {1.f, 0.5f, 0.f}) {
        int tried = 0;
        for (size_t x = 0; x < pairs.size() && tried < set.rocket_tries; x++) {
          if (!room(x, share, true)) continue;
          tried++;
          for (const float pitch : set.flip_pitches)
            if (attempt(x, share, pitch, true)) return keep();
        }
      }
      for (size_t x = 0; x < pairs.size() && static_cast<int>(x) < set.rocket_tries; x++)
        for (const float share : {1.f, 0.5f, 0.f})
          if (room(x, share, false) && attempt(x, share, set.rj.pitch, false)) return keep();
      return best >= 0 ? keep() : false;
    }
    for (size_t x = 0; x < pairs.size() && static_cast<int>(x) < set.tries; x++) {
      const Vec3f from = g.nodes[static_cast<size_t>(pairs[x].from)].origin;
      const Vec3f to = g.nodes[static_cast<size_t>(pairs[x].to)].origin;
      struct Way {
        NavKind kind;
        float share;
      };
      const Way ways[] = {{to.z < from.z - kStepHeight ? NavKind::drop : NavKind::walk, 0},
                          {NavKind::jump, 1},
                          {NavKind::jump, 0.75f},
                          {NavKind::jump, 0.5f},
                          {NavKind::jump, 0}};
      for (const Way& w : ways) {
        out.proofs++;
        const float speed = w.share * move.maxspeed;
        const NavProof pr = prove_link(
            world, move, from, to, w.kind == NavKind::drop ? NavKind::walk : w.kind, speed, g.set);
        int32_t at = pr.reached ? pairs[x].to : lands_in(pr.end);
        if (at < 0) continue;
        NavExtraLink l{pairs[x].from, at, w.kind,
                       pr.time_s + (speed == 0 && w.kind == NavKind::jump ? kStandingJump_s : 0),
                       speed};
        extra.push_back(l);
        return true;
      }
    }
    return false;
  };
  // Inward: every pocket the main part does not reach, rounds until none is added; rocket jumps
  // only once no other way adds a pocket (a pocket a later round reaches on foot is not jumped to).
  bool rockets_now = false;
  for (int round = 0; round < 12; round++) {
    std::vector<uint8_t> outside(g.nodes.size());
    for (size_t n = 0; n < outside.size(); n++) outside[n] = main[n] || avoided(n) ? 0 : 1;
    const std::vector<int32_t> group = pockets_of(g, outside);
    const std::vector<uint8_t> back_now = rockets_now ? reaching(g, main) : std::vector<uint8_t>{};
    std::vector<int32_t> size(g.nodes.size(), 0);
    for (const int32_t r : group)
      if (r >= 0) size[static_cast<size_t>(r)]++;
    std::vector<NavExtraLink> extra;
    for (size_t r = 0; r < size.size(); r++) {
      if (size[r] < set.least || group[r] != static_cast<int32_t>(r)) continue;
      if (round == 0) out.pockets++;
      landing_back = rockets_now ? &back_now : nullptr;
      if (rockets_now && !set.wanted.empty()) {
        bool worth = false;
        for (size_t n = 0; n < group.size() && n < set.wanted.size() && !worth; n++)
          worth = group[n] == static_cast<int32_t>(r) && set.wanted[n];
        if (!worth) continue;
      }
      if (bridge(group, static_cast<int32_t>(r), main, true, rockets_now, extra)) out.bridged_in++;
      if (rockets_now && !extra.empty()) break;  // one rocket jump a round, then on foot again
    }
    if (extra.empty()) {
      if (rockets_now || !set.rockets) break;
      rockets_now = true;
      continue;
    }
    rockets_now = false;
    nav_add_links(g, extra);
    grow_marks(g, main);
  }
  // Outward: the nodes the main part reaches that reach none of it back.
  {
    const std::vector<uint8_t> back = reaching(g, main);
    std::vector<uint8_t> stuck(g.nodes.size());
    for (size_t n = 0; n < stuck.size(); n++) stuck[n] = main[n] && !back[n] && !avoided(n) ? 1 : 0;
    // `back` within the main part: the land a way out lands on.
    std::vector<uint8_t> land(g.nodes.size());
    for (size_t n = 0; n < land.size(); n++) land[n] = main[n] && back[n] ? 1 : 0;
    const std::vector<int32_t> group = pockets_of(g, stuck);
    std::vector<NavExtraLink> extra;
    for (size_t r = 0; r < group.size(); r++)
      if (group[r] == static_cast<int32_t>(r) &&
          bridge(group, static_cast<int32_t>(r), land, false, false, extra))
        out.bridged_out++;
    nav_add_links(g, extra);
  }
  return out;
}

NavTrickCounts nav_add_tricks(const MoveWorld& world, const MoveSettings& move, NavGraph& g,
                              std::span<const NavTrick> tricks, std::span<const uint8_t> back,
                              const NavTrickSettings& set) {
  NavTrickCounts out;
  out.rocket_from.assign(tricks.size(), -1);
  out.grenade_from.assign(tricks.size(), -1);
  if (!back.empty() && back.size() != g.nodes.size()) return out;
  // Where a proof ended, as a node: the floor node under it (within 24 across and a step up or
  // down), within `land` of people's landing; -1 none.
  const auto landed = [&](Vec3f end, Vec3f people) {
    const int32_t q = nav_nearest(g, end);
    if (q < 0) return -1;
    const NavNode& n = g.nodes[static_cast<size_t>(q)];
    const float dz = end.z - n.origin.z, pz = n.origin.z - people.z;
    const bool under = across(end, n.origin) <= 24 && dz <= kStepHeight && dz >= -kStepHeight;
    const bool there = across(n.origin, people) <= set.land && pz <= 32 && pz >= -32;
    const bool out_again = back.empty() || back[static_cast<size_t>(q)];
    return under && there && out_again && !n.swim ? q : -1;
  };
  std::vector<NavExtraLink> extra;
  for (size_t t = 0; t < tricks.size(); t++) {
    const NavTrick& tr = tricks[t];
    // The takeoff nodes: the floor nodes within `near` of people's, nearest first.
    struct Near {
      float d;
      int32_t n;
    };
    std::vector<Near> starts;
    floor_near(g, tr.from, set.near, [&](int32_t n) {
      const NavNode& nd = g.nodes[static_cast<size_t>(n)];
      const Vec3f d = nd.origin - tr.from;
      const float dist = std::sqrt(dot(d, d));
      if (dist <= set.near && nd.mover < 0 && !nd.wet) starts.push_back({dist, n});
    });
    std::sort(starts.begin(), starts.end(),
              [](const Near& a, const Near& b) { return a.d != b.d ? a.d < b.d : a.n < b.n; });
    if (static_cast<int>(starts.size()) > set.takeoffs)
      starts.resize(static_cast<size_t>(set.takeoffs));
    const float run = tr.run < move.maxspeed ? tr.run : move.maxspeed;
    const float speeds[3] = {run, run * 0.75f, 0};
    const float offs[5] = {0, -8, 8, -16, 16};
    bool done = !set.rockets;
    for (size_t x = 0; x < starts.size() && !done; x++) {
      const Vec3f from = g.nodes[static_cast<size_t>(starts[x].n)].origin;
      for (const float speed : speeds) {
        if (done) break;
        const float share = speed / move.maxspeed;
        if (speed > 0 && !room_to_run_up(world, from, tr.to,
                                         g.set.run_up + (tr.back ? set.rj.turn_room * share : 0)))
          continue;
        for (const float off : offs) {
          RocketJumpSpec rj = set.rj;
          rj.back = tr.back;
          float pitch = tr.pitch + off;
          pitch = pitch < 10 ? 10 : pitch > 89 ? 89 : pitch;
          (tr.back ? rj.back_pitch : rj.pitch) = pitch;
          out.proofs++;
          const NavProof pr = prove_rocket_jump(world, move, from, tr.to, speed, g.set, rj);
          const int32_t at = pr.landed ? landed(pr.end, tr.to) : -1;
          if (at < 0 || at == starts[x].n) continue;
          NavExtraLink l{starts[x].n, at,   NavKind::rocket, pr.time_s + set.cost_s, speed,
                         tr.back,     pitch};
          l.level = tr.level;
          extra.push_back(l);
          out.held++;
          out.flips += tr.back;
          out.rocket_from[t] = starts[x].n;
          done = true;
          break;
        }
      }
    }
    // The grenade jump: standing at the takeoff, each stand past the grenade in turn.
    done = !set.grenades;
    for (size_t x = 0; x < starts.size() && !done; x++)
      for (const float lead : set.leads) {
        out.proofs++;
        const Vec3f from = g.nodes[static_cast<size_t>(starts[x].n)].origin;
        const NavProof pr = prove_grenade_jump(world, move, from, tr.to, lead, g.set, set.gj);
        const int32_t at = pr.landed ? landed(pr.end, tr.to) : -1;
        if (at < 0 || at == starts[x].n) continue;
        NavExtraLink l{starts[x].n, at, NavKind::grenade, pr.time_s + set.cost_s};
        l.pitch = set.gj.pitch;
        l.lead = lead;
        extra.push_back(l);
        out.grenades++;
        out.grenade_from[t] = starts[x].n;
        done = true;
        break;
      }
  }
  nav_add_links(g, extra);
  return out;
}

void nav_room(NavGraph& g, int most) {
  const size_t n = g.nodes.size();
  g.room.assign(n, 0);
  std::vector<int32_t> queue;
  std::vector<uint8_t> done(n, 0);
  for (size_t a = 0; a < n; a++) {
    const NavNode& nd = g.nodes[a];
    if (nd.swim || nd.col < 0 || nd.wet) {
      done[a] = 1;  // no room: never strayed off the line there
      continue;
    }
    // Its walk links to the eight neighbouring columns, one a column.
    uint8_t seen = 0;
    for (int32_t k = 0; k < nd.links; k++) {
      const NavLink& l = g.links[static_cast<size_t>(nd.first_link) + static_cast<size_t>(k)];
      if (l.kind != NavKind::walk || l.mover >= 0) continue;
      const NavNode& to = g.nodes[static_cast<size_t>(l.to)];
      const int32_t di = to.col - nd.col, dj = to.row - nd.row;
      if (to.wet || di < -1 || di > 1 || dj < -1 || dj > 1 || (di == 0 && dj == 0)) continue;
      const int bit = (di + 1) * 3 + (dj + 1);
      seen = static_cast<uint8_t>(seen | (1u << (bit > 4 ? bit - 1 : bit)));
    }
    if (seen != 0xFF) {
      done[a] = 1;  // an edge: room 0
      queue.push_back(static_cast<int32_t>(a));
    }
  }
  // Breadth first from the edges over the walk links: a step further in, a step more room.
  for (size_t q = 0; q < queue.size(); q++) {
    const auto a = static_cast<size_t>(queue[q]);
    const NavNode& nd = g.nodes[a];
    for (int32_t k = 0; k < nd.links; k++) {
      const NavLink& l = g.links[static_cast<size_t>(nd.first_link) + static_cast<size_t>(k)];
      const auto b = static_cast<size_t>(l.to);
      if (l.kind != NavKind::walk || done[b]) continue;
      done[b] = 1;
      g.room[b] = static_cast<uint8_t>(g.room[a] + 1 < most ? g.room[a] + 1 : most);
      queue.push_back(l.to);
    }
  }
}

std::vector<uint8_t> nav_back(const NavGraph& g, const std::vector<uint8_t>& mark) {
  return reaching(g, mark);
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
    if (n.swim) mix(&n.swim, 1);
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
