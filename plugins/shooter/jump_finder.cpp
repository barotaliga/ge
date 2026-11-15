// The jump finder: ledges from the nav graph's floors, starts under them, calculated arcs
// (jump_finder.hpp).
#include "shooter/jump_finder.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <span>
#include <tuple>
#include <utility>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/brush_hull.hpp"

namespace ge::shooter {

namespace {

constexpr float kStep = 18;  // a ledge's nodes within a step of each other

float flat(Vec3f a, Vec3f b) {
  const float dx = a.x - b.x, dy = a.y - b.y;
  return std::sqrt(dx * dx + dy * dy);
}

bool usable(const NavNode& n) { return !n.swim && !n.wet && n.mover < 0 && n.ride < 0; }

// A start: a ledge's floor, or a lift's top where it carries a player (a ride node).
bool start_ok(const NavNode& n) { return !n.swim && !n.wet && (n.mover < 0 || n.ride >= 0); }

// The world a jump from node `n` is flown in: a lift's top with the lift there (`there`, filled
// only then).
const MoveWorld& world_at(const MoveWorld& world, const NavGraph& g, const NavNode& n,
                          MoveWorld& there) {
  if (n.ride < 0 || world.count >= kMaxSolids) return world;
  for (const NavMover& m : g.movers)
    if (m.ent == n.ride) {
      there = world;
      there.solids[there.count++] = mover_solid(m, m.rest + m.travel);
      return there;
    }
  return world;
}

// Whether node `n` is on an edge: a neighbouring column holds no floor within a step of it.
bool on_edge(const NavGraph& g, const NavNode& n) {
  for (int di = -1; di <= 1; ++di)
    for (int dj = -1; dj <= 1; ++dj) {
      if (di == 0 && dj == 0) continue;
      const int32_t i = n.col + di, j = n.row + dj;
      if (i < 0 || j < 0 || i >= g.cols || j >= g.rows) return true;
      const size_t c = size_t(i) * size_t(g.rows) + size_t(j);
      bool near = false;
      for (int32_t k = 0; k < g.col_n[c] && !near; ++k)
        near = std::fabs(g.nodes[size_t(g.col_first[c] + k)].origin.z - n.origin.z) <= kStep;
      if (!near) return true;
    }
  return false;
}

struct Ledge {
  std::vector<int32_t> nodes;
  float z = 0;
  bool edge = false;
};

std::vector<Ledge> ledges_of(const NavGraph& g) {
  const size_t nn = g.swim_first >= 0 ? size_t(g.swim_first) : g.nodes.size();
  std::vector<int32_t> owner(nn, -1);
  std::vector<Ledge> out;
  for (size_t s = 0; s < nn; ++s) {
    if (owner[s] >= 0 || !usable(g.nodes[s])) continue;
    Ledge l;
    std::vector<int32_t> todo{int32_t(s)};
    owner[s] = int32_t(out.size());
    while (!todo.empty()) {
      const int32_t a = todo.back();
      todo.pop_back();
      const NavNode& na = g.nodes[size_t(a)];
      l.nodes.push_back(a);
      l.edge = l.edge || on_edge(g, na);
      for (int di = -1; di <= 1; ++di)
        for (int dj = -1; dj <= 1; ++dj) {
          const int32_t i = na.col + di, j = na.row + dj;
          if (i < 0 || j < 0 || i >= g.cols || j >= g.rows) continue;
          const size_t c = size_t(i) * size_t(g.rows) + size_t(j);
          for (int32_t k = 0; k < g.col_n[c]; ++k) {
            const int32_t b = g.col_first[c] + k;
            if (size_t(b) >= nn || owner[size_t(b)] >= 0 || !usable(g.nodes[size_t(b)])) continue;
            if (std::fabs(g.nodes[size_t(b)].origin.z - na.origin.z) > kStep) continue;
            owner[size_t(b)] = owner[s];
            todo.push_back(b);
          }
        }
    }
    std::sort(l.nodes.begin(), l.nodes.end());
    std::vector<float> zs;
    zs.reserve(l.nodes.size());
    for (const int32_t n : l.nodes) zs.push_back(g.nodes[size_t(n)].origin.z);
    std::nth_element(zs.begin(), zs.begin() + std::ptrdiff_t(zs.size() / 2), zs.end());
    l.z = zs[zs.size() / 2];
    out.push_back(std::move(l));
  }
  return out;
}

// The walk from node `a` to node `b` (the graph's links but rocket and grenade jumps), s; -1 none.
float walk_time(const NavGraph& g, int32_t a, int32_t b, ai::Search& search) {
  const NavRoute r = nav_route(g, a, b, search);
  return r.nodes.empty() ? -1.0f : r.time_s;
}

}  // namespace

namespace {

constexpr float kJumpUp = 270;     // the jump's speed up, units/s
constexpr float kStepS = 1.0f / 72;  // the arc's step, s
constexpr float kFloorZ = 0.7f;    // a plane this steep or flatter is a floor
constexpr float kOverLip = 32;     // a ledge is landed this far under the arc's top
constexpr float kAirWish = 30;     // the air's wish speed a held key gains toward

// The view's yaw from `a` toward `b`, degrees.
float yaw_to(Vec3f a, Vec3f b) {
  const double r = core::libm::atan2(double(b.y - a.y), double(b.x - a.x));
  return float(r * (180.0 / core::kPi));
}

// The flat way from `a` toward `b`, a unit vector (x along the world when they stand together).
Vec3f way_of(Vec3f a, Vec3f b) {
  Vec3f w{b.x - a.x, b.y - a.y, 0};
  if (normalize(w) <= 0) w = {1, 0, 0};
  return w;
}

// The blast of a burst at `at` on a player at `origin`: his speed added and the points it deals
// him (the game's: the damage less half the distance to his middle, his own share of it).
float blast(Vec3f origin, Vec3f at, const RocketJumpSpec& rj, Vec3f& velocity) {
  const Vec3f centre = origin + (kPlayerMins + kPlayerMaxs) * 0.5f;
  const float d = (rj.damage - 0.5f * length(at - centre)) * rj.self_share;
  if (d <= 0) return 0;
  Vec3f away = origin - at;
  normalize(away);
  velocity = velocity + away * (d * rj.push);
  return d;
}

}  // namespace

RocketArc calc_rocket_jump(const MoveWorld& world, const MoveSettings& move, Vec3f from,
                           Vec3f toward, float speed, float lift_vz, bool back, float pitch,
                           const RocketJumpSpec& rj, float limit_s, float turn) {
  RocketArc out;
  // The take-off's heading: the way to `toward`, turned by `turn`.
  const float head = yaw_to(from, toward) + turn;
  Vec3f way = angle_vectors({0, head, 0}).forward;
  way.z = 0;
  normalize(way);
  const float yaw = head + (back ? 180.0f : 0.0f);
  const RocketBurst rb = rocket_burst(world, from, {pitch, yaw, 0}, rj);
  if (!rb.ok) return out;
  // The take-off: the run, the jump and what the floor adds; the rocket flies while he rises.
  Vec3f v = way * speed + Vec3f{0, 0, kJumpUp + lift_vz};
  const float g = move.gravity, tb = rb.flight_s;
  Vec3f p = from + v * tb - Vec3f{0, 0, 0.5f * g * tb * tb};
  v.z -= g * tb;
  // A burst the jumper has already climbed out of the box of: no wall between them is checked
  // (the burst is under or behind him, inside his reach).
  const HullTrace up = player_trace(world, from, p);
  if (up.fraction < 1) return out;
  out.health = blast(p, rb.at, rj, v);
  if (out.health <= 0) return out;
  out.fired = true;
  out.points.push_back(from);
  out.points.push_back(p);
  float t = tb;
  float top = p.z;
  while (t < limit_s) {
    const Vec3f next = p + v * kStepS - Vec3f{0, 0, 0.5f * g * kStepS * kStepS};
    const HullTrace tr = player_trace(world, p, next);
    if (tr.startsolid || tr.allsolid) return out;
    v.z -= g * kStepS;
    // Forward held in the air, the view on `toward`: the move's air gain that way, up to its
    // 30 units/s (what steers a jump round a corner).
    const Vec3f wish = way_of(p, toward);
    const float along = v.x * wish.x + v.y * wish.y;
    if (along < kAirWish) {
      const float add = std::min(move.accelerate * move.maxspeed * kStepS, kAirWish - along);
      v = v + wish * add;
    }
    t += kStepS;
    if (tr.fraction < 1) {
      p = tr.endpos;
      const Vec3f n = tr.plane.normal;
      if (n.z >= kFloorZ && v.z <= 0) {
        out.landed = true;
        out.end = p;
        out.time_s = t;
        out.points.push_back(p);
        out.apex = top - from.z;
        return out;
      }
      // Along the wall or the ceiling it met.
      const float into = dot(v, n);
      if (into < 0) v = v - n * into;
    } else {
      p = next;
    }
    top = std::max(top, p.z);
    out.points.push_back(p);
  }
  out.apex = top - from.z;
  return out;
}

float rocket_reach(const MoveSettings& move, const RocketJumpSpec& rj) {
  // Straight down onto an open floor 24 units under the origin: the burst `back_off` over it.
  const float eye = rj.eye - 8, floor = -24 + rj.back_off;
  const float tb = (eye - floor) / rj.missile_speed, g = move.gravity;
  Vec3f v{0, 0, kJumpUp};
  const Vec3f p{0, 0, kJumpUp * tb - 0.5f * g * tb * tb};
  v.z -= g * tb;
  if (blast(p, {0, 0, floor}, rj, v) <= 0) return 0;
  // The origin's top less what the box needs over a lip to come down onto the floor past it.
  return p.z + v.z * v.z / (2 * g) - kOverLip;
}


NavProof prove_any_rocket_jump(const MoveWorld& world, const MoveSettings& move, Vec3f from,
                               Vec3f to, const NavSettings& set, const RocketJumpSpec& base,
                               bool direct, std::span<const int> runs, RocketJumpTry* how,
                               float* health, int64_t* proofs,
                               const std::function<bool(const NavProof&)>& lands) {
  struct Way {
    float speed;
    int run;
    bool back;
    float pitch;
  };
  std::vector<Way> ways;
  if (direct)
    for (const float speed : {0.0f, 320.0f})
      for (const bool back : {false, true})
        for (const float pitch : {back ? 55.0f : 80.0f, back ? 40.0f : 65.0f})
          ways.push_back({speed, 0, back, pitch});
  for (const int run : runs)
    for (const bool back : {true, false})
      for (const float pitch : {back ? 55.0f : 80.0f, back ? 40.0f : 65.0f})
        ways.push_back({0, run, back, pitch});
  NavProof last;
  for (const Way& w : ways) {
    RocketJumpSpec rj = base;
    rj.back = w.back;
    if (w.back)
      rj.back_pitch = w.pitch;
    else
      rj.pitch = w.pitch;
    rj.run = w.run;
    float hp = 0;
    std::vector<Vec3f> path;
    if (proofs) ++*proofs;
    last = prove_rocket_jump(world, move, from, to, w.speed, set, rj, &hp, &path);
    if (!last.reached && lands && last.landed && lands(last)) last.reached = true;
    if (!last.reached) continue;
    if (how) {
      how->speed = w.speed;
      how->run = w.run;
      how->back = w.back;
      how->pitch = w.pitch;
      how->launch = w.run > 0 && size_t(w.run) < path.size() ? path[size_t(w.run)] : from;
    }
    if (health) *health = hp;
    return last;
  }
  return last;
}

namespace {

// What the floor under a start adds up to a run of `speed` along `way`: a ramp's rise.
float ramp_lift(const MoveWorld& world, Vec3f from, Vec3f way, float speed) {
  if (speed <= 0) return 0;
  const HullTrace tr = player_trace(world, from + Vec3f{0, 0, 1}, from - Vec3f{0, 0, 8});
  if (tr.fraction >= 1 || tr.plane.normal.z < kFloorZ || tr.plane.normal.z >= 0.999f) return 0;
  const Vec3f n = tr.plane.normal;
  const float up = -speed * (n.x * way.x + n.y * way.y) / n.z;
  return up > 0 ? up : 0;
}

// The climb of the lift a ride node stands on, units/s (0: none).
float lift_climb(const NavGraph& g, const NavNode& n, float fallback) {
  if (n.ride < 0) return 0;
  for (const NavMover& m : g.movers)
    if (m.ent == n.ride) {
      const float ride = m.ride_s - 1;  // its wait at rest, 1 s, then the travel
      return ride > 0.05f && m.travel.z > 0 ? m.travel.z / ride : fallback;
    }
  return fallback;
}

struct Try {
  int32_t start = -1, target = -1;
  RocketArc arc;
  float speed = 0, lift_vz = 0, pitch = 0;
  bool back = false;
  JumpLift by = JumpLift::none;
};

// Every way from node `s` toward node `t`, the ones that land where `lands` accepts, highest
// landing first, at most one a way.
void try_ways(const MoveWorld& world, const MoveSettings& move, const NavGraph& g,
              const JumpFinderSettings& set, int32_t s, int32_t t, MoveWorld& there,
              const std::function<bool(Vec3f)>& lands, JumpFinderCounts& tally,
              std::vector<Try>& out) {
  const NavNode& ns = g.nodes[size_t(s)];
  const Vec3f from = ns.origin, to = g.nodes[size_t(t)].origin;
  const MoveWorld& w = world_at(world, g, ns, there);
  const Vec3f way = way_of(from, to);
  const float plat = lift_climb(g, ns, set.lift_speed);
  const float rise = to.z - from.z;
  struct Way {
    float speed;
    bool back;
    float pitch;
  };
  static constexpr Way kWays[] = {{0, false, 89},  {0, false, 80},   {0, false, 70},
                                  {1, false, 89},  {1, false, 80},   {1, false, 70},
                                  {1, true, 65},   {1, true, 50},    {1, true, 35}};
  // Straight over, or (a wall across the way at the ledge's height) taken off at an angle and
  // steered round in the air.
  const Vec3f over{0, 0, rise > 0 ? rise + 40 : 40};
  const bool open = point_trace(w, from + over, Vec3f{to.x, to.y, from.z} + over).fraction >= 1;
  static constexpr float kStraight[] = {0}, kRound[] = {0, -20, 20, -40, 40};
  const std::span<const float> turns = open ? std::span<const float>(kStraight) : kRound;
  for (const float turn : turns)
  for (const float lift_on : {1.0f, 0.0f}) {
    if (lift_on > 0 && plat <= 0 && ns.ride >= 0) continue;
    for (const Way& wy : kWays) {
      const float speed = wy.speed * set.run;
      float lift_vz = 0;
      JumpLift by = JumpLift::none;
      if (ns.ride >= 0) {
        if (lift_on > 0) lift_vz = plat, by = JumpLift::lift;
      } else {
        if (lift_on == 0) continue;  // one pass for a floor
        lift_vz = ramp_lift(w, from, way, speed);
        if (lift_vz > 0) by = JumpLift::ramp;
      }
      // Too low however the blast falls: at most its whole push, straight up.
      const RocketJumpSpec rj;
      const float vz = kJumpUp + lift_vz + rj.damage * rj.self_share * rj.push;
      if (vz * vz / (2 * move.gravity) < rise) continue;
      ++tally.arcs;
      RocketArc arc = calc_rocket_jump(w, move, from, to, speed, lift_vz, wy.back, wy.pitch, {}, 3,
                                       turn);
      if (!arc.landed || !lands(arc.end)) continue;
      ++tally.landed;
      Try tr;
      tr.start = s;
      tr.target = t;
      tr.arc = std::move(arc);
      tr.speed = speed;
      tr.lift_vz = lift_vz;
      tr.pitch = wy.pitch;
      tr.back = wy.back;
      tr.by = by;
      out.push_back(std::move(tr));
      return;  // the first way that makes it (the plainest)
    }
  }
}

FoundJump found_of(const NavGraph& g, const Try& t, float reach, int ledge, float walk) {
  FoundJump j;
  j.from = g.nodes[size_t(t.start)].origin;
  j.to = t.arc.end;
  j.launch = j.from;
  j.rise = j.to.z - j.from.z;
  j.speed = t.speed;
  j.lift_vz = t.lift_vz;
  j.by = t.by;
  j.back = t.back;
  j.pitch = t.pitch;
  j.lifted = j.rise > reach && t.lift_vz > 0;
  j.health = t.arc.health;
  j.time_s = t.arc.time_s;
  j.apex = t.arc.apex;
  j.ledge = ledge;
  j.walk_s = walk;
  j.arc = t.arc.points;
  return j;
}

}  // namespace

std::vector<FoundJump> find_jumps(const MoveWorld& world, const MoveSettings& move,
                                  const NavGraph& g, const JumpFinderSettings& set,
                                  JumpFinderCounts* counts) {
  JumpFinderCounts tally;
  std::vector<FoundJump> out;
  const float reach = set.reach > 0 ? set.reach : rocket_reach(move);
  tally.reach = reach;
  std::vector<Ledge> ledges = ledges_of(g);
  std::vector<size_t> order;
  for (size_t i = 0; i < ledges.size(); ++i)
    if (ledges[i].edge && int(ledges[i].nodes.size()) >= set.ledge_nodes) order.push_back(i);
  std::stable_sort(order.begin(), order.end(),
                   [&](size_t a, size_t b) { return ledges[a].z > ledges[b].z; });
  if (int(order.size()) > set.max_ledges) order.resize(size_t(set.max_ledges));
  const size_t nn = g.swim_first >= 0 ? size_t(g.swim_first) : g.nodes.size();
  std::vector<int32_t> ledge_of(nn, -1);
  for (size_t i = 0; i < ledges.size(); ++i)
    for (const int32_t n : ledges[i].nodes) ledge_of[size_t(n)] = int32_t(i);
  std::vector<uint8_t> edge(nn, 0);
  for (size_t i = 0; i < nn; ++i) edge[i] = usable(g.nodes[i]) && on_edge(g, g.nodes[i]);
  const float lo = set.near * reach, hi = set.lift * reach;
  ai::Search search(g.nodes.size());
  const auto there = std::make_unique<MoveWorld>();
  for (const size_t li : order) {
    const Ledge& l = ledges[li];
    ++tally.ledges;
    std::vector<int32_t> rim;
    for (const int32_t n : l.nodes)
      if (edge[size_t(n)]) rim.push_back(n);
    if (rim.empty()) continue;
    // The starts: under the ledge by `lo` to `hi`, within `across` of its rim; the nearest of each
    // 96-unit cell (and kind: floor, ramp, lift top) of each lower ledge.
    struct Start {
      int32_t node;
      float dist;
    };
    std::vector<Start> cand;
    for (size_t s = 0; s < nn; ++s) {
      const NavNode& ns = g.nodes[s];
      if (!start_ok(ns) || ledge_of[s] == int32_t(li)) continue;
      const float rise = l.z - ns.origin.z;
      if (rise < lo || rise > hi) continue;
      float bd = 1e30f;
      for (const int32_t t : rim) bd = std::min(bd, flat(ns.origin, g.nodes[size_t(t)].origin));
      if (bd > set.across || bd < 24) continue;
      cand.push_back({int32_t(s), bd});
    }
    std::stable_sort(cand.begin(), cand.end(),
                     [](const Start& a, const Start& b) { return a.dist < b.dist; });
    std::vector<std::tuple<int32_t, int32_t, int32_t, int32_t>> seen;
    std::vector<int32_t> starts;
    for (const Start& c : cand) {
      const NavNode& cn = g.nodes[size_t(c.node)];
      const auto key = std::make_tuple(cn.ride >= 0 ? -2 - cn.ride : ledge_of[size_t(c.node)],
                                       int32_t(std::floor(cn.origin.x / 96)),
                                       int32_t(std::floor(cn.origin.y / 96)),
                                       int32_t(std::floor(cn.origin.z / 48)));
      if (std::find(seen.begin(), seen.end(), key) != seen.end()) continue;
      seen.push_back(key);
      starts.push_back(c.node);
      if (int(starts.size()) >= set.starts) break;
    }
    const auto lands = [&](Vec3f e) {
      const int32_t n = nav_nearest(g, e);
      return n >= 0 && size_t(n) < nn && ledge_of[size_t(n)] == int32_t(li) &&
             std::fabs(g.nodes[size_t(n)].origin.z - e.z) < 4;
    };
    std::vector<Try> made;
    for (const int32_t s : starts) {
      ++tally.tried;
      // The rim's nodes nearest the start.
      std::vector<std::pair<float, int32_t>> near_rim;
      for (const int32_t t : rim) near_rim.push_back({flat(g.nodes[size_t(s)].origin, g.nodes[size_t(t)].origin), t});
      std::stable_sort(near_rim.begin(), near_rim.end(),
                       [](const auto& a, const auto& b) { return a.first < b.first; });
      const size_t before = made.size();
      for (size_t k = 0; k < near_rim.size() && k < size_t(set.targets); ++k) {
        try_ways(world, move, g, set, s, near_rim[k].second, *there, lands, tally, made);
        if (made.size() > before) break;
      }
    }
    // The highest of them a walk does not match.
    std::stable_sort(made.begin(), made.end(), [&](const Try& a, const Try& b) {
      return a.arc.end.z - g.nodes[size_t(a.start)].origin.z >
             b.arc.end.z - g.nodes[size_t(b.start)].origin.z;
    });
    for (const Try& t : made) {
      const float walk = walk_time(g, t.start, t.target, search);
      if (walk >= 0 && walk < t.arc.time_s + set.shortcut_s) {
        ++tally.walkable;
        continue;
      }
      FoundJump j = found_of(g, t, reach, int(li), walk);
      if (j.rise < lo) continue;
      (j.lifted ? tally.lifted : tally.plain)++;
      out.push_back(std::move(j));
      break;
    }
  }
  if (counts) *counts = tally;
  return out;
}

std::vector<FoundJump> jumps_from(const MoveWorld& world, const MoveSettings& move,
                                  const NavGraph& g, Vec3f at, const JumpFinderSettings& set,
                                  JumpFinderCounts* counts) {
  JumpFinderCounts tally;
  std::vector<FoundJump> out;
  const float reach = set.reach > 0 ? set.reach : rocket_reach(move);
  tally.reach = reach;
  const int32_t start = nav_nearest(g, at);
  if (start < 0) return out;
  const Vec3f from = g.nodes[size_t(start)].origin;
  const std::vector<Ledge> ledges = ledges_of(g);
  ai::Search search(g.nodes.size());
  const auto there = std::make_unique<MoveWorld>();
  for (size_t li = 0; li < ledges.size(); ++li) {
    const Ledge& l = ledges[li];
    if (std::find(l.nodes.begin(), l.nodes.end(), start) != l.nodes.end()) continue;
    if (int(l.nodes.size()) < set.ledge_nodes || !l.edge) continue;
    std::vector<std::pair<float, int32_t>> rim;
    for (const int32_t t : l.nodes)
      if (on_edge(g, g.nodes[size_t(t)])) rim.push_back({flat(from, g.nodes[size_t(t)].origin), t});
    if (rim.empty()) continue;
    std::stable_sort(rim.begin(), rim.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });
    const float rise = l.z - from.z, bd = rim[0].first;
    const bool up = rise >= 48, across_drop = rise > -96 && bd >= 192;
    if ((!up && !across_drop) || rise > set.lift * reach || bd > set.across * 1.5f) continue;
    ++tally.ledges;
    ++tally.tried;
    const auto lands = [&](Vec3f e) {
      const int32_t n = nav_nearest(g, e);
      return n >= 0 && std::find(l.nodes.begin(), l.nodes.end(), n) != l.nodes.end() &&
             std::fabs(g.nodes[size_t(n)].origin.z - e.z) < 4;
    };
    std::vector<Try> made;
    for (size_t k = 0; k < rim.size() && k < size_t(set.targets) && made.empty(); ++k)
      try_ways(world, move, g, set, start, rim[k].second, *there, lands, tally, made);
    if (made.empty()) continue;
    const float walk = walk_time(g, start, made[0].target, search);
    if (walk >= 0 && walk < made[0].arc.time_s + set.shortcut_s) {
      ++tally.walkable;
      continue;
    }
    FoundJump j = found_of(g, made[0], reach, int(li), walk);
    (j.lifted ? tally.lifted : tally.plain)++;
    out.push_back(std::move(j));
  }
  if (counts) *counts = tally;
  return out;
}

}  // namespace ge::shooter
