// THE ROUTE FOLLOWED (pilot_step_parts.hpp): a node reached, a link passed or a walk on the next
// already, the lane off the route's line, the pursuit's point along it, a chain's pickup run
// through, the aim pulled back to what the box reaches, and the walk foreseen.
//
// ge:summary The pilot's route followed: reached, passed, the lane, the pursuit, a chain's pickup
// ge:keywords pilot reached passed nearer_on lane lane_target pursue pursuit via_aim via_check
// ge:keywords aim_clear foresee walk-foreseen chain pickup
// ge:skill shooter bots
// ge:test tests/shooter_pilot_test.cpp
#include <algorithm>

#include "core/prng.hpp"
#include "shooter/pilot_step_parts.hpp"

namespace ge::shooter::pilot_parts {

namespace {

constexpr float kRideArrive = 2;       // a ride is over within this of the top node's height, units
constexpr float kLaneSettle_s = 0.4f;  // a lane's offset follows the room ahead this fast, s
constexpr float kLanded = 32;          // a jump has landed on its target within this, units
constexpr float kPlatformAt = 24;      // a moving platform this near a ride's stop has come to it
// A chain's pickup (`pilot_chain`): run through with the player's box over the item's (each 32
// across: centres within 32 across, the feet from 24 under the item's floor to 72 over it), or
// left behind once past its node and this far from it.
constexpr float kViaTouch = 32, kViaUnder = 24, kViaOver = 72, kViaPast = 64;

}  // namespace

bool reached(const Pilot& p, const PilotSense& s, int32_t n) {
  const NavNode& b = node(*p.g, n);
  if (pool_floor(b) && over_floor(p, s, b.origin.z))  // a pool's floor node, swum over
    return across(s.origin, b.origin) <= p.set.swim_arrive;
  if (b.swim || s.waterlevel >= 2) {  // a wet cell's centre, or any node to a swimmer: near it
    const Vec3f d = s.origin - b.origin;
    return dot(d, d) <= p.set.swim_arrive * p.set.swim_arrive;
  }
  if (!s.onground) return false;
  // A moving platform's top: anywhere on it (it carries him off the node's fixed place).
  if (b.stop >= 0) return aboard(p, s, b.mover);
  // A ride's top is wherever on the mover the player boarded: arrived by height alone.
  if (b.ride >= 0) return s.origin.z >= b.origin.z - kRideArrive;
  // A node on a plat: anywhere on its top (stepped onto, the player is on it; late hands
  // circling its node let it leave without him); a door's, near it.
  if (b.mover >= 0 && aboard(p, s, b.mover)) return true;
  if (across(s.origin, b.origin) > p.set.arrive) return false;
  const float dz = s.origin.z - b.origin.z;
  if (b.mover >= 0) return on_mover_top(p, s, b.mover);
  return dz <= kStandUp && dz >= -kStandUp;
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
  // A ride on a moving platform is done aboard it once it stands at the ride's stop.
  if (l && l->kind == NavKind::ride && l->stop >= 0 && l->mover >= 0) {
    const NavMover* rest = mover_of(g.movers, l->mover);
    const NavMover* now = mover_of(s.movers, l->mover);
    if (rest == nullptr || now == nullptr || !moving_platform(*rest)) return false;
    const Vec3f d = platform_at(*rest, *now) - rest->stops[static_cast<size_t>(l->stop)];
    return aboard(p, s, l->mover) && d.x * d.x + d.y * d.y + d.z * d.z <= kPlatformAt * kPlatformAt;
  }
  // A scripted way is passed when its keys say its last step is reached.
  if (l && l->kind == NavKind::script)
    return p.script_from == a && p.script_to == b && p.script_done;
  if (l && flies(l->kind)) {
    // A jump lands where its flight ends, near the target but rarely on its point.
    const float dz = s.origin.z - node(g, b).origin.z;
    return p.airborne && s.onground && across(s.origin, node(g, b).origin) <= kLanded &&
           dz <= kStandUp && dz >= -kStandUp;
  }
  if (l && l2 && leaps(l2->kind) && l2->escape < 0 && l->mover < 0 && l->kind != NavKind::drop)
    return false;
  if (l && l2 && through(*l) && through(*l2) && level(g, a, b, p.set.slopes) &&
      level(g, b, p.route[k + 2], p.set.slopes)) {
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
  if (!l || !l2 || l->mover >= 0 || !through(*l2) || !level(g, b, c, p.set.slopes) ||
      leaps(l->kind) || l->kind == NavKind::script)
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
    if (l == nullptr || !through(*l) || !level(g, p.route[k], p.route[k + 1], p.set.slopes) ||
        p.via_k == static_cast<int32_t>(k + 1)) {  // (a chain's pickup ahead: on the line)
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
Vec3f pursue(const Pilot& p, Vec3f at, float look) {
  const NavGraph& g = *p.g;
  float f = 0;
  Vec3f a = node(g, p.route[p.k]).origin, b = node(g, p.route[p.k + 1]).origin;
  off_segment(at, a, b, &f);
  Vec3f from{a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f, b.z};
  float left = look;
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
    if (!l || !l2 || !through(*l) || !through(*l2) ||
        !level(g, p.route[k], p.route[k + 1], p.set.slopes) ||
        !level(g, p.route[k + 1], p.route[k + 2], p.set.slopes))
      return b;
    if (p.set.brink_line && (lip(p, *l) || lip(p, *l2))) return b;  // a lip's corner: turned at
  }
}

// A chain's pickup within `look` of `at`, or its node passed, and the box's way to it clear (the
// map's solids, when the sense has them): the aim is the item itself (a walk or a hop runs through
// it, not by its node); otherwise `aim`.
Vec3f via_aim(const Pilot& p, const PilotSense& s, Vec3f at, float look, Vec3f aim) {
  if (p.via_k < 0) return aim;
  if (across(at, p.via_at) > look && p.k < static_cast<size_t>(p.via_k)) return aim;
  const Vec3f to{p.via_at.x, p.via_at.y, s.origin.z};
  if (s.world != nullptr && !walk_clear(*s.world, s.origin, to)) return aim;
  return {p.via_at.x, p.via_at.y, aim.z};
}

// A chain's pickup run through (the boxes meet) or left behind: the via dropped, counted.
void via_check(Pilot& p, const PilotSense& s, float speed) {
  if (p.via_k < 0) return;
  const float dx = s.origin.x - p.via_at.x, dy = s.origin.y - p.via_at.y;
  const float dz = s.origin.z - p.via_at.z;
  const bool touched = dx < kViaTouch && dx > -kViaTouch && dy < kViaTouch && dy > -kViaTouch &&
                       dz > -kViaUnder && dz < kViaOver;
  const float d = across(s.origin, p.via_at);
  if (d < p.via_near) p.via_near = d;
  // Past it: on a link beyond its node, and moving off it, or never as near as the move's step.
  const bool past = p.k > static_cast<size_t>(p.via_k) && d > kViaPast && d > p.via_near + 32;
  if (!touched && !past) return;
  p.via_left = p.route[static_cast<size_t>(p.via_k)];
  p.via_left_at = p.clock_s;
  // Run through the item beside its node: the node is passed with it (the walk runs on from it).
  const bool on_to_it = touched && p.k + 1 == static_cast<size_t>(p.via_k) &&
                        p.k + 2 < p.route.size() && p.mode == PilotMode::walk;
  p.via_k = -1;
  if (on_to_it) advance(p, s);
  if (touched) {
    p.stats.via_touched++;
    p.stats.via_speed += speed;
  } else {
    p.stats.via_missed++;
  }
}

// The walk's aim `target` (the pursuit point `aim` with the lane's offset) pulled back along the
// route until the box reaches it straight from `o`: the look halved, the lane's offset kept while
// it clears and then dropped; unchanged when nothing nearer clears either (a door shut ahead is
// the mover's wait to handle).
Vec3f aim_clear(const Pilot& p, const MoveWorld& world, Vec3f o, Vec3f at, Vec3f aim,
                Vec3f target) {
  if (walk_clear(world, o, target)) return target;
  const Vec3f lane = target - aim;
  for (int div = 2; p.set.look / static_cast<float>(div) >= 8; div *= 2) {
    const Vec3f a = pursue(p, at, p.set.look / static_cast<float>(div));
    if (walk_clear(world, o, a + lane)) return a + lane;
    if (walk_clear(world, o, a)) return a;
  }
  return target;
}

// THE WALK FORESEEN (look_ahead.hpp `foresee_walk`): the way the walk presses now; `w` keeps the
// pursuit's keys or takes another way's.
WalkWay foresee(const Pilot& p, const PilotSense& s, Vec3f aim, Vec3f near, Want& w, ai::Why* why,
                bool& fall) {
  fall = false;
  WalkStart st{s.origin,
               s.velocity,
               s.onground,
               s.waterlevel,
               s.maxspeed,
               {static_cast<float>(p.pressed[0]), static_cast<float>(p.pressed[1]),
                static_cast<float>(p.pressed[2])},
               p.set.lead_s,
               p.set.lead_s + p.set.ahead_s,
               p.set.ahead_bodies ? s.others : std::span<const OtherBox>{},
               &p.g->move};
  st.harm = p.set.walk_falls;
  st.steps = p.set.stair_free;
  WalkKeys keys;
  WalkSeen seen[kWalkWays];
  bool flown = false;
  const WalkWay way = foresee_walk(*s.world, st, aim, near, keys, seen, flown);
  fall = flown && seen[0].falls;
  if (flown) record_walk(why, across({}, s.velocity), way, seen);
  if (way == WalkWay::pursue) return way;
  w.yaw = unwrap(keys.yaw, p.yaw);
  w.forward = keys.forward * facing(w.yaw, p.yaw);
  w.side = keys.side;
  return way;
}

}  // namespace ge::shooter::pilot_parts
