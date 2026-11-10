// The pilot on the movers (pilot_movers.hpp): a plat stepped onto only at its rest and silent, on
// its top or under it raised, what keeps the pilot at a link's start, the node a plan starts from.
//
// The maintainer's rule: stay clear of a plat until it has come down and is silent, and a player
// walking onto one rises a step (its top is not level with the floor). dm2, 12 ten-minute seeds,
// these rules against the old (a plat boarded within 12 units of its rest, its nodes reached at
// any height, plans from the nearest node): speed 182 -> 203 u/s, frags a minute 8.5 -> 10.4,
// waits 382 -> 197 a match, time stood at a mover 7.4 -> 5.0 %; head to head the side with them
// +16 and +19 u/s, +2.4 frags a match.
//
// ge:summary The pilot on the movers: boarding at rest and silent, on the top or under it raised
// ge:keywords pilot plat mover ride board silent under_raised plan_from mover_in_way on_mover_top
// ge:skill shooter bots
// ge:test tests/shooter_pilot_test.cpp
#include "shooter/pilot_movers.hpp"

#include <algorithm>

#include "shooter/world.hpp"

namespace ge::shooter::pilot_parts {

namespace {

constexpr float kRideRest = 12;    // a ridden mover this far off its rest is away, units
constexpr float kBoardRest = 1;    // a plat this near its rest is down, units
constexpr double kSilent_s = 0.3;  // and still this long is silent: stepped onto, s
constexpr float kRideLag = 24;   // a moving plat's top this near a rider's feet carries him, units
constexpr float kStepOver = 18;  // a mover's top this near a player's feet is a step up, units

}  // namespace

// Whether ridden mover `m` has not moved for a beat (a plat's wait at its rest is silent).
bool silent(const Pilot& p, int32_t m) {
  for (const Pilot::MoverSeen& v : p.movers_seen)
    if (v.ent == m) return p.clock_s - v.moved_at >= kSilent_s;
  return true;  // never seen moving
}

// Each ridden mover's height noted, and when it moved.
void see_movers(Pilot& p, const PilotSense& s) {
  for (const NavMover& now : s.movers) {
    const NavMover* rest = mover_of(p.g->movers, now.ent);
    if (rest == nullptr || rest->model == nullptr) continue;
    Pilot::MoverSeen* slot = nullptr;  // its own, else the first free
    for (Pilot::MoverSeen& v : p.movers_seen) {
      if (v.ent == now.ent) {
        slot = &v;
        break;
      }
      if (slot == nullptr && v.ent < 0) slot = &v;
    }
    if (slot == nullptr) continue;
    if (slot->ent == now.ent && slot->z != now.absmin.z) slot->moved_at = p.clock_s;
    slot->ent = now.ent;
    slot->z = now.absmin.z;
  }
}

bool on_mover_top(const Pilot& p, const PilotSense& s, int32_t ent) {
  const NavMover* rest = mover_of(p.g->movers, ent);
  const NavMover* now = mover_of(s.movers, ent);
  if (rest == nullptr || now == nullptr || rest->model == nullptr) return true;
  // A player stepping on rises onto its top (not level with the floor); one under it raised has
  // not. A moving one's height as the client has it lags the rider's by up to a frame's travel.
  const float feet = s.origin.z + kPlayerMins.z;
  const float tol = silent(p, ent) ? kStandUp : kRideLag;
  return feet >= now->absmax.z - tol && feet <= now->absmax.z + tol;
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
    // A ridden mover is stepped onto only at its rest and silent (down, and still a beat: a plat
    // on its way or settling is waited for clear of it, as a person waits for it to come down and
    // stop); stepped off any time, unless it hangs over the player (raised over the shaft he
    // walked into: out from under it first). With a node on it reached only on its top
    // (`reached`) and a plan never started under it (`plan_from`): dm2, 12 ten-minute seeds,
    // speed 182 -> 203 u/s, frags a minute 8.5 -> 10.4, waits 382 -> 197 a match, time stood at
    // a mover 7.4 -> 5.0 %; head to head the side with it +16 and +19 u/s, +2.4 frags a match.
    const float up = now->absmin.z - rest->absmin.z;
    const bool away = up > kRideRest || up < -kRideRest;
    if (nb.mover != l.mover || na.mover == l.mover)
      return away && s.onground && now->absmin.z > s.origin.z + kPlayerMins.z &&
             s.origin.x + kPlayerMaxs.x > now->absmin.x &&
             s.origin.x + kPlayerMins.x < now->absmax.x &&
             s.origin.y + kPlayerMaxs.y > now->absmin.y &&
             s.origin.y + kPlayerMins.y < now->absmax.y;
    if (away || up > kBoardRest || up < -kBoardRest || !silent(p, l.mover)) return true;
    // Its top already more than a step over the feet (rising off its rest under another rider):
    // walked on, the player goes in under it, and standing in its field holds it up for good.
    return s.onground && now->absmax.z > s.origin.z + kPlayerMins.z + kStepHeight;
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

// Whether the player is UNDER ridden mover `m`, away from its rest: his feet within a step of its
// bottom (a raised slab's edge stands on the floor's height) and a step under its top (a rider's
// are at the top, whatever the client's lag). The graph's nodes on a plat stand where its top is
// at rest, so a player on the floor under the raised plat (dm2's plat 206 is a slab on legs) is
// near them and on none of them.
bool under_raised(const Pilot& p, const PilotSense& s, int32_t m) {
  if (m < 0) return false;
  const NavMover* rest = mover_of(p.g->movers, m);
  const NavMover* now = mover_of(s.movers, m);
  if (rest == nullptr || now == nullptr || rest->model == nullptr) return false;
  const float up = now->absmin.z - rest->absmin.z;
  const float feet = s.origin.z + kPlayerMins.z;
  return (up > kRideRest || up < -kRideRest) &&
         feet < std::min(now->absmin.z + kStepHeight, now->absmax.z - kStepHeight);
}

// The node a plan starts from: the nearest, or the nearest off a ridden mover the player is under
// (planned from a node on its top, the walk across it sent him into a leg, out from under it,
// and in again every 0.6 s).
int32_t plan_from(const Pilot& p, const PilotSense& s) {
  const int32_t from = nav_nearest(*p.g, s.origin);
  if (from < 0) return from;
  const NavNode& n = node(*p.g, from);
  const int32_t m = n.mover >= 0 ? n.mover : n.ride;
  if (!under_raised(p, s, m) || s.world == nullptr) return from;
  // The nearest node off it that he can walk straight to and that is no further than a plan
  // keeps him from its first link (`lost`): the nearest by the columns alone was across a wall
  // under dm2's other plat, and planned from there he planned again every other command.
  const NavGraph& g = *p.g;
  int32_t best = -1;
  float best_d = p.set.lost * p.set.lost;
  for (size_t k = 0; k < g.nodes.size(); k++) {
    const NavNode& c = g.nodes[k];
    if (c.mover == m || c.ride == m || c.swim) continue;
    const Vec3f d = c.origin - s.origin;
    if (d.z > kStepHeight || d.z < -kStepHeight) continue;
    const float dd = dot(d, d);
    if (dd >= best_d) continue;
    const HullTrace tr = player_trace(*s.world, s.origin, c.origin);
    if (tr.startsolid || tr.fraction < 1) continue;
    best = static_cast<int32_t>(k);
    best_d = dd;
  }
  return best >= 0 ? best : from;
}

}  // namespace ge::shooter::pilot_parts
