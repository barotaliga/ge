// ge:summary The pilot's step: the link's mode, the run-up, the jump, waits, rides, strafe jumps
// ge:keywords pilot pilot_step pilot_go pilot_source run-up strafe-jump wait ride stuck replan
// ge:skill shooter bots
// ge:test tests/shooter_pilot_test.cpp
#include "shooter/pilot.hpp"

#include <utility>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/server.hpp"
#include "shooter/usercmd.hpp"

namespace ge::shooter {

namespace {

constexpr float kRideArrive = 2;    // a ride is over within this of the top node's height, units
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
constexpr float kClear = 4;           // a player waits this far clear of a mover's box, units
constexpr float kLanded = 32;         // a jump has landed on its target within this, units
constexpr float kSlow = 50;           // under this a velocity's way is not read, units/s
constexpr float kSlowing_s = 0.25f;   // a run slows to a jump's speed in about this, s
constexpr float kBrake = 0.25f;       // the least share of maxspeed walked into the goal
constexpr float kFacing = 10;      // a run-up goes when the view is this near the jump's way, deg
constexpr float kOnLine = 10;      // a jump is run at on its line within this across, units
constexpr float kStepHeight = 18;  // the move's step

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
  bool jump = false;
};

bool reached(const Pilot& p, const PilotSense& s, int32_t n) {
  if (!s.onground) return false;
  const NavNode& b = node(*p.g, n);
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

// Whether the link's mover keeps the pilot at its start now.
bool mover_in_way(const Pilot& p, const PilotSense& s, const NavLink& l, int32_t a, int32_t b) {
  if (l.mover < 0 || l.kind == NavKind::ride) return false;
  const NavMover* rest = mover_of(p.g->movers, l.mover);
  const NavMover* now = mover_of(s.movers, l.mover);
  if (rest == nullptr || now == nullptr) return false;
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
  return boxes_meet(lo + kPlayerMins, hi + kPlayerMaxs, now->absmin, now->absmax);
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
    if (l2 != nullptr && l2->kind == NavKind::jump) break;
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
  (void)s;
}

void advance(Pilot& p, const PilotSense& s) {
  p.k++;
  restart(p, s);
  if (p.mode == PilotMode::wait || p.mode == PilotMode::ride || p.mode == PilotMode::air ||
      p.mode == PilotMode::jump || p.mode == PilotMode::run_up)
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
  if (l && l->kind == NavKind::jump) {
    // A jump lands where its flight ends, near the target but rarely on its point.
    const float dz = s.origin.z - node(g, b).origin.z;
    return p.airborne && s.onground && across(s.origin, node(g, b).origin) <= kLanded &&
           dz <= kStandUp && dz >= -kStandUp;
  }
  if (l && l2 && l2->kind == NavKind::jump && l->mover < 0 && l->kind != NavKind::drop)
    return false;
  if (l && l2 && through(*l) && through(*l2) && level(g, a, b) && level(g, b, p.route[k + 2])) {
    float f = 0;
    const float off = off_segment(s.origin, node(g, a).origin, node(g, b).origin, &f);
    if (!on_height(s, node(g, a).origin, node(g, b).origin, f)) return false;
    return across(s.origin, node(g, b).origin) <= p.set.arrive || (f >= 1 && off <= p.set.corner);
  }
  return reached(p, s, b);
}

// On a level walk on through its end (an overshoot off a drop, a corner cut), the next link's
// line is nearer the player than this one's: it is on the next link already.
bool nearer_on(const Pilot& p, const PilotSense& s) {
  if (!s.onground || p.k + 2 >= p.route.size()) return false;
  const NavGraph& g = *p.g;
  const int32_t a = p.route[p.k], b = p.route[p.k + 1], c = p.route[p.k + 2];
  const NavLink* l = link_of(g, a, b);
  const NavLink* l2 = link_of(g, b, c);
  if (!l || !l2 || l->mover >= 0 || !through(*l2) || !level(g, b, c) || l->kind == NavKind::jump)
    return false;
  const float here = off_segment(s.origin, node(g, a).origin, node(g, b).origin);
  float f = 0;
  const float there = off_segment(s.origin, node(g, b).origin, node(g, c).origin, &f);
  return there + 1 < here && on_height(s, node(g, b).origin, node(g, c).origin, f);
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

// The run-up and the jump from S toward T at `v`: line up behind S (the run-up point, `run_up`
// back along the jump's way), run at S with the forward axis held to `v`, and jump at S moving
// at it along the way. True on the command the jump is pressed.
bool approach_jump(Pilot& p, const PilotSense& s, Vec3f S, Vec3f T, float v, Want& w, Vec3f& target,
                   const char*& because) {
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
  // The run-up point: as far back as the graph found room to run up.
  const Vec3f P = S - dir * run_up;
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
  const Vec3f aim = along < -(run_up + p.set.arrive) ? P : S + dir * p.set.look;
  w.yaw = unwrap(yaw_at(s.origin, aim), p.yaw);
  // Down to the jump's speed before the run-up: friction takes about a quarter second.
  w.forward = along > -(run_up + p.set.corner + speed * kSlowing_s) ? v / s.maxspeed : 1;
  const bool fast = along_v >= kJumpAlong * v && along_v <= kJumpOver * v + 16 && cosv >= kJumpAim;
  if (along >= -2 && lat_abs <= kOnLine && fast && s.onground && !p.jump_pressed) {
    w.jump = true;
    because = "at the start at the jump's speed: jump";
    return true;
  }
  because = "running at the jump";
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
  int32_t barred[4];
  int nb = 0;
  for (const int32_t b : p.barred)
    if (b >= 0) barred[nb++] = b;
  p.route = nav_route(*p.g, from, goal, p.search, 1,
                      std::span<const int32_t>(barred, static_cast<size_t>(nb)))
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
        speed >= kStrafeSpeed * s.maxspeed && !(l2 && l2->kind == NavKind::jump)) {
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
    } else if (l.kind == NavKind::ride) {
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
        w.yaw = unwrap(heading(away.x, away.y), p.yaw);
        w.forward = 0.5f;
        because = "a mover in the way: stepping clear of it";
      }
      const NavMover* rest = mover_of(g.movers, l.mover);
      const bool door = rest == nullptr || rest->model == nullptr;
      if (door && p.wait_s > p.set.wait_s && !is_barred(p, l.mover)) {
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
    } else if (l.kind == NavKind::jump && p.airborne) {
      if (!s.onground) {
        p.mode = PilotMode::air;
        w.yaw = unwrap(yaw_at(s.origin, B), p.yaw);
        w.forward = 1;
        because = "in the air: toward the target";
      } else {
        // Landed, and not on the target (passed() would have moved on): plan again.
        p.stats.lost++;
        replanned = true;
        replan(p, s);
        because = "landed short";
      }
    } else if (l.kind == NavKind::jump ||
               (l2 && l2->kind == NavKind::jump && l.mover < 0 && l.kind != NavKind::drop)) {
      // A jump: from this link's start, or (a walk into a jump's start) from the next one's.
      const bool on_it = l.kind == NavKind::jump;
      const Vec3f S = on_it ? A : B;
      const Vec3f T = on_it ? B : node(g, p.route[p.k + 2]).origin;
      const float v = on_it ? l.speed : l2->speed;
      if (approach_jump(p, s, S, T, v, w, target, because)) {
        p.stats.jumps++;
        if (!on_it) advance(p, s);
        p.airborne = true;
        p.mode = PilotMode::air;
      }
    } else {
      if (p.mode == PilotMode::wait || p.mode == PilotMode::run_up || p.mode == PilotMode::jump)
        p.mode = PilotMode::walk;
      target = pursue(p, ahead);
      if (across(ahead, target) > 1) w.yaw = unwrap(yaw_at(ahead, target), p.yaw);
      w.forward = facing(w.yaw, p.yaw);
      // Slowing into the goal (late hands run past a small top and off it, or orbit it).
      const float d = across(ahead, node(*p.g, p.route.back()).origin);
      if (d < p.set.look) w.forward *= d < p.set.look * kBrake ? kBrake : d / p.set.look;
      because = l.kind == NavKind::drop ? "walking off the edge" : "walking the route";
    }

    // STUCK and LOST, on the ground while moving under its own legs.
    if (!replanned && s.onground &&
        (p.mode == PilotMode::walk || p.mode == PilotMode::jump || p.mode == PilotMode::run_up)) {
      const float d = across(s.origin, target);
      if (d < p.best - 0.5f) {
        p.best = d;
        p.best_s = t;
      } else if (t - p.best_s > p.set.stuck_s) {
        p.stats.stuck++;
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
  c.angles = {0, quantize_angle(wrap(p.yaw)), 0};
  c.forward = quantize_axis(static_cast<float>(pressed[1]) * s.maxspeed);
  c.side = quantize_axis(static_cast<float>(pressed[2]) * s.maxspeed);
  p.jump_pressed = pressed[3] > 0.5;
  c.buttons = p.jump_pressed ? kButtonJump : 0;
  return c;
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
  s.maxspeed = me.maxspeed > 0 ? me.maxspeed : 320;
  b.now.resize(p.g->movers.size());
  for (size_t i = 0; i < b.now.size(); i++) {
    const NavMover& m = p.g->movers[i];
    const Edict& e = sv.edicts[static_cast<size_t>(m.ent)];
    b.now[i] = m;
    b.now[i].absmin = e.absmin;
    b.now[i].absmax = e.absmax;
  }
  s.movers = b.now;
  s.world = b.world;
  if ((p.mode == PilotMode::idle || p.mode == PilotMode::arrived) && !b.goals.empty()) {
    if (p.mode == PilotMode::arrived) b.arrivals++;
    for (int32_t& d : p.barred) d = -1;
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
