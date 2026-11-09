// ge:summary The pilot's step: the link's mode, the run-up, the jump, waits, rides, strafe jumps
// ge:keywords pilot pilot_step pilot_go pilot_source run-up strafe-jump wait ride stuck replan
// ge:skill shooter bots
// ge:test tests/shooter_pilot_test.cpp
#include "shooter/pilot.hpp"

#include <algorithm>
#include <utility>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/nav_script.hpp"
#include "shooter/server.hpp"
#include "shooter/swim.hpp"
#include "shooter/usercmd.hpp"

namespace ge::shooter {

namespace {

constexpr float kRideArrive = 2;  // a ride is over within this of the top node's height, units
constexpr float kSwimPitch = 70;  // a swimmer's view pitched at most this far, degrees
constexpr float kSwimRise = 16;   // the next node this much higher: jump held to rise, units
// A script's keys are pressed in full only with the view within ~14 degrees of the way (the cos).
constexpr double kScriptFacing = 0.97;
constexpr float kScriptFell = 40;  // standing this far under a script's lowest step: fallen off
constexpr float kOffFloor = 12;    // a pool's floor swum over this far above its standing height
constexpr float kLaneSettle_s = 0.4f;  // a lane's offset follows the room ahead this fast, s
constexpr float kRideRest = 12;     // a ridden mover this near its rest may be stepped onto, units
constexpr float kStandUp = 8;       // a node is arrived at within this up or down, units
constexpr float kJumpAlong = 0.9f;  // a jump goes moving at least this share of its speed
constexpr float kJumpOver = 1.15f;  // and at most this share (and 16 u/s) of it
constexpr float kJumpAim = 0.95f;   // with the velocity within this cosine of the jump's way
constexpr float kPastStart = 24;    // past a jump's start this far without its speed: run up
constexpr float kStrafeHold = 2;    // the strafe's side holds until the target is this far off, deg
constexpr float kSimpleSide = 3;    // the simple hop takes the side the route is this far off, deg
constexpr float kStrafeSpeed = 0.8f;  // a strafe starts moving at least this share of maxspeed
constexpr float kLevel = 2;           // a strafe stretch's nodes lie within this of one height
constexpr float kJumpKin = 72;   // a jump given up takes those beside it, start and end this near
constexpr int kRunUpTries = 4;   // a jump lined up for this many times and never made is given up
constexpr float kStepOver = 18;  // a mover's top this near a player's feet is a step up, units
constexpr float kClear = 4;      // a player waits this far clear of a mover's box, units
constexpr float kLanded = 32;    // a jump has landed on its target within this, units
constexpr float kSlow = 50;      // under this a velocity's way is not read, units/s
constexpr float kSlowing_s = 0.25f;  // a run slows to a jump's speed in about this, s
constexpr float kBrake = 0.25f;      // the least share of maxspeed walked into the goal
constexpr float kFacing = 10;        // a run-up goes when the view is this near the jump's way, deg
constexpr float kFlipFacing = 25;    // a flip goes with the view this near straight behind, deg
constexpr float kFlipSpare_s = 0.05f;  // a flip's turn starts this much earlier than it takes, s
// In the air (`air`): the aim kAirPast past the target toward the node after it within kAirCorner
// of it, units; slower
// than kAirSlow across, or the aim more than kAirGiveUp off the velocity, forward at it (the
// proof's way); within kAirHold of it, no key; the side key full from kAirFull off, degrees.
constexpr float kAirCorner = 64, kAirPast = 24, kAirSlow = 100;
constexpr float kAirGiveUp = 100, kAirHold = 2, kAirFull = 12;
constexpr float kGrenadeStill = 30;  // a grenade jump fires only this slow, units/s
constexpr float kGrenadeAim = 4;     // and with the view this near the target's way, deg
constexpr float kGrenadeNear = 32;   // and within this of its start, units
constexpr float kGrenadeSlow = 160;  // slowing into the start from this far, units
constexpr float kGrenadeStep = 4;    // the step to its stand: this times the way off, units/s
constexpr float kOnLine = 10;        // a jump is run at on its line within this across, units
constexpr float kStepHeight = 18;    // the move's step

float across(Vec3f a, Vec3f b) {
  const float dx = b.x - a.x, dy = b.y - a.y;
  return root_of(dx * dx + dy * dy);
}

float heading(float dx, float dy) {
  const double r = core::libm::atan2(static_cast<double>(dy), static_cast<double>(dx));
  return static_cast<float>(r * (180.0 / core::kPi));
}

float yaw_at(Vec3f from, Vec3f to) { return heading(to.x - from.x, to.y - from.y); }

// `deg` moved by whole turns to within half a turn of `near`.
float unwrap(float deg, float near) {
  float d = deg - near;
  while (d > 180) d -= 360;
  while (d < -180) d += 360;
  return near + d;
}

float wrap(float deg) { return unwrap(deg, 0); }

// How much of a run toward `want` the view at `now` already faces: all of it within 60 degrees
// (a hand does not ease off for the lag of its own view round a corner), none past 120, between
// by the cosine (late hands sweep wide circles round a point they run at while turning round).
float facing(float want, float now) {
  const double c = core::libm::cos(static_cast<double>(want - now) * (core::kPi / 180));
  return c >= 0.5 ? 1.0f : c > -0.5 ? static_cast<float>(c + 0.5) : 0.0f;
}

const NavNode& node(const NavGraph& g, int32_t n) { return g.nodes[static_cast<size_t>(n)]; }

const NavLink* link_of(const NavGraph& g, int32_t a, int32_t b) {
  const NavNode& n = node(g, a);
  for (int32_t k = 0; k < n.links; k++) {
    const NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
    if (l.to == b) return &l;
  }
  return nullptr;
}

const NavMover* mover_of(std::span<const NavMover> movers, int32_t ent) {
  for (const NavMover& m : movers)
    if (m.ent == ent) return &m;
  return nullptr;
}

bool boxes_meet(Vec3f alo, Vec3f ahi, Vec3f blo, Vec3f bhi) {
  return alo.x < bhi.x && alo.y < bhi.y && alo.z < bhi.z && blo.x < ahi.x && blo.y < ahi.y &&
         blo.z < ahi.z;
}

// Across from `p` to the segment a..b, and how far along it (0..1) the nearest point lies.
float off_segment(Vec3f p, Vec3f a, Vec3f b, float* along = nullptr) {
  const float ex = b.x - a.x, ey = b.y - a.y;
  const float ee = ex * ex + ey * ey;
  float f = ee > 0 ? ((p.x - a.x) * ex + (p.y - a.y) * ey) / ee : 0;
  if (f < 0) f = 0;
  if (f > 1) f = 1;
  if (along) *along = f;
  return across(p, {a.x + ex * f, a.y + ey * f, 0});
}

// A link the pursuit runs on through: a walk with no mover.
bool through(const NavLink& l) { return l.kind == NavKind::walk && l.mover < 0; }

// Two nodes within a step of one height (a stair is a level walk; a drop is not).
bool level(const NavGraph& g, int32_t a, int32_t b) {
  const float dz = node(g, a).origin.z - node(g, b).origin.z;
  return dz <= kStepHeight && dz >= -kStepHeight;
}

// The player stands at the height of the segment a..b where it lies `f` along it (a stair's step
// or a ramp's slope between two nodes is no more than a step off the line between them).
bool on_height(const PilotSense& s, Vec3f a, Vec3f b, float f) {
  const float dz = s.origin.z - (a.z + (b.z - a.z) * f);
  return dz <= kStandUp + 2 && dz >= -kStepHeight - 2;
}

struct Want {
  float yaw = 0, forward = 0, side = 0;  // degrees; shares of maxspeed
  float pitch = 0;                       // the view's pitch, down positive (not on the hands)
  bool jump = false, attack = false;
};

// A leap off the ground at its start: a jump, or a rocket jump played the same way.
bool leaps(NavKind k) { return k == NavKind::jump || k == NavKind::rocket; }
// A flight off the ground on the link: a leap, or a grenade jump (its start played apart).
bool flies(NavKind k) { return leaps(k) || k == NavKind::grenade; }

// A swimmer over a pool's floor (PilotSettings::floor_swim): in the water at most `floor_swim_up`
// over the floor's height `z` (and no more than a step under it).
bool over_floor(const Pilot& p, const PilotSense& s, float z) {
  const float dz = s.origin.z - z;
  return p.set.floor_swim && s.waterlevel >= 2 && !s.onground && dz <= p.set.floor_swim_up &&
         dz >= -kStepHeight - 2;
}

// A pool's floor node, as `floor_swim` swims over it: wet, not a cell's centre, and not on a mover
// (a plat in the water is boarded standing on it).
bool pool_floor(const NavNode& n) { return n.wet && !n.swim && n.mover < 0 && n.ride < 0; }

bool reached(const Pilot& p, const PilotSense& s, int32_t n) {
  const NavNode& b = node(*p.g, n);
  if (pool_floor(b) && over_floor(p, s, b.origin.z))  // a pool's floor node, swum over
    return across(s.origin, b.origin) <= p.set.swim_arrive;
  if (b.swim || s.waterlevel >= 2) {  // a wet cell's centre, or any node to a swimmer: near it
    const Vec3f d = s.origin - b.origin;
    return dot(d, d) <= p.set.swim_arrive * p.set.swim_arrive;
  }
  if (!s.onground) return false;
  // A ride's top is wherever on the mover the player boarded: arrived by height alone.
  if (b.ride >= 0) return s.origin.z >= b.origin.z - kRideArrive;
  if (across(s.origin, b.origin) > p.set.arrive) return false;
  const float dz = s.origin.z - b.origin.z;
  return b.mover >= 0 || (dz <= kStandUp && dz >= -kStandUp);
}

bool is_barred(const Pilot& p, int32_t m) {
  for (const int32_t b : p.barred)
    if (b == m) return true;
  return false;
}

// Whether the link's mover keeps the pilot at its start now. A ride's plat away from its rest
// with a swimmer under its top (`floor_swim`: on a pool's floor under a raised plat, which a plat
// coming down onto him sends back up, so it would never come for him) is in his way too.
bool mover_in_way(const Pilot& p, const PilotSense& s, const NavLink& l, int32_t a, int32_t b) {
  if (l.mover < 0) return false;
  const NavMover* rest = mover_of(p.g->movers, l.mover);
  const NavMover* now = mover_of(s.movers, l.mover);
  if (rest == nullptr || now == nullptr) return false;
  if (l.kind == NavKind::ride) {
    const float up = now->absmin.z - rest->absmin.z;
    return p.set.floor_swim && s.waterlevel >= 2 && (up > kRideRest || up < -kRideRest) &&
           s.origin.z + kPlayerMins.z < now->absmax.z - kStandUp;
  }
  const NavNode& na = node(*p.g, a);
  const NavNode& nb = node(*p.g, b);
  if (rest->model != nullptr) {
    // A ridden mover is stepped onto only near its rest; stepped off any time.
    if (nb.mover != l.mover || na.mover == l.mover) return false;
    const float up = now->absmin.z - rest->absmin.z;
    return up > kRideRest || up < -kRideRest;
  }
  Vec3f lo = na.origin, hi = na.origin;
  for (int i = 0; i < 3; i++) {
    if (nb.origin[i] < lo[i]) lo[i] = nb.origin[i];
    if (nb.origin[i] > hi[i]) hi[i] = nb.origin[i];
  }
  // A door sunk to within a step of the floor (one that opens downward) is walked over.
  return boxes_meet(lo + kPlayerMins + Vec3f{0, 0, kStepOver}, hi + kPlayerMaxs, now->absmin,
                    now->absmax);
}

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
  for (size_t j = p.k + 1; j < p.route.size(); j++) {
    const NavLink* l = link_of(g, p.route[j - 1], p.route[j]);
    if (l == nullptr || (l->kind != NavKind::walk && l->kind != NavKind::drop) || l->mover >= 0)
      break;
    const NavLink* l2 = j + 1 < p.route.size() ? link_of(g, p.route[j], p.route[j + 1]) : nullptr;
    if (l2 != nullptr && leaps(l2->kind)) break;
    if (j > p.k + 1) along += across(node(g, p.route[j - 1]).origin, node(g, p.route[j]).origin);
    if (along >= p.set.hop_min) best = static_cast<int32_t>(j);
  }
  return best;
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

// The most the velocity turns a second in the air with the wish across it (hops.hpp's rule:
// 30 units/s across it a command), degrees/s.
float hop_turn_rate(float speed, float dt) {
  if (speed <= 1 || dt <= 0) return 0;
  const double r = core::libm::atan2(30.0, static_cast<double>(speed));
  return static_cast<float>(r * (180.0 / core::kPi)) / dt;
}

// How long the hop's arc on `side` (-1 left, 1 right) stays clear of walls, up to `horizon`: the
// velocity turned at the move's most each command, the player's box traced along it a step up
// (so a stair's riser is not a wall). Floors and ramps it meets do not count.
float arc_clear(const MoveWorld& world, Vec3f o, Vec3f v, int side, float dt, float horizon) {
  const float speed = root_of(v.x * v.x + v.y * v.y);
  if (speed < 1 || dt <= 0) return horizon;
  const double step =
      -static_cast<double>(side) * hop_turn_rate(speed, dt) * dt * (core::kPi / 180.0);
  const auto c = static_cast<float>(core::libm::cos(step)),
             sn = static_cast<float>(core::libm::sin(step));
  Vec3f at{o.x, o.y, o.z + kStepHeight};
  if (!box_free(world, at)) at = o;
  Vec3f vel{v.x, v.y, 0};
  constexpr int kChunk = 4;  // commands a trace covers
  for (float t = 0; t < horizon;) {
    Vec3f to = at;
    for (int i = 0; i < kChunk; i++) {
      vel = {vel.x * c - vel.y * sn, vel.x * sn + vel.y * c, 0};
      to = to + vel * dt;
    }
    const HullTrace tr = player_trace(world, at, to);
    if (tr.startsolid) return t;
    if (tr.fraction < 1 && tr.plane.normal.z < 0.7f) return t + tr.fraction * dt * kChunk;
    at = to;
    t += dt * kChunk;
  }
  return horizon;
}

void restart(Pilot& p, const PilotSense& s) {
  p.best = 1e30f;
  p.best_s = static_cast<float>(p.stats.time_s);
  p.wait_s = 0;
  p.airborne = false;
  p.grenade_s = -1;
  (void)s;
}

void advance(Pilot& p, const PilotSense& s) {
  p.k++;
  restart(p, s);
  if (p.mode == PilotMode::wait || p.mode == PilotMode::ride || p.mode == PilotMode::air ||
      p.mode == PilotMode::jump || p.mode == PilotMode::run_up || p.mode == PilotMode::swim ||
      p.mode == PilotMode::grenade)
    p.mode = PilotMode::walk;
}

bool replan(Pilot& p, const PilotSense& s) {
  const int32_t goal = p.goal;
  const PilotStats keep = p.stats;
  const bool ok = pilot_go(p, goal, s);
  p.stats.plans = keep.plans + 1;
  return ok;
}

// Whether the link route[k] -> route[k + 1] is done. A walk on through its end (the next link a
// level walk too, no mover) is done once the player is past the end's line or within `arrive`; a
// node a jump starts from is left by the jump itself (approach_jump); anything else is reached
// standing on it.
bool passed(const Pilot& p, const PilotSense& s, size_t k) {
  const NavGraph& g = *p.g;
  const int32_t a = p.route[k], b = p.route[k + 1];
  const NavLink* l = link_of(g, a, b);
  const NavLink* l2 = k + 2 < p.route.size() ? link_of(g, b, p.route[k + 2]) : nullptr;
  // A scripted way is passed when its keys say its last step is reached.
  if (l && l->kind == NavKind::script)
    return p.script_from == a && p.script_to == b && p.script_done;
  if (l && flies(l->kind)) {
    // A jump lands where its flight ends, near the target but rarely on its point.
    const float dz = s.origin.z - node(g, b).origin.z;
    return p.airborne && s.onground && across(s.origin, node(g, b).origin) <= kLanded &&
           dz <= kStandUp && dz >= -kStandUp;
  }
  if (l && l2 && leaps(l2->kind) && l->mover < 0 && l->kind != NavKind::drop) return false;
  if (l && l2 && through(*l) && through(*l2) && level(g, a, b) && level(g, b, p.route[k + 2])) {
    float f = 0;
    const float off = off_segment(s.origin, node(g, a).origin, node(g, b).origin, &f);
    const bool swum =
        pool_floor(node(g, a)) && pool_floor(node(g, b)) &&
        over_floor(p, s, node(g, a).origin.z + (node(g, b).origin.z - node(g, a).origin.z) * f);
    if (!swum && !on_height(s, node(g, a).origin, node(g, b).origin, f)) return false;
    // (a walker keeping a lane off the line passes a node as far off it as the lane is)
    const float lane = p.offset < 0 ? -p.offset : p.offset;
    return across(s.origin, node(g, b).origin) <= p.set.arrive + lane ||
           (f >= 1 && off <= p.set.corner + lane);
  }
  return reached(p, s, b);
}

// On a level walk on through its end (an overshoot off a drop, a corner cut), the next link's
// line is nearer the player than this one's: it is on the next link already.
bool nearer_on(const Pilot& p, const PilotSense& s) {
  if (p.k + 2 >= p.route.size()) return false;
  const NavGraph& g = *p.g;
  const int32_t a = p.route[p.k], b = p.route[p.k + 1], c = p.route[p.k + 2];
  const bool swum =
      pool_floor(node(g, b)) && pool_floor(node(g, c)) && over_floor(p, s, node(g, b).origin.z);
  if (!s.onground && !swum) return false;
  const NavLink* l = link_of(g, a, b);
  const NavLink* l2 = link_of(g, b, c);
  if (!l || !l2 || l->mover >= 0 || !through(*l2) || !level(g, b, c) || leaps(l->kind) ||
      l->kind == NavKind::script)
    return false;
  const float here = off_segment(s.origin, node(g, a).origin, node(g, b).origin);
  float f = 0;
  const float there = off_segment(s.origin, node(g, b).origin, node(g, c).origin, &f);
  return there + 1 < here && (swum || on_height(s, node(g, b).origin, node(g, c).origin, f));
}

// The point `target` moved off the route's line by the walker's lane (PilotSettings::lane): the
// lane drifts to a side and an amount drawn now and then, within the room of the next few nodes,
// and back to the line before anything but a level walk.
Vec3f lane_target(Pilot& p, float dt, float t, Vec3f target) {
  const NavGraph& g = *p.g;
  if (p.set.lane <= 0 || g.room.size() != g.nodes.size() || p.k + 1 >= p.route.size()) {
    p.offset = 0;
    return target;
  }
  if (t >= p.lane_until) {
    p.lane_want = static_cast<float>(core::range(p.lane_rng, -1, 1));
    p.lane_until =
        t + static_cast<float>(core::range(p.lane_rng, p.set.lane_min_s, p.set.lane_max_s));
  }
  float room = 1e9f;
  for (size_t k = p.k; k + 1 < p.route.size() && k < p.k + 3; k++) {
    const NavLink* l = link_of(g, p.route[k], p.route[k + 1]);
    if (l == nullptr || !through(*l) || !level(g, p.route[k], p.route[k + 1])) {
      room = 0;
      break;
    }
    room = std::min({room, static_cast<float>(g.room[static_cast<size_t>(p.route[k])]),
                     static_cast<float>(g.room[static_cast<size_t>(p.route[k + 1])])});
  }
  if (room > 1e8f) room = 0;
  p.lane += (p.lane_want - p.lane) * std::min(1.f, dt / p.set.lane_ease_s);
  const float want = p.lane * room * g.set.spacing * p.set.lane;
  p.offset += (want - p.offset) * std::min(1.f, dt / kLaneSettle_s);
  const Vec3f a = node(g, p.route[p.k]).origin, b = node(g, p.route[p.k + 1]).origin;
  Vec3f side{-(b.y - a.y), b.x - a.x, 0};
  if (normalize(side) <= 0) return target;
  return target + side * p.offset;
}

// The point `look` units along the route from where `at` stands on the current link, carried
// through the nodes a walk passes through (passed() above) and stopped at any other.
Vec3f pursue(const Pilot& p, Vec3f at) {
  const NavGraph& g = *p.g;
  float f = 0;
  Vec3f a = node(g, p.route[p.k]).origin, b = node(g, p.route[p.k + 1]).origin;
  off_segment(at, a, b, &f);
  Vec3f from{a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f, b.z};
  float left = p.set.look;
  for (size_t k = p.k;; k++) {
    b = node(g, p.route[k + 1]).origin;
    const float d = across(from, b);
    if (d >= left) {
      const float u = left / d;
      return {from.x + (b.x - from.x) * u, from.y + (b.y - from.y) * u, b.z};
    }
    left -= d;
    from = b;
    if (k + 2 >= p.route.size()) return b;
    const NavLink* l = link_of(g, p.route[k], p.route[k + 1]);
    const NavLink* l2 = link_of(g, p.route[k + 1], p.route[k + 2]);
    if (!l || !l2 || !through(*l) || !through(*l2) || !level(g, p.route[k], p.route[k + 1]) ||
        !level(g, p.route[k + 1], p.route[k + 2]))
      return b;
  }
}

// A jump missed (landed short, or no nearer its start): the second miss of the same one is not a
// jump this player makes, and it is never planned again.
void missed_jump(Pilot& p, NavExtraLink j) {
  int times = 0;
  for (const NavExtraLink& e : p.short_jumps) times += e.from == j.from && e.to == j.to ? 1 : 0;
  if (p.short_jumps.size() < 256) p.short_jumps.push_back(j);
  if (times != 1) return;
  // It and every jump beside it (the graph lays a jump from each node along an edge): from within
  // kJumpKin of its start to within kJumpKin of its end.
  const NavGraph& g = *p.g;
  const Vec3f fa = node(g, j.from).origin, fb = node(g, j.to).origin;
  for (size_t na = 0; na < g.nodes.size(); na++) {
    const NavNode& n = g.nodes[na];
    if (across(n.origin, fa) > kJumpKin || n.origin.z > fa.z + kJumpKin ||
        n.origin.z < fa.z - kJumpKin)
      continue;
    for (int32_t k = 0; k < n.links; k++) {
      const NavLink& kl = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
      const Vec3f to = node(g, kl.to).origin;
      if (kl.kind != NavKind::jump || across(to, fb) > kJumpKin || to.z > fb.z + kJumpKin ||
          to.z < fb.z - kJumpKin)
        continue;
      const NavExtraLink e{static_cast<int32_t>(na), kl.to, kl.kind, kl.time_s};
      p.failed.push_back(e);
      p.cut.push_back(e);
    }
  }
}

// The run-up and the jump from S toward T at `v`: line up behind S (the run-up point, `run_up`
// back along the jump's way), run at S with the forward axis held to `v`, and jump at S moving
// at it along the way. True on the command the jump is pressed.
// How long before a flip's start its turn round begins: what the view's hand takes to turn 180
// degrees, its delay and its slew over them (within 0.02 s of the time each profile's hand
// takes to come within 25 degrees), and a little to spare; at least a few commands.
float flip_lead_s(const Pilot& p) {
  const ai::AxisHands& h = p.hands.spec(0);
  const double t = h.delay_s + (h.slew > 0 ? 180 / h.slew : 0) + kFlipSpare_s;
  return t > 0.03 ? static_cast<float>(t) : 0.03f;
}

// The view the hands will show when keys pressed now arrive (the keys' delay), the view wanted
// held meanwhile: worked out on a copy of the hands. A turn strafed with keys for the view as it
// is now presses them for a view already gone.
float view_when_keys_land(const Pilot& p, float want, int msec) {
  const ai::AxisHands& k = p.hands.spec(1);
  // The commands in the keys' delay, to the nearest.
  int steps = 0;
  while ((steps + 0.5) * msec < k.delay_s * 1000) steps++;
  if (steps <= 0) return p.yaw;
  ai::Hands h = p.hands;
  const double wanted[4] = {want, p.wanted[1], p.wanted[2], 0};
  double out[4] = {};
  const size_t n = h.axes() < 4 ? h.axes() : 4;
  for (int i = 0; i < steps; i++)
    h.step(std::span<const double>(wanted, n), std::span<double>(out, n));
  return static_cast<float>(out[0]);
}

// A FLIP on the route ahead past only walks: true, `back` the view's heading at its shot (behind,
// away from its target) and `v` its speed; `d_to_flip` how far along the route its start is.
size_t flip_index(const Pilot& p) {
  const NavGraph& g = *p.g;
  for (size_t j = p.k + 1; j + 1 < p.route.size() && j <= p.k + 12; j++) {
    const NavLink* l = link_of(g, p.route[j], p.route[j + 1]);
    if (!l) return 0;
    if (l->kind == NavKind::rocket && l->back) return j;
    if (l->kind != NavKind::walk) return 0;
  }
  return 0;
}

bool flip_ahead(const Pilot& p, const PilotSense& s, float& back, float& v) {
  (void)s;
  const size_t j = flip_index(p);
  if (j == 0) return false;
  const NavGraph& g = *p.g;
  const Vec3f n = node(g, p.route[j]).origin, t = node(g, p.route[j + 1]).origin;
  back = heading(t.x - n.x, t.y - n.y) + 180;
  v = link_of(g, p.route[j], p.route[j + 1])->speed;
  return true;
}

float d_to_flip(const Pilot& p, const PilotSense& s) {
  const size_t j = flip_index(p);
  if (j == 0) return 1e30f;
  Vec3f at = s.origin;
  float went = 0;
  for (size_t i = p.k + 1; i <= j; i++) {
    const Vec3f n = node(*p.g, p.route[i]).origin;
    went += across(at, n);
    at = n;
  }
  return went;
}

// `flip`: a rocket jump played as the flip (nav.hpp `RocketJumpSpec::back`): the view turned round
// into the jump from `rocket_flip_lead_s` before its start, strafing through the turn (the keys a
// hand presses nearest the way, `flip_keys`), and the shot only with the view behind.
bool approach_jump(Pilot& p, const PilotSense& s, Vec3f S, Vec3f T, float v, Want& w, Vec3f& target,
                   const char*& because, bool flip = false, int msec = 13) {
  Vec3f dir{T.x - S.x, T.y - S.y, 0};
  normalize(dir);
  const Vec3f rel{s.origin.x - S.x, s.origin.y - S.y, 0};
  const float along = dot(rel, dir);
  const float lat = rel.x * dir.y - rel.y * dir.x;
  const float lat_abs = lat < 0 ? -lat : lat;
  const float speed = across({}, s.velocity);
  const float along_v = dot(s.velocity, dir);
  const float cosv = speed > 1 ? along_v / speed : 0;
  const float run_up = p.g->set.run_up;
  if (v <= 0) {
    // A standing jump (proven with no run-up: up a stair's tread): to S, then facing T, forward
    // held to the full and the jump pressed, as the proof flew it.
    target = S;
    p.mode = PilotMode::jump;
    const float d = across(s.origin, S);
    if (d > p.set.arrive && along < 0) {
      w.yaw = unwrap(yaw_at(s.origin, S), p.yaw);
      w.forward = d > 2 * p.set.look ? 1 : d / (2 * p.set.look);
      because = "to the foot of a standing jump";
      return false;
    }
    // A standing flip: turned round on the spot, then the back key with the jump.
    const float up = heading(dir.x, dir.y) + (flip ? 180 : 0);
    w.yaw = unwrap(up, p.yaw);
    w.forward = flip ? 0 : 1;
    const float off = wrap(p.yaw - up);
    if (s.onground && !p.jump_pressed && off < kFacing && off > -kFacing) {
      if (flip) flip_keys(heading(dir.x, dir.y), p.yaw, w.forward, w.side);
      w.jump = true;
      because = flip ? "a standing flip" : "a standing jump up";
      return true;
    }
    because = flip ? "turning round for a standing flip" : "turning to a standing jump";
    return false;
  }
  // The run-up point: as far back as the graph found room to run up (a flip's farther: room for
  // its turn at the speed).
  const float back_up = run_up + (flip ? v * flip_lead_s(p) : 0);
  const Vec3f P = S - dir * back_up;
  if (p.mode != PilotMode::run_up && p.mode != PilotMode::jump) p.mode = PilotMode::jump;
  if (p.mode == PilotMode::jump) {
    // Off the line at the start (the run steers back onto it before), or past it without the
    // speed: run up again.
    const bool off_line = along > -2 && (lat_abs > kOnLine || (speed > kSlow && cosv < kJumpAim));
    const bool overran = along > kPastStart || (along > 2 && (along_v < kJumpAlong * v));
    if (off_line || overran) {
      p.mode = PilotMode::run_up;
      p.stats.run_ups++;
      p.best = 1e30f;
    }
  }
  if (p.mode == PilotMode::run_up) {
    target = P;
    const float d = across(s.origin, P);
    const float down = heading(dir.x, dir.y);  // the view down the jump
    if (d > p.set.look * 0.25f) {
      // Slowing into the run-up point, so the run at the start is a run, not a turn.
      w.yaw = unwrap(yaw_at(s.origin, P), p.yaw);
      w.forward = d > 2 * p.set.look ? 1 : d / (2 * p.set.look);
      // Turning before running: the share of the way the view already faces (a run at a point
      // behind sweeps a wide circle round it under late hands).
      w.forward *= facing(w.yaw, p.yaw);
    } else {
      // At the point: stand, turned down the jump (late hands circle a point aimed at).
      w.yaw = unwrap(down, p.yaw);
      w.forward = 0;
    }
    because = "lining up behind the jump";
    const float off = wrap(p.yaw - down);
    if (d <= 2 * p.set.arrive && speed < kSlow && off < kFacing && off > -kFacing) {
      p.mode = PilotMode::jump;
      p.best = 1e30f;
    }
    return false;
  }
  // On the line: run through S at the jump's speed, the view along the way.
  target = S;
  const Vec3f aim = along < -(back_up + p.set.arrive) ? P : S + dir * p.set.look;
  w.yaw = unwrap(yaw_at(s.origin, aim), p.yaw);
  // Down to the jump's speed before the run-up: friction takes about a quarter second.
  w.forward = along > -(run_up + p.set.corner + speed * kSlowing_s) ? v / s.maxspeed : 1;
  bool behind = true;
  if (flip && along > -(v * flip_lead_s(p) + 2)) {
    // The flip: turning round into the jump, strafing through the turn.
    const float way = w.yaw, share = w.forward;
    const float back = heading(dir.x, dir.y) + 180;
    w.yaw = unwrap(back, p.yaw);
    flip_keys(way, view_when_keys_land(p, w.yaw, msec), w.forward, w.side);
    w.forward *= share;
    w.side *= share;
    const float off = wrap(p.yaw - back);
    behind = off < kFlipFacing && off > -kFlipFacing;
  } else if (flip) {
    behind = false;
  }
  const bool fast = along_v >= kJumpAlong * v && along_v <= kJumpOver * v + 16 && cosv >= kJumpAim;
  if (along >= -2 && lat_abs <= kOnLine && fast && behind && s.onground && !p.jump_pressed) {
    w.jump = true;
    because = "at the start at the jump's speed: jump";
    return true;
  }
  because = flip && behind ? "running at the flip, the view behind"
            : flip         ? "running at the flip, turning round"
                           : "running at the jump";
  return false;
}

}  // namespace

const char* pilot_mode_name(PilotMode m) {
  switch (m) {
    case PilotMode::idle:
      return "idle";
    case PilotMode::walk:
      return "walk";
    case PilotMode::run_up:
      return "run_up";
    case PilotMode::jump:
      return "jump";
    case PilotMode::air:
      return "air";
    case PilotMode::wait:
      return "wait";
    case PilotMode::ride:
      return "ride";
    case PilotMode::strafe:
      return "strafe";
    case PilotMode::arrived:
      return "arrived";
    case PilotMode::hop:
      return "hop";
    case PilotMode::swim:
      return "swim";
    case PilotMode::grenade:
      return "grenade";
  }
  return "?";
}

std::vector<ai::AxisHands> pilot_hands(PilotHands profile) {
  std::vector<ai::AxisHands> h(4);
  for (ai::AxisHands& a : h) a.pass = true;
  h[0].lo = -1e9;
  h[0].hi = 1e9;
  if (profile == PilotHands::human) {
    // The view: a mouse's turn, a reaction late, settling with a little overshoot.
    h[0] = {false, 0.15, 0.8, 0.4, 0.08, 1, 900, -1e9, 1e9, 0};
    // A stick's thumb: late, settling over a tenth of a second, across its travel in a quarter.
    h[1] = {false, 0.18, 0, 0.5, 0.1, 1, 8, -1, 1, 0};
    h[2] = {false, 0.18, 0, 0.5, 0.1, 1, 8, -1, 1, 0};
  } else if (profile == PilotHands::practised) {
    // A movement drilled to habit: no reaction to wait for (the hand runs ahead of the eye, as a
    // recorded pro's view stays on his velocity while it turns 200 degrees a second), a little
    // wander, the view's turn and the keys' travel still a hand's.
    h[0] = {false, 0, 0.3, 0.4, 0, 1, 1800, -1e9, 1e9, 0};
    h[1] = {false, 0, 0, 0.5, 0, 1, 30, -1, 1, 0};
    h[2] = {false, 0, 0, 0.5, 0, 1, 30, -1, 1, 0};
  } else if (profile == PilotHands::steady) {
    // A movement known but not drilled: a little late (steered from that far ahead), a
    // wandering view, no spring (a spring this quick rings at the command rate).
    h[0] = {false, 0.03, 1.5, 0.6, 0, 1, 2400, -1e9, 1e9, 0};
    h[1] = {false, 0.03, 0, 0.5, 0, 1, 20, -1, 1, 0};
    h[2] = {false, 0.03, 0, 0.5, 0, 1, 20, -1, 1, 0};
  }
  return h;
}

float pilot_lead_s(PilotHands profile) {
  return profile == PilotHands::human ? 0.15f : profile == PilotHands::steady ? 0.03f : 0.0f;
}

void pilot_init(Pilot& p, const NavGraph& g, const PilotSettings& set,
                std::span<const ai::AxisHands> hands, double hz, uint32_t seed) {
  Pilot fresh;
  p = std::move(fresh);
  p.g = &g;
  p.set = set;
  p.hands = ai::Hands(hands, hz, seed);
  p.lane_rng = core::make_rng(seed * 2246822519u + 3266489917u);
  p.search.reset(g.nodes.size());
}

void pilot_face(Pilot& p, float yaw) {
  p.yaw = yaw;
  const double at[4] = {yaw, 0, 0, 0};
  p.hands.reset(std::span<const double>(at, p.hands.axes() < 4 ? p.hands.axes() : 4));
}

bool pilot_go(Pilot& p, int32_t goal, const PilotSense& s) {
  p.goal = goal;
  p.k = 0;
  p.strafe_end_node = -1;
  p.strafe_side = 0;
  p.stats.plans++;
  restart(p, s);
  const int32_t from = nav_nearest(*p.g, s.origin);
  int32_t barred[4 + 60];
  int nb = 0;
  for (const int32_t b : p.barred)
    if (b >= 0) barred[nb++] = b;
  for (const int32_t b : p.shut)
    if (b >= 0 && nb < 64) barred[nb++] = b;
  std::span<const NavExtraLink> cut = p.cut;
  if (!p.shut_links.empty()) {
    p.cut_all = p.cut;
    p.cut_all.insert(p.cut_all.end(), p.shut_links.begin(), p.shut_links.end());
    cut = p.cut_all;
  }
  p.route = nav_route(*p.g, from, goal, p.search, 1,
                      std::span<const int32_t>(barred, static_cast<size_t>(nb)), cut, p.set.jumper)
                .nodes;
  if (p.route.empty()) {
    p.mode = PilotMode::idle;
    return false;
  }
  p.mode = p.route.size() == 1 ? PilotMode::arrived : PilotMode::walk;
  return true;
}

MoveCmd pilot_step(Pilot& p, const PilotSense& s, int msec, ai::Why* why) {
  const float dt = static_cast<float>(msec) * 0.001f;
  const float t = static_cast<float>(p.stats.time_s);
  p.stats.time_s += static_cast<double>(dt);
  const NavGraph& g = *p.g;
  Want w;
  w.yaw = p.yaw;
  const char* because = "no route";
  const float speed = across({}, s.velocity);
  // Where the player will be when the hands have caught up: the pilot steers from there.
  const Vec3f ahead = s.origin + s.velocity * p.set.lead_s;

  // Through every node already passed (several in a command near a corner).
  for (int guard = 0; p.mode != PilotMode::idle && p.mode != PilotMode::arrived && guard < 8;
       guard++) {
    if (p.k + 1 >= p.route.size()) {
      p.mode = PilotMode::arrived;
      break;
    }
    if (p.mode == PilotMode::strafe || p.mode == PilotMode::hop) break;
    if (passed(p, s, p.k) || nearer_on(p, s)) {
      advance(p, s);
      continue;
    }
    break;
  }
  if (p.mode == PilotMode::arrived) because = "at the goal";

  bool replanned = false;
  if (p.mode != PilotMode::idle && p.mode != PilotMode::arrived) {
    const int32_t a = p.route[p.k], b = p.route[p.k + 1];
    const NavLink* lp = link_of(g, a, b);
    const NavLink l = lp ? *lp : NavLink{b, NavKind::walk, 0, -1, 0};
    const Vec3f A = node(g, a).origin, B = node(g, b).origin;
    const NavLink* l2 = p.k + 2 < p.route.size() ? link_of(g, b, p.route[p.k + 2]) : nullptr;
    Vec3f target = B;

    // The hop's stretch (an expert's), entered on the ground on a walk moving near full speed.
    if (p.set.hop && p.mode == PilotMode::walk && s.onground &&
        (l.kind == NavKind::walk || l.kind == NavKind::drop) && l.mover < 0 &&
        speed >= kStrafeSpeed * s.maxspeed && !(l2 && leaps(l2->kind))) {
      const int32_t j = hop_stretch(p, s.origin);
      if (j >= 0) {
        p.mode = PilotMode::hop;
        p.strafe_end_node = j;
        p.strafe_side = 0;
        p.best = 1e30f;
        p.best_s = t;
        p.stats.hops++;
      }
    }
    float hop_left = 0;
    if (p.mode == PilotMode::hop && !p.set.hop) {
      // The hop turned off mid-stretch (a bot that took the view to aim): walk on from here.
      restart(p, s);
      p.mode = PilotMode::walk;
    }
    if (p.mode == PilotMode::hop) {
      hop_left = hop_follow(p, s.origin, p.strafe_end_node);
      target = node(g, p.route[static_cast<size_t>(p.strafe_end_node)]).origin;
      const float off =
          off_segment(s.origin, node(g, p.route[p.k]).origin, node(g, p.route[p.k + 1]).origin);
      if (hop_left <= p.set.hop_end ||
          (s.onground && speed < p.set.hop_slow * s.maxspeed && !p.jump_pressed)) {
        restart(p, s);
        p.mode = PilotMode::walk;
      } else if (off > p.set.lost) {
        p.stats.lost++;
        replanned = true;
        replan(p, s);
        because = "hopped off the way: plan again";
      } else if (hop_left < p.best - 0.5f) {
        p.best = hop_left;
        p.best_s = t;
      } else if (t - p.best_s > p.set.stuck_s) {
        p.stats.stuck++;
        // No nearer on the way to a jump (its run-up walled in): a miss of that jump.
        if (leaps(l.kind))
          missed_jump(p, {a, b, l.kind, l.time_s});
        else if (l2 && leaps(l2->kind))
          missed_jump(p, {b, p.route[p.k + 2], l2->kind, l2->time_s});
        replanned = true;
        replan(p, s);
        because = "the hop is no nearer: plan again";
      }
    }

    // The strafe stretch, entered on a level walk moving near full speed.
    if (!p.set.hop && p.set.strafe && p.mode == PilotMode::walk && s.onground &&
        l.kind == NavKind::walk && l.mover < 0 && speed >= kStrafeSpeed * s.maxspeed) {
      const int32_t j = strafe_stretch(p, s.origin);
      if (j >= 0) {
        p.mode = PilotMode::strafe;
        p.strafe_end_node = j;
        p.stats.strafes++;
      }
    }
    if (p.mode == PilotMode::strafe) {
      target = node(g, p.route[static_cast<size_t>(p.strafe_end_node)]).origin;
      if (across(s.origin, target) <= p.set.strafe_end) {
        p.k = static_cast<size_t>(p.strafe_end_node) - 1;
        restart(p, s);
        p.mode = PilotMode::walk;
      }
    }

    if (replanned) {
      // Planned again off the hop: this command coasts; the next one starts on the new route.
      w.yaw = p.yaw;
    } else if (p.mode == PilotMode::hop) {
      const float vh = speed > kSlow ? heading(s.velocity.x, s.velocity.y) : p.yaw;
      const float look = speed * p.set.hop_look_s > 64 ? speed * p.set.hop_look_s : 64;
      const Vec3f aim = hop_aim(p, s.origin, look, p.strafe_end_node);
      const float err = wrap(yaw_at(s.origin, aim) - vh);
      // The side the aim lies on (the right side turns the velocity clockwise), held until the
      // aim lies `hop_hold` the other way: arcs that weave round the route.
      p.side_s += dt;
      const int8_t want = err > p.set.hop_hold ? -1 : err < -p.set.hop_hold ? 1 : 0;
      const float swing = err < 0 ? -err : err;
      if (!p.set.hop_simple &&
          (p.strafe_side == 0 || (want != 0 && want != p.strafe_side &&
                                  (p.side_s >= p.set.hop_side_s || swing > p.set.hop_swing)))) {
        p.strafe_side = want != 0 ? want : 1;
        p.side_s = 0;
      }
      if (p.set.hop_simple && (s.onground || p.strafe_side == 0)) {
        // The simple hop: the side the route turns to, chosen at each jump and held through
        // the air (a swap of side swings the view 90 degrees, so it is not done in flight).
        p.strafe_side = err > kSimpleSide ? -1 : err < -kSimpleSide ? 1 : p.strafe_side;
        if (p.strafe_side == 0) p.strafe_side = 1;
      }
      // A wall in the arc: the other side at once, when its arc is the clearer.
      if (!p.set.hop_simple && s.world != nullptr && !s.onground) {
        const float mine =
            arc_clear(*s.world, s.origin, s.velocity, p.strafe_side, dt, p.set.hop_see_s);
        if (mine < p.set.hop_see_s) {
          const float other =
              arc_clear(*s.world, s.origin, s.velocity, -p.strafe_side, dt, p.set.hop_see_s);
          if (other > mine) {
            p.strafe_side = static_cast<int8_t>(-p.strafe_side);
            p.side_s = 0;
          }
        }
      }
      // The view on the velocity, wanted where it will have turned to when late hands get there:
      // the most turn the move gives at this speed, the way the side held turns it.
      const float rate = -static_cast<float>(p.strafe_side) * hop_turn_rate(speed, dt);
      float lead = rate * p.set.lead_s;  // (the simple hop leads by its own turn)
      if (lead > 45) lead = 45;
      if (lead < -45) lead = -45;
      w.side = static_cast<float>(p.strafe_side);
      if (p.set.hop_simple) {
        // Forward and the side key: their wish lies 45 degrees off the view toward the side. Its
        // angle off the velocity steers: at acos(30 / v) the air adds nothing, at 90 it adds 30
        // across (the most turn); between, the turn the route asks for (its error made good in
        // `hop_look_s`), and only as much speed as that turn brings: it keeps its speed on a
        // straight and gains in the curves (a gentle weave on a straight lost more to walls and
        // late hands than it gained, measured on the se-hop lab's trips).
        const double side = static_cast<double>(p.strafe_side);
        const double toward = -side * static_cast<double>(err) * (core::kPi / 180);
        double add = static_cast<double>(speed) * toward * static_cast<double>(dt) /
                     static_cast<double>(p.set.hop_look_s);
        add = add < 0 ? 0 : add > 30 ? 30 : add;
        double c = speed > 30 ? (30 - add) / static_cast<double>(speed) : 0;
        c = c < 0 ? 0 : c > 1 ? 1 : c;
        const double across = core::libm::acos(c) * (180 / core::kPi);
        const float rate_simple = static_cast<float>(add) / (speed > 1 ? speed : 1) / dt *
                                  static_cast<float>(180 / core::kPi);
        w.yaw = unwrap(vh - static_cast<float>(side * (across - 45)) -
                           static_cast<float>(side) * rate_simple * p.set.lead_s,
                       p.yaw);
        w.forward = 1;
      } else {
        w.yaw = unwrap(vh + lead, p.yaw);
        const float err_abs = err < 0 ? -err : err;
        if (err_abs > p.set.hop_brake && speed > s.maxspeed) w.forward = -1;
      }
      if (s.onground) {
        // The landing's own command: jump (before friction) and strafe on.
        w.jump = !p.jump_pressed;
        if (w.jump) p.stats.hop_jumps++;
        because = "a landing: jump on it";
      } else {
        w.jump = false;  // let go in the air, so the landing's press is a fresh one
        because = "hopping: the side key, the view on the velocity";
      }
    } else if (p.mode == PilotMode::strafe) {
      const float tb = yaw_at(s.origin, target);
      if (s.onground) {
        w.yaw = unwrap(tb, p.yaw);
        w.forward = 1;
        w.jump = !p.jump_pressed;
        because = "a landing: jump again";
      } else {
        const float vh = heading(s.velocity.x, s.velocity.y);
        const float err = wrap(tb - vh);
        // The right side turns the velocity clockwise: the side the target is on, held until
        // the target is kStrafeHold off the other way.
        if (err > kStrafeHold)
          p.strafe_side = -1;
        else if (err < -kStrafeHold || p.strafe_side == 0)
          p.strafe_side = 1;
        w.yaw = unwrap(vh, p.yaw);
        w.side = static_cast<float>(p.strafe_side);
        w.jump = !p.jump_pressed;  // pressed afresh every other command: a landing catches one
        because = "in the air: the wish across the velocity";
      }
    } else if (l.kind == NavKind::script && l.script >= 0 &&
               static_cast<size_t>(l.script) < g.scripts.size() && !mover_in_way(p, s, l, a, b)) {
      // A SCRIPTED WAY (nav_script.hpp): the script's own keys, as its proof flew them.
      if (p.script_from != a || p.script_to != b) {
        p.script_from = a;
        p.script_to = b;
        p.script_step = 0;
        p.script_seen = 0;
        p.script_done = false;
        p.best_s = t;
      }
      const NavScript& sc = g.scripts[static_cast<size_t>(l.script)];
      const ScriptKeys k =
          script_keys(sc, p.script_step, s.origin, s.velocity, s.onground, p.set.arrive);
      p.mode = s.onground ? PilotMode::walk : PilotMode::air;
      if (p.script_step > p.script_seen) {
        p.script_seen = p.script_step;
        p.best_s = t;
      }
      float lowest = A.z;
      for (const ScriptStep& st : sc.steps) lowest = st.at.z < lowest ? st.at.z : lowest;
      if (k.done) {
        p.script_done = true;
        because = "a scripted way: its last step";
      } else if ((s.onground && s.origin.z < lowest - kScriptFell) ||
                 t - p.best_s > 2 * p.set.stuck_s) {
        // Fallen off it, or no step further for long: missed (twice: not taken again).
        int times = 0;
        for (const NavExtraLink& e : p.short_jumps) times += e.from == a && e.to == b ? 1 : 0;
        const NavExtraLink e{a, b, l.kind, l.time_s};
        missed_jump(p, e);
        if (times >= 1) {
          p.failed.push_back(e);
          p.cut.push_back(e);
        }
        p.stats.lost++;
        p.script_from = -1;
        replanned = true;
        replan(p, s);
        because = "a scripted way missed: plan again";
      } else {
        // As the proof keys it, but forward only as far as the view already faces the way and
        // the hop only facing it (the hands turn at a person's pace; the stairs are narrow).
        w.yaw = unwrap(k.yaw, p.yaw);
        const double c = core::libm::cos(static_cast<double>(w.yaw - p.yaw) * (core::kPi / 180));
        const float faces = c >= kScriptFacing ? 1.0f
                            : c > 0.5 ? static_cast<float>((c - 0.5) / (kScriptFacing - 0.5))
                                      : 0.0f;
        w.forward = k.forward * faces;
        w.jump = k.jump && c >= kScriptFacing && !p.jump_pressed;
        if (w.jump) p.stats.jumps++;
        because = k.jump ? "a scripted way: a hop up" : "a scripted way";
      }
    } else if (l.kind == NavKind::swim || node(g, b).swim ||
               (s.waterlevel >= 2 && node(g, a).swim) ||
               (p.set.floor_swim && s.waterlevel >= 2 && !s.onground && l.mover < 0 &&
                pool_floor(node(g, a)) && pool_floor(node(g, b)))) {
      // Swimming: the view pitched at the next node, forward, and jump held to rise; out of the
      // water onto a dry node by the climb the nav proved; into it off a dry edge, walked. A
      // pool's floor swum over (`floor_swim`): level, off the floor (where its friction slows a
      // swimmer) and no more than `floor_swim_up` over it, while the way to the next node is
      // clear at that height; else down at the node (under a lintel at the floor's height).
      const Vec3f me = s.origin;
      const bool floor = pool_floor(node(g, b)) && l.kind != NavKind::swim;
      float aim_z = B.z;
      if (floor && p.set.floor_swim && s.world != nullptr) {
        const float z = std::clamp(me.z, B.z + kOffFloor, B.z + p.set.floor_swim_up);
        const HullTrace tr = player_trace(*s.world, me, {B.x, B.y, z});
        if (!tr.startsolid && tr.fraction >= 1) aim_z = z;
      }
      const float flat = floor ? std::max(across(me, B), p.set.look) : across(me, B);
      const float dz = aim_z - me.z;
      w.yaw = unwrap(yaw_at(me, B), p.yaw);
      w.forward = facing(w.yaw, p.yaw);
      if (!node(g, b).swim && !(floor && p.set.floor_swim) && s.waterlevel >= 1 &&
          dz > -kSwimRise) {
        MoveState ms;
        ms.origin = me;
        ms.waterlevel = s.waterlevel;
        const MoveCmd out = swim_out(ms, B, msec);
        w.pitch = out.angles.x;
        w.jump = (out.buttons & kButtonJump) != 0;
        w.forward = 1;
        because = "climbing out of the water";
      } else if (s.waterlevel >= 2 || (!node(g, b).swim && s.waterlevel >= 1)) {
        // Through the water at the next node: a cell's centre, or a pool's floor under it.
        const double up = core::libm::atan2(static_cast<double>(dz), static_cast<double>(flat));
        float pitch = static_cast<float>(-up * (180.0 / core::kPi));
        pitch = pitch > kSwimPitch ? kSwimPitch : pitch < -kSwimPitch ? -kSwimPitch : pitch;
        w.pitch = pitch;
        w.jump = dz > kSwimRise;
        because = "swimming the route";
      } else {
        because = "into the water";
      }
      if (p.mode != PilotMode::swim) {
        p.mode = PilotMode::swim;
        p.best = 1e30f;
        p.best_s = t;
      }
      const Vec3f d = me - B;
      const float d3 = length(d);
      if (d3 < p.best - 0.5f) {
        p.best = d3;
        p.best_s = t;
      } else if (t - p.best_s > 2 * p.set.stuck_s) {
        p.stats.stuck++;
        replanned = true;
        replan(p, s);
        because = "no nearer in the water: plan again";
      }
    } else if (l.kind == NavKind::ride && !mover_in_way(p, s, l, a, b)) {
      if (p.mode != PilotMode::ride) p.stats.rides++;
      p.mode = PilotMode::ride;
      because = "carried up";
      // Over to the top node while carried: the links off the top start there, and a player who
      // boarded at an edge (late hands) walks off the far side of it.
      const float d = across(s.origin, B);
      if (d > p.set.arrive) {
        w.yaw = unwrap(yaw_at(s.origin, B), p.yaw);
        w.forward = facing(w.yaw, p.yaw) * (d < p.set.look * kBrake ? kBrake
                                            : d < p.set.look        ? d / p.set.look
                                                                    : 1);
        because = "carried up, stepping to the middle";
      }
      // A plat in the water (`floor_swim`): a swimmer over its top swims down onto it, as a plat
      // goes up only for a player standing on it.
      const NavMover* now = mover_of(s.movers, l.mover);
      if (p.set.floor_swim && now != nullptr && s.waterlevel >= 2 && !s.onground &&
          s.origin.z > now->absmax.z - kPlayerMins.z + kStandUp) {
        w.yaw = unwrap(yaw_at(s.origin, B), p.yaw);
        w.pitch = kSwimPitch;
        w.forward = 1;
        because = "swimming down onto the plat";
      }
    } else if (mover_in_way(p, s, l, a, b) && p.mode != PilotMode::air) {
      if (p.mode != PilotMode::wait) {
        p.mode = PilotMode::wait;
        p.wait_s = 0;
        p.stats.waits++;
      }
      p.wait_s += dt;
      w.yaw = unwrap(yaw_at(s.origin, B), p.yaw);
      because = "a mover in the way";
      // Waited for clear of the mover's box: a plat coming down on a player stops on him.
      const NavMover* in = mover_of(g.movers, l.mover);
      if (in != nullptr &&
          boxes_meet(s.origin + kPlayerMins - Vec3f{kClear, kClear, 0},
                     s.origin + kPlayerMaxs + Vec3f{kClear, kClear, 0}, in->absmin,
                     Vec3f{in->absmax.x, in->absmax.y, in->absmax.z + in->travel.z})) {
        const Vec3f mid{(in->absmin.x + in->absmax.x) * 0.5f, (in->absmin.y + in->absmax.y) * 0.5f,
                        s.origin.z};
        Vec3f away{s.origin.x - mid.x, s.origin.y - mid.y, 0};
        // Straight out of the box's nearer side.
        const float hx = (in->absmax.x - in->absmin.x) * 0.5f,
                    hy = (in->absmax.y - in->absmin.y) * 0.5f;
        const float ox = (away.x < 0 ? -away.x : away.x) - hx,
                    oy = (away.y < 0 ? -away.y : away.y) - hy;
        if (ox > oy)
          away = {away.x < 0 ? -1.0f : 1.0f, 0, 0};
        else
          away = {0, away.y < 0 ? -1.0f : 1.0f, 0};
        if (p.set.floor_swim && s.world != nullptr) {
          // Out of the side nearest the player that is open (a plat in a shaft is walled on
          // some sides): the four, by how far each is from his box's edge to the mover's.
          const Vec3f sides[4] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
          const float gaps[4] = {in->absmax.x - (s.origin.x + kPlayerMins.x),
                                 (s.origin.x + kPlayerMaxs.x) - in->absmin.x,
                                 in->absmax.y - (s.origin.y + kPlayerMins.y),
                                 (s.origin.y + kPlayerMaxs.y) - in->absmin.y};
          float best = 1e30f;
          for (int i = 0; i < 4; i++) {
            const float go = gaps[i] + kClear;
            if (go >= best) continue;
            const HullTrace tr = player_trace(*s.world, s.origin, s.origin + sides[i] * go);
            if (!tr.startsolid && tr.fraction >= 1) {
              best = go;
              away = sides[i];
            }
          }
        }
        w.yaw = unwrap(heading(away.x, away.y), p.yaw);
        w.forward = 0.5f;
        because = "a mover in the way: stepping clear of it";
      }
      const NavMover* rest = mover_of(g.movers, l.mover);
      const bool door = rest == nullptr || rest->model == nullptr;
      // A door away from its rest is open and still in the way: its leaf stands across this
      // link (a sliding door's half beside the doorway's middle). Every link through it its leaf
      // meets now is cut, and the way planned through the rest of the doorway.
      const NavMover* now_m = mover_of(s.movers, l.mover);
      const bool open = door && rest != nullptr && now_m != nullptr &&
                        (now_m->absmin.x != rest->absmin.x || now_m->absmin.y != rest->absmin.y ||
                         now_m->absmin.z != rest->absmin.z);
      if (open && p.wait_s > p.set.wait_s) {
        const NavGraph& gg = *p.g;
        for (size_t na = 0; na < gg.nodes.size(); na++) {
          const NavNode& n = gg.nodes[na];
          for (int32_t k = 0; k < n.links; k++) {
            const NavLink& kl =
                gg.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
            if (kl.mover != l.mover) continue;
            const Vec3f o = gg.nodes[static_cast<size_t>(kl.to)].origin;
            Vec3f lo = n.origin, hi = n.origin;
            for (int i = 0; i < 3; i++) {
              if (o[i] < lo[i]) lo[i] = o[i];
              if (o[i] > hi[i]) hi[i] = o[i];
            }
            if (boxes_meet(lo + kPlayerMins, hi + kPlayerMaxs, now_m->absmin, now_m->absmax))
              p.cut.push_back({static_cast<int32_t>(na), kl.to, kl.kind, kl.time_s});
          }
        }
        p.stats.barred++;
        replanned = true;
        replan(p, s);
        because = "waited too long at an open door: round its leaf";
      } else if (door && p.wait_s > p.set.wait_s && !is_barred(p, l.mover)) {
        for (int32_t& slot : p.barred)
          if (slot < 0) {
            slot = l.mover;
            break;
          }
        p.stats.barred++;
        replanned = true;
        replan(p, s);
        because = "waited too long: barred";
      }
    } else if (l.kind == NavKind::grenade && !p.airborne) {
      // The grenade jump's start (pilot.hpp `grenade`).
      if (p.mode != PilotMode::grenade) {
        p.mode = PilotMode::grenade;
        p.best_s = t;
      }
      const GrenadeJumpSpec gj;
      Vec3f dir{B.x - A.x, B.y - A.y, 0};
      normalize(dir);
      w.yaw = unwrap(yaw_at(s.origin, B), p.yaw);
      if (p.grenade_s < 0) {
        const float d = across(s.origin, A);
        const float v = root_of(s.velocity.x * s.velocity.x + s.velocity.y * s.velocity.y);
        // Slowing into the start from `kGrenadeSlow` off, and still anywhere within `kGrenadeNear`
        // of it (late hands coming in at a run overshoot a point and circle it); the stand is
        // taken from where the grenade really lies.
        if (d > kGrenadeNear) {
          w.yaw = unwrap(yaw_at(s.origin, A), p.yaw);
          w.forward = facing(w.yaw, p.yaw) * (d < kGrenadeSlow ? d / kGrenadeSlow : 1);
          because = "to a grenade jump's start";
        } else {
          w.pitch = l.pitch > 0 ? l.pitch : gj.pitch;
          const float off = wrap(yaw_at(s.origin, B) - p.yaw);
          if (s.onground && v < kGrenadeStill && (off < 0 ? -off : off) < kGrenadeAim &&
              s.grenade_ready) {
            w.attack = true;
            p.grenade_s = 0;
            const Vec3f at = s.world ? grenade_at(*s.world, s.origin, {w.pitch, wrap(p.yaw), 0},
                                                  gj.fuse, gj, msec)
                                     : s.origin;
            p.grenade_stand = at + dir * l.lead;
            p.grenade_yaw = wrap(yaw_at(s.origin, B));
            p.stats.grenade_jumps++;
            because = "a grenade jump: the grenade at the feet";
          } else {
            because = s.grenade_ready ? "lining up a grenade jump" : "a grenade jump: the launcher";
          }
        }
        // Never fired (walled off its start, the launcher never in hand): planned round.
        if (t - p.best_s > 2 * p.set.stuck_s) {
          missed_jump(p, {a, b, l.kind, l.time_s});
          missed_jump(p, {a, b, l.kind, l.time_s});
          replanned = true;
          replan(p, s);
          because = "a grenade jump never fired: planned round";
        }
      } else {
        p.grenade_s += dt;
        if (p.grenade_s < gj.fuse - gj.jump_before_s) {
          // The view kept on the target, stepped to the stand with the keys in its frame (a
          // person does not turn to walk a few units; late hands turning to a point circle it).
          w.yaw = unwrap(p.grenade_yaw, p.yaw);
          const ViewAxes ax = angle_vectors({0, p.yaw, 0});
          const Vec3f d{p.grenade_stand.x - s.origin.x, p.grenade_stand.y - s.origin.y, 0};
          const auto key = [&](float along) {
            const float k = along * kGrenadeStep / s.maxspeed;
            return k > 1 ? 1.f : k < -1 ? -1.f : k;
          };
          w.forward = key(dot(d, ax.forward));
          w.side = key(dot(d, ax.right));
          because = "to the stand past the grenade, the fuse burning";
        } else {
          w.forward = 1;
          w.jump = true;
          p.airborne = true;
          p.stats.jumps++;
          because = "the grenade jump: jump as the fuse runs out";
        }
      }
    } else if (flies(l.kind) && p.airborne) {
      if (!s.onground) {
        p.mode = PilotMode::air;
        // STEERED IN THE AIR (pilot.hpp `air`): the aim the target, or near it the node after it
        // when the route walks on from there (the momentum carried round the corner it turns).
        Vec3f aim = B;
        if (l2 && (l2->kind == NavKind::walk || l2->kind == NavKind::drop) &&
            across(s.origin, B) < kAirCorner) {
          // A point a little past the target toward the next node: the landing stays on the
          // target, the velocity already turning the route's way.
          Vec3f on = node(g, p.route[p.k + 2]).origin - B;
          on.z = 0;
          const float len = root_of(on.x * on.x + on.y * on.y);
          if (len > 1) aim = B + on * (kAirPast / len);
        }
        const float vh = heading(s.velocity.x, s.velocity.y);
        const float view_off = wrap(p.yaw - vh);
        const float err = wrap(yaw_at(s.origin, aim) - vh);
        const float err_abs = err < 0 ? -err : err;
        if (l.kind == NavKind::rocket && l.back && (view_off > 90 || view_off < -90)) {
          // The flip's view turning back to the target, strafing round the other way.
          w.yaw = unwrap(yaw_at(s.origin, B), p.yaw);
          flip_keys(w.yaw, view_when_keys_land(p, w.yaw, msec), w.forward, w.side);
          because = "in the flip: the view back to the target";
        } else if (!p.set.air_steer || speed < kAirSlow || err_abs > kAirGiveUp) {
          // Slow (straight up off a standing jump), or the aim behind (passed over it): the view
          // at the aim and forward, as the proof flew.
          w.yaw = unwrap(yaw_at(s.origin, aim), p.yaw);
          w.forward = 1;
          because = "in the air: toward the target";
        } else if (err_abs <= kAirHold) {
          // On the line: the view along the velocity, no key (nothing taken off the speed).
          w.yaw = unwrap(vh, p.yaw);
          because = "in the air: on the line";
        } else {
          // Off the line: the side key toward the turn (the right side turns the velocity
          // clockwise), pressed by how far off, the view led on the velocity by the turn the
          // move gives (late hands get there as it has turned).
          const float side = err > 0 ? -1.f : 1.f;
          const float share = err_abs < kAirFull ? err_abs / kAirFull : 1.f;
          float lead = -side * hop_turn_rate(speed, dt) * p.set.lead_s;
          lead = lead > 45 ? 45 : lead < -45 ? -45 : lead;
          w.yaw = unwrap(vh + lead, p.yaw);
          w.side = side * share;
          because = aim.x == B.x && aim.y == B.y ? "in the air: strafing onto the target"
                                                 : "in the air: strafing round the corner";
        }
      } else {
        // Landed, and not on the target (passed() would have moved on): plan again. A jump
        // landed short twice is not one this player makes: never planned again.
        p.stats.lost++;
        missed_jump(p, {a, b, l.kind, l.time_s});
        replanned = true;
        replan(p, s);
        because = "landed short";
      }
    } else if (leaps(l.kind) || (l2 && leaps(l2->kind) && l.mover < 0 && l.kind != NavKind::drop)) {
      // A jump: from this link's start, or (a walk into a jump's start) from the next one's.
      const bool on_it = leaps(l.kind);
      const Vec3f S = on_it ? A : B;
      const Vec3f T = on_it ? B : node(g, p.route[p.k + 2]).origin;
      const float v = on_it ? l.speed : l2->speed;
      const NavExtraLink jl = on_it ? NavExtraLink{a, b, l.kind, l.time_s}
                                    : NavExtraLink{b, p.route[p.k + 2], l2->kind, l2->time_s};
      if (jl.from != p.try_from || jl.to != p.try_to) {
        p.try_from = jl.from;
        p.try_to = jl.to;
        p.tries = 0;
      }
      const int32_t run_ups = p.stats.run_ups;
      const NavLink& jumped = on_it ? l : *l2;
      const bool flip = jumped.kind == NavKind::rocket && jumped.back;
      if (approach_jump(p, s, S, T, v, w, target, because, flip, msec)) {
        // A rocket jump: the shot at the feet with the jump (its blast does the rest); the flip's
        // behind, down at the floor it runs off.
        if (jumped.kind == NavKind::rocket) {
          w.pitch = jumped.pitch > 0 ? jumped.pitch
                    : flip           ? p.set.rocket_back_pitch
                                     : p.set.rocket_pitch;
          w.attack = true;
          because = flip ? "a flip: the shot behind with the jump"
                         : "a rocket jump: the shot at the feet with the jump";
          p.stats.rocket_jumps++;
          if (flip) p.stats.flips++;
        }
        p.stats.jumps++;
        if (!on_it) advance(p, s);
        p.airborne = true;
        p.mode = PilotMode::air;
      } else if (p.stats.run_ups != run_ups && ++p.tries >= kRunUpTries) {
        // Lined up again and again and never off (the run-up walled in, the line never held):
        // not a jump this player makes.
        missed_jump(p, jl);
        missed_jump(p, jl);
        p.tries = 0;
        replanned = true;
        replan(p, s);
        because = "a jump never made: planned round";
      }
    } else {
      if (p.mode == PilotMode::wait || p.mode == PilotMode::run_up || p.mode == PilotMode::jump ||
          p.mode == PilotMode::swim)
        p.mode = PilotMode::walk;
      target = lane_target(p, dt, t, pursue(p, ahead));
      if (across(ahead, target) > 1) w.yaw = unwrap(yaw_at(ahead, target), p.yaw);
      w.forward = facing(w.yaw, p.yaw);
      // Slowing into the goal (late hands run past a small top and off it, or orbit it).
      const float d = across(ahead, node(*p.g, p.route.back()).origin);
      if (d < p.set.look) w.forward *= d < p.set.look * kBrake ? kBrake : d / p.set.look;
      because = l.kind == NavKind::drop ? "walking off the edge" : "walking the route";
      // A flip ahead within its turn: turning round into it already, strafing along the route.
      float back = 0;
      float flip_v = 0;
      if (flip_ahead(p, s, back, flip_v) && d_to_flip(p, s) <= flip_v * flip_lead_s(p) + 2) {
        const float way = w.yaw, share = w.forward;
        w.yaw = unwrap(back, p.yaw);
        flip_keys(way, view_when_keys_land(p, w.yaw, msec), w.forward, w.side);
        w.forward *= share;
        w.side *= share;
        because = "turning round into a flip";
      }
    }

    // STUCK and LOST, on the ground while moving under its own legs (or swimming a walk).
    const bool swimming_walk =
        p.set.floor_swim && s.waterlevel >= 2 && !s.onground && p.mode == PilotMode::walk;
    if (!replanned && swimming_walk && p.k + 1 < p.route.size() &&
        std::min(node(g, p.route[p.k]).origin.z, node(g, p.route[p.k + 1]).origin.z) - s.origin.z >
            p.set.lost_under) {
      p.stats.lost++;
      replanned = true;
      replan(p, s);
      because = "in the water under the way: plan again";
    }
    if (!replanned && (s.onground || swimming_walk) &&
        (p.mode == PilotMode::walk || p.mode == PilotMode::jump || p.mode == PilotMode::run_up)) {
      const float d = across(s.origin, target);
      if (d < p.best - 0.5f) {
        p.best = d;
        p.best_s = t;
      } else if (t - p.best_s > p.set.stuck_s) {
        p.stats.stuck++;
        // No nearer on the way to a jump (its run-up walled in): a miss of that jump.
        if (leaps(l.kind))
          missed_jump(p, {a, b, l.kind, l.time_s});
        else if (l2 && leaps(l2->kind))
          missed_jump(p, {b, p.route[p.k + 2], l2->kind, l2->time_s});
        replan(p, s);
        because = "no nearer: plan again";
      } else if (p.k + 1 < p.route.size() &&
                 off_segment(s.origin, node(g, p.route[p.k]).origin,
                             node(g, p.route[p.k + 1]).origin) > p.set.lost) {
        p.stats.lost++;
        replan(p, s);
        because = "off the way: plan again";
      }
    }
  }

  if (why) {
    why->decide("link");
    why->fact("k", static_cast<double>(p.k));
    why->fact("speed", static_cast<double>(speed));
    why->fact("onground", s.onground ? 1 : 0);
    if (p.k + 1 < p.route.size()) {
      const NavLink* l = link_of(g, p.route[p.k], p.route[p.k + 1]);
      why->fact("kind", l ? static_cast<double>(l->kind) : -1);
      why->fact("to", static_cast<double>(p.route[p.k + 1]));
      why->fact("dist", static_cast<double>(across(s.origin, node(g, p.route[p.k + 1]).origin)));
    }
    why->choose(pilot_mode_name(p.mode), because);
  }

  // Through the hands.
  const double wanted[4] = {w.yaw, w.forward, w.side, w.jump ? 1.0 : 0.0};
  double pressed[4];
  static constexpr const char* kNames[3] = {"yaw", "forward", "side"};
  ai::record_intent(why, kNames, std::span<const double>(wanted, 3));
  p.hands.step(wanted, pressed);
  for (int i = 0; i < 3; i++) {
    p.wanted[i] = wanted[i];
    p.pressed[i] = pressed[i];
  }
  p.yaw = static_cast<float>(pressed[0]);
  MoveCmd c;
  c.msec = static_cast<uint8_t>(msec);
  c.angles = {quantize_angle(w.pitch), quantize_angle(wrap(p.yaw)), 0};
  c.forward = quantize_axis(static_cast<float>(pressed[1]) * s.maxspeed);
  c.side = quantize_axis(static_cast<float>(pressed[2]) * s.maxspeed);
  p.jump_pressed = pressed[3] > 0.5;
  c.buttons =
      static_cast<uint8_t>((p.jump_pressed ? kButtonJump : 0) | (w.attack ? kButtonAttack : 0));
  return c;
}

std::vector<Vec3f> pilot_path(const Pilot& p, Vec3f at, std::vector<uint8_t>* breaks) {
  std::vector<Vec3f> out{at};
  if (breaks) breaks->assign(1, 0);
  if (p.g == nullptr || p.k + 1 >= p.route.size()) return out;
  const NavGraph& g = *p.g;
  bool lane = p.offset != 0;
  for (size_t k = p.k; k + 1 < p.route.size(); k++) {
    const Vec3f a = node(g, p.route[k]).origin, b = node(g, p.route[k + 1]).origin;
    const NavLink* l = link_of(g, p.route[k], p.route[k + 1]);
    lane = lane && l != nullptr && through(*l) && level(g, p.route[k], p.route[k + 1]);
    // A teleporter's link: far across and next to no time.
    if (breaks)
      breaks->push_back(l != nullptr && l->kind == NavKind::walk && l->time_s < 0.2f &&
                                across(a, b) > 4 * g.set.spacing
                            ? 1
                            : 0);
    Vec3f side{-(b.y - a.y), b.x - a.x, 0};
    if (!lane || normalize(side) <= 0) {
      out.push_back(b);
      continue;
    }
    out.push_back(b + side * p.offset);
  }
  return out;
}

namespace {

MoveCmd pilot_next(void* ctx, const Server& sv, int slot, int msec) {
  PilotBot& b = *static_cast<PilotBot*>(ctx);
  Pilot& p = b.pilot;
  const Edict& me = sv.edicts[static_cast<size_t>(slot) + 1];
  PilotSense s;
  s.origin = me.origin;
  s.velocity = me.velocity;
  s.onground = (me.flags & kFlagOnGround) != 0;
  s.waterlevel = me.waterlevel;
  s.maxspeed = me.maxspeed > 0 ? me.maxspeed : 320;
  b.now.resize(p.g->movers.size());
  for (size_t i = 0; i < b.now.size(); i++) {
    const NavMover& m = p.g->movers[i];
    const Edict& e = sv.edicts[static_cast<size_t>(m.ent)];
    b.now[i] = m;
    // Where the mover stands now; one the player's client was not sent this frame (outside its
    // visibility set: a free slot in a client's world) is taken as the graph planned it.
    if (!e.free) {
      b.now[i].absmin = e.absmin;
      b.now[i].absmax = e.absmax;
    }
  }
  s.movers = b.now;
  s.world = b.world;
  s.grenade_ready = b.grenade_ready;
  if ((p.mode == PilotMode::idle || p.mode == PilotMode::arrived) && !b.goals.empty()) {
    if (p.mode == PilotMode::arrived) b.arrivals++;
    for (int32_t& d : p.barred) d = -1;
    p.cut = p.failed;
    for (size_t tries = 0; tries < b.goals.size(); tries++) {
      const int32_t goal = b.goals[b.next_goal];
      b.next_goal = (b.next_goal + 1) % b.goals.size();
      if (pilot_go(p, goal, s)) break;
    }
  }
  ai::Why* why = b.why ? b.why->follow(slot, b.command) : nullptr;
  b.command++;
  return pilot_step(p, s, msec, why);
}

}  // namespace

CmdSource pilot_source(PilotBot& b) { return {&b, pilot_next}; }

}  // namespace ge::shooter
