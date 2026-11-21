// THE STRAFE'S AND THE HOP'S STRETCH (pilot_step_parts.hpp): how far along the route a strafe or
// a hop runs, the hop's place on it and its aim ahead, the headroom over a hop's jump, a landing
// walled in, and the circle jump that starts a hop.
//
// ge:summary The pilot's stretches: the strafe's and the hop's, the headroom, the circle jump
// ge:keywords pilot strafe_stretch hop_stretch hop_follow hop_aim jump_blocked headroom
// ge:keywords land_walled landwall circling circle-jump hopopen stretch
// ge:skill shooterbots bots
// ge:test tests/shooterbots_pilot_test.cpp
#include "shooterbots/circle_jump.hpp"
#include "shooterbots/pilot_gaps.hpp"
#include "shooterbots/pilot_step_parts.hpp"

#include "shooterbots/kit.hpp"

namespace ge::shooterbots::pilot_parts {

namespace {

// `hopopen`: a circle's side chosen for the open within this of the way, degrees, its arcs looked
// along this long, s.
constexpr float kOpenErr = 30, kOpenLook_s = 0.4f;
constexpr float kGapCurveTop = 560;      // the curve's speed is not what ends it
constexpr float kGapCurveMost_s = 0.6f;  // it runs this long at most
constexpr float kGapCurveCorner = 5;     // and begins with the landing this far off the velocity
constexpr float kLevel = 2;              // a strafe stretch's nodes lie within this of one height

}  // namespace

// The end of a strafe stretch from route[k]: the furthest route index j whose links from k are
// walks on one level, no mover, every node between within `line` of the straight line from the
// player to node j; -1 when the stretch is shorter than `strafe_min`.
int32_t strafe_stretch(const Pilot& p, Vec3f at) {
  const NavGraph& g = *p.g;
  int32_t best = -1;
  const float z = node(g, p.route[p.k]).origin.z;
  for (size_t j = p.k + 1; j < p.route.size(); j++) {
    const NavLink* l = link_of(g, p.route[j - 1], p.route[j]);
    const Vec3f nj = node(g, p.route[j]).origin;
    if (l == nullptr || l->kind != NavKind::walk || l->mover >= 0) break;
    if (p.set.brink && lip(p, *l)) break;
    if (nj.z > z + kLevel || nj.z < z - kLevel) break;
    bool straight = true;
    for (size_t i = p.k + 1; i < j && straight; i++)
      straight = off_segment(node(g, p.route[i]).origin, at, nj) <= p.set.line;
    if (!straight) break;
    if (across(at, nj) >= p.set.strafe_min) best = static_cast<int32_t>(j);
  }
  return best;
}

// THE HOP's stretch from route[k]: the furthest route index whose links from k are walks or drops
// with no mover, none of them the walk into a jump's start; and its length along the route.
// -1 when shorter than `hop_min`.
int32_t hop_stretch(const Pilot& p, Vec3f at) {
  const NavGraph& g = *p.g;
  int32_t best = -1;
  float along = across(at, node(g, p.route[p.k + 1]).origin);
  // Still on the stair: the link's ends a climb over the player's own feet (the climbs below are
  // node to node, and a link's first node is passed across before it is stood level with).
  if (p.set.hop_stairs_walked && (node(g, p.route[p.k]).origin.z - at.z >= p.set.hop_climb ||
                                  node(g, p.route[p.k + 1]).origin.z - at.z >= p.set.hop_climb))
    return -1;
  for (size_t j = p.k + 1; j < p.route.size(); j++) {
    const NavLink* l = link_of(g, p.route[j - 1], p.route[j]);
    if (l != nullptr && l->gap && hops_gaps(p)) {
      best = static_cast<int32_t>(j);  // a gap trick: the hop runs into it and on across it
      continue;
    }
    if (l == nullptr || (l->kind != NavKind::walk && l->kind != NavKind::drop) || l->mover >= 0 ||
        l->teleport || l->ladder || (p.set.brink && lip(p, *l) && !p.set.hop_lips) ||
        (p.set.hop_side_off && l->brink_side))
      break;
    const NavLink* l2 = j + 1 < p.route.size() ? link_of(g, p.route[j], p.route[j + 1]) : nullptr;
    if (l2 != nullptr && leaps(l2->kind) && !(l2->gap && hops_gaps(p))) break;
    if (p.set.hop_stairs_walked &&
        node(g, p.route[j]).origin.z - node(g, p.route[j - 1]).origin.z >= p.set.hop_climb)
      break;  // a climb is walked
    if (j > p.k + 1) along += across(node(g, p.route[j - 1]).origin, node(g, p.route[j]).origin);
    if (along >= p.set.hop_min) best = static_cast<int32_t>(j);
  }
  return best;
}

const char* hop_stretch_end(const Pilot& p) {
  const NavGraph& g = *p.g;
  const auto e = static_cast<size_t>(p.strafe_end_node);
  if (p.strafe_end_node < 0 || e + 1 >= p.route.size()) return "goal";
  const NavLink* l = link_of(g, p.route[e], p.route[e + 1]);
  if (l == nullptr) return "link";
  if (l->kind != NavKind::walk && l->kind != NavKind::drop) return "leap";
  if (l->mover >= 0) return "mover";
  if (l->teleport) return "teleport";
  if (p.set.hop_stairs_walked &&
      node(g, p.route[e + 1]).origin.z - node(g, p.route[e]).origin.z >= p.set.hop_climb)
    return "climb";
  const NavLink* l2 = e + 2 < p.route.size() ? link_of(g, p.route[e + 1], p.route[e + 2]) : nullptr;
  if (l2 != nullptr && leaps(l2->kind)) return "leap";
  if (p.set.brink && lip(p, *l)) return "lip";
  if (p.set.hop_side_off && l->brink_side) return "side";
  return "link";
}

const char* hop_stretch_why(const Pilot& p, Vec3f at) {
  const NavGraph& g = *p.g;
  float along = across(at, node(g, p.route[p.k + 1]).origin);
  if (p.set.hop_stairs_walked && (node(g, p.route[p.k]).origin.z - at.z >= p.set.hop_climb ||
                                  node(g, p.route[p.k + 1]).origin.z - at.z >= p.set.hop_climb))
    return "on a stair";
  for (size_t j = p.k + 1; j < p.route.size(); j++) {
    const NavLink* l = link_of(g, p.route[j - 1], p.route[j]);
    if (l != nullptr && l->gap && hops_gaps(p)) return "a gap";
    const char* end = l == nullptr                                           ? "no link"
                      : l->kind != NavKind::walk && l->kind != NavKind::drop ? "a jump or ride"
                      : l->mover >= 0                                        ? "a mover"
                      : l->teleport                                          ? "a teleporter"
                      : p.set.brink && lip(p, *l) && !p.set.hop_lips         ? "a lip"
                      : p.set.hop_side_off && l->brink_side                  ? "a brink's side"
                                                                             : nullptr;
    const NavLink* l2 = j + 1 < p.route.size() ? link_of(g, p.route[j], p.route[j + 1]) : nullptr;
    if (end == nullptr && l2 != nullptr && leaps(l2->kind) && !(l2->gap && hops_gaps(p)))
      end = "a leap next";
    if (end == nullptr && p.set.hop_stairs_walked &&
        node(g, p.route[j]).origin.z - node(g, p.route[j - 1]).origin.z >= p.set.hop_climb)
      end = "a climb";
    if (end == nullptr && j > p.k + 1)
      along += across(node(g, p.route[j - 1]).origin, node(g, p.route[j]).origin);
    if (end == nullptr && j + 1 == p.route.size()) end = "the goal";
    if (end != nullptr && end[0] == 't' && end[4] == 'g')
      return along < 64              ? "the goal <64"
             : along < 128           ? "the goal <128"
             : along < p.set.hop_min ? "the goal <256"
                                     : "long enough";
    if (end != nullptr) return along < p.set.hop_min ? end : "long enough";
  }
  return "long enough";
}

// The route index k' in [k, end) whose link at lies nearest, from k on (the hop runs past nodes
// without standing on them), and how far along the route from there to node `end` is left.
float hop_follow(Pilot& p, Vec3f at, int32_t end) {
  const NavGraph& g = *p.g;
  auto off = [&](size_t k) {
    return off_segment(at, node(g, p.route[k]).origin, node(g, p.route[k + 1]).origin);
  };
  while (p.k + 2 <= static_cast<size_t>(end) && off(p.k + 1) <= off(p.k)) p.k++;
  float f = 0;
  const Vec3f a = node(g, p.route[p.k]).origin, b = node(g, p.route[p.k + 1]).origin;
  off_segment(at, a, b, &f);
  float left = across(at, b) * 1.0f;
  for (size_t j = p.k + 2; j <= static_cast<size_t>(end); j++)
    left += across(node(g, p.route[j - 1]).origin, node(g, p.route[j]).origin);
  return left;
}

// The point `look` units along the route from where `at` lies on link k, up to node `end`.
Vec3f hop_aim(const Pilot& p, Vec3f at, float look, int32_t end) {
  const NavGraph& g = *p.g;
  float f = 0;
  Vec3f a = node(g, p.route[p.k]).origin, b = node(g, p.route[p.k + 1]).origin;
  off_segment(at, a, b, &f);
  Vec3f from{a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f, b.z};
  float left = look;
  for (size_t k = p.k; k + 1 <= static_cast<size_t>(end); k++) {
    b = node(g, p.route[k + 1]).origin;
    const float d = across(from, b);
    if (d >= left) {
      const float u = left / d;
      return {from.x + (b.x - from.x) * u, from.y + (b.y - from.y) * u, b.z};
    }
    left -= d;
    from = b;
  }
  return node(g, p.route[static_cast<size_t>(end)]).origin;
}

// THE HEADROOM: a hop's jump pressed now meets a ceiling over the route ahead (a lintel over a
// stair down): at each node of the route within the flight's reach (`horizon` s at the speed held)
// and halfway between, the player's box raised from standing there to the height the flight has
// by then (the jump's 270 units/s against gravity, from the takeoff) hits solid. People time the
// jump past it. Walls are not read (the route runs between them); the read ends where the flight
// falls back past the takeoff's height (the landing's business).
bool jump_blocked(const Pilot& p, const MoveWorld& world, Vec3f o, float speed, float horizon) {
  constexpr float kRise = 270, kGravity = 800;
  if (speed < 1) return false;
  const NavGraph& g = *p.g;
  auto low = [&](Vec3f at, float d) {
    const float t = d / speed;
    if (t > horizon) return false;
    const float z = o.z + kRise * t - 0.5f * kGravity * t * t;
    if (z <= at.z) return false;
    const HullTrace tr = player_trace(world, at, {at.x, at.y, z});
    return !tr.startsolid && tr.fraction < 1;
  };
  float d = 0;
  Vec3f from = o;
  for (size_t j = p.k + 1; j <= static_cast<size_t>(p.strafe_end_node) && j < p.route.size(); j++) {
    const Vec3f b = node(g, p.route[j]).origin;
    const float leg = across(from, b);
    const Vec3f mid{(from.x + b.x) * 0.5f, (from.y + b.y) * 0.5f, from.z > b.z ? from.z : b.z};
    if (j > p.k + 1 && low(mid, d + leg * 0.5f)) return true;
    d += leg;
    if (d / speed > horizon) break;
    if (low(b, d)) return true;
    from = b;
  }
  return false;
}

// `landwall`: a jump from `o` at `v` meets a wall within `horizon` on every curve the air can give
// it (straight, and the most turn either way).
bool land_walled(const MoveWorld& world, Vec3f o, Vec3f v, float dt, float horizon) {
  for (int side = -1; side <= 1; side++)
    if (arc_clear(world, o, v, side, dt, horizon) >= horizon) return false;
  return true;
}

// THE CIRCLE JUMP (circle_jump.hpp): the hop's first jump from a curve on the ground. True while
// still curving (the keys and view in `w`); false once due, the jump then taken as any landing's.
bool circling(Pilot& p, const PilotSense& s, float vh, float speed, float err, float dt, int msec,
              Want& w) {
  if (p.circle_s < 0 || p.jump_pressed) return false;
  if (p.no_circle && p.gap_k < 0) {
    p.circle_s = -1;  // (`circleaim`: a view not the hand's own curves nothing: jump now)
    return false;
  }
  MoveSettings move = p.g->move;
  move.maxspeed = s.maxspeed;
  if (p.gap_k >= 0) {
    // INTO A GAP (`gap_circle`, pilot_gaps.hpp): the curve aimed across at its landing, or none.
    bool now = false;
    if (!gap_curve(p, s, msec, err, now)) return false;
    if (now) {
      p.circle_s = -1;
      return false;  // a jump now lands across: gap_takeoff takes it
    }
  }
  if (p.set.hop_open && p.circle_side == 0 && p.gap_k < 0 && s.world != nullptr && err < kOpenErr &&
      err > -kOpenErr) {
    // (`hopopen`) The way near straight ahead: the curve out to the side whose arc stays clear
    // of a wall (a start run up along a wall circles out into the corridor's space).
    const int8_t mine = err > 0 ? int8_t{-1} : int8_t{1};
    const float a = arc_clear(*s.world, s.origin, s.velocity, mine, dt, kOpenLook_s);
    if (a < kOpenLook_s) {
      const float b = arc_clear(*s.world, s.origin, s.velocity, -mine, dt, kOpenLook_s);
      if (b > a) p.circle_side = static_cast<int8_t>(-mine);
    }
  }
  CircleSettings cs = p.set.circle;
  if (p.gap_k >= 0) {
    cs.target = kGapCurveTop;  // the jump goes when one lands across, not at a speed
    cs.most_s = kGapCurveMost_s;
    cs.corner = kGapCurveCorner;
  }
  const CircleStep c = circle_step(vh, speed, err, p.circle_s, static_cast<float>(msec) * 0.001f,
                                   p.circle_side, move, cs);
  if (c.jump) {
    if (p.circle_s > 0) {  // a curve was run (not a straight way's jump at once)
      p.stats.circles++;
      p.stats.circle_speed += speed;
    }
    p.circle_s = -1;
    return false;
  }
  p.circle_s += dt;
  p.circle_side = c.side;
  p.strafe_side = c.side;
  p.curve_keys = c.side;
  w.forward = 1;
  w.side = static_cast<float>(c.side);
  w.yaw = unwrap(c.yaw, p.yaw);
  w.jump = false;
  p.circling_now = true;
  return true;
}

}  // namespace ge::shooterbots::pilot_parts
