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
// ge:skill shooterbots bots
// ge:test tests/shooterbots_pilot_test.cpp
#include "shooterbots/pilot_movers.hpp"

#include <algorithm>

#include "shooter/world.hpp"
#include "shooterbots/pilot_step_parts.hpp"

namespace ge::shooterbots::pilot_parts {

namespace {

constexpr float kCageFloor = 32;  // feet this near a mover's bottom stand on its floor (a car's is
                                  // 16 thick, the client lags), units
constexpr float kClearIn = 8;     // a player's middle this far inside a mover's box is in it, units
constexpr float kRideRest = 12;   // a ridden mover this far off its rest is away, units
constexpr float kBoardRest = 1;   // a plat this near its rest is down, units
constexpr double kSilent_s = 0.3;  // and still this long is silent: stepped onto, s
constexpr float kRideLag = 24;   // a moving plat's top this near a rider's feet carries him, units
constexpr float kStepOver = 18;  // a mover's top this near a player's feet is a step up, units
constexpr float kOnTop = 16;     // a moving platform's top this far past a spot holds a foot there
constexpr float kAtStop = 24;    // a moving platform this near a stop stands at it, units

}  // namespace

// Where a moving platform's model stands now (its box's shift from where the nav saw it).
Vec3f platform_at(const NavMover& rest, const NavMover& now) {
  return rest.rest + (now.absmin - rest.absmin);
}

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
    if (rest == nullptr || !ridden(*rest)) continue;
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
  if (rest == nullptr || now == nullptr || !ridden(*rest)) return true;
  // A player stepping on rises onto its top (not level with the floor); one under it raised has
  // not. A moving one's height as the client has it lags the rider's by up to a frame's travel.
  const float feet = s.origin.z + kPlayerMins.z;
  const float tol = silent(p, ent) ? kStandUp : kRideLag;
  return feet >= now->absmax.z - tol && feet <= now->absmax.z + tol;
}

bool aboard(const Pilot& p, const PilotSense& s, int32_t ent) {
  const NavMover* rest = mover_of(p.g->movers, ent);
  const NavMover* now = mover_of(s.movers, ent);
  if (rest == nullptr || now == nullptr || !ridden(*rest)) return false;
  // (Not `onground`: a plat starting up lifts its rider off it for a frame.)
  return on_mover_top(p, s, ent) && s.origin.x + kPlayerMaxs.x > now->absmin.x &&
         s.origin.x + kPlayerMins.x < now->absmax.x && s.origin.y + kPlayerMaxs.y > now->absmin.y &&
         s.origin.y + kPlayerMins.y < now->absmax.y;
}

// Whether the link's mover keeps the pilot at its start now. A ride's plat away from its rest
// with a swimmer under its top (`floor_swim`: on a pool's floor under a raised plat, which a plat
// coming down onto him sends back up, so it would never come for him) is in his way too, and with
// `under_plat` one he stands under on dry ground.
// On the map's own floor (the movers left out): a trace a little way down from him meets it.
static bool on_world_floor(const PilotSense& s) {
  if (s.world == nullptr) return false;
  const HullTrace tr = player_trace(*s.world, s.origin, {s.origin.x, s.origin.y, s.origin.z - 4});
  return tr.fraction < 1 && !tr.startsolid;
}

bool rises(const NavMover& m, Vec3f& lo, Vec3f& hi) {
  if (m.stops.empty()) return false;
  lo = m.absmin;
  hi = m.absmax;
  float zlo = m.stops[0].z, zhi = m.stops[0].z;
  for (const Vec3f& at : m.stops) {
    const Vec3f d = at - m.rest;
    lo = {std::min(lo.x, m.absmin.x + d.x), std::min(lo.y, m.absmin.y + d.y),
          std::min(lo.z, m.absmin.z + d.z)};
    hi = {std::max(hi.x, m.absmax.x + d.x), std::max(hi.y, m.absmax.y + d.y),
          std::max(hi.z, m.absmax.z + d.z)};
    zlo = std::min(zlo, at.z);
    zhi = std::max(zhi, at.z);
  }
  return zhi - zlo > kStepHeight;
}

const NavMover* over_him(const Pilot& p, const PilotSense& s, float margin) {
  if (!p.set.under_plat) return nullptr;
  const float feet = s.origin.z + kPlayerMins.z;
  for (const NavMover& rm : p.g->movers) {
    if (!ridden(rm) || moving_platform(rm) || silent(p, rm.ent)) continue;
    const NavMover* nm = mover_of(s.movers, rm.ent);
    if (nm == nullptr) continue;
    const float up = nm->absmin.z - rm.absmin.z;
    if ((up <= kRideRest && up >= -kRideRest) || aboard(p, s, rm.ent)) continue;
    // Standing on its bottom (a car's floor inside its box: base2's lift down to bunk1, ridden
    // in, is no plat coming down on him). Not on the map's own floor at its bottom: a slab on legs
    // (bunk1's sinking floor, raised) has its box's bottom at the floor of the recess under it,
    // where he stood as a car's rider and was pinned when it came down.
    if (feet >= nm->absmin.z && feet <= nm->absmin.z + kCageFloor && !on_world_floor(s)) continue;
    if (s.origin.x + kPlayerMaxs.x > nm->absmin.x - margin &&
        s.origin.x + kPlayerMins.x < nm->absmax.x + margin &&
        s.origin.y + kPlayerMaxs.y > nm->absmin.y - margin &&
        s.origin.y + kPlayerMins.y < nm->absmax.y + margin && nm->absmax.z > feet + kStepHeight &&
        nm->absmin.z < s.origin.z + kPlayerMaxs.z)
      return &rm;
  }
  return nullptr;
}

bool mover_in_way(const Pilot& p, const PilotSense& s, const NavLink& l, int32_t a, int32_t b) {
  // (`under_plat`) Another ridden mover on its way, away from its rest, whose box takes the place
  // the link goes to: waited for out of its reach (bunk1's floor that sinks, called by its button
  // beside the pit: walked into as it came down, along nodes of the pit's rim the mover does not
  // hold, he was pinned under it in its recess and it stalled on him for good).
  if (p.set.under_plat) {
    // One on its way near him now (`over_him`): no link is his until he is clear of it (the next
    // link's place outside it, his way there crossed under it).
    if (p.crushing) return true;
    const NavNode& nb = node(*p.g, b);
    for (const NavMover& rm : p.g->movers) {
      if (!ridden(rm) || moving_platform(rm)) continue;
      const NavMover* nm = mover_of(s.movers, rm.ent);
      if (nm == nullptr) continue;
      const float up = nm->absmin.z - rm.absmin.z;
      if ((up <= kRideRest && up >= -kRideRest) || aboard(p, s, rm.ent)) continue;
      // Already inside it (its middle over him): on out, never held there; held (to step clear)
      // only while it comes down on him.
      if (s.origin.x > nm->absmin.x + kClearIn && s.origin.x < nm->absmax.x - kClearIn &&
          s.origin.y > nm->absmin.y + kClearIn && s.origin.y < nm->absmax.y - kClearIn) {
        if (over_him(p, s, 0) == &rm) return true;
        continue;
      }
      // On its way, a run's length short of it too: at a run he was a step inside before the
      // link that went in was his.
      const float m = silent(p, rm.ent) ? 0 : kCrushNear;
      if (nb.origin.x + kPlayerMaxs.x > nm->absmin.x - m &&
          nb.origin.x + kPlayerMins.x < nm->absmax.x + m &&
          nb.origin.y + kPlayerMaxs.y > nm->absmin.y - m &&
          nb.origin.y + kPlayerMins.y < nm->absmax.y + m &&
          nm->absmax.z > nb.origin.z + kPlayerMins.z + kStepHeight &&
          nm->absmin.z < nb.origin.z + kPlayerMaxs.z)
        return true;
    }
  }
  if (l.mover < 0) return false;
  const NavMover* rest = mover_of(p.g->movers, l.mover);
  const NavMover* now = mover_of(s.movers, l.mover);
  if (rest == nullptr || now == nullptr) return false;
  if (moving_platform(*rest)) {
    // A MOVING PLATFORM: a ride is never waited for (it carries him); onto it, the spot on its top
    // he steps to must be under its top now (it moves on under him); off it, it stands at the stop.
    if (l.kind == NavKind::ride || l.stop < 0) return false;
    const NavNode& nb = node(*p.g, b);
    // Onto it, it waits at the stop of the node stepped to, as a step off it does: at another
    // stop the node's spot may lie under its top too, and the ride planned from it starts from
    // the wrong end (boss1's ferry, boarded at its far stop: he jumped for the ledge from the
    // near end, into the lava).
    if (nb.mover == l.mover && node(*p.g, a).mover != l.mover) {
      const Vec3f d = platform_at(*rest, *now) - rest->stops[static_cast<size_t>(l.stop)];
      if (d.x * d.x + d.y * d.y + d.z * d.z > kAtStop * kAtStop) return true;
    }
    if (nb.mover == l.mover) {
      const Vec3f o = nb.origin;
      return !(o.x - now->absmin.x >= kOnTop && now->absmax.x - o.x >= kOnTop &&
               o.y - now->absmin.y >= kOnTop && now->absmax.y - o.y >= kOnTop);
    }
    const Vec3f d = platform_at(*rest, *now) - rest->stops[static_cast<size_t>(l.stop)];
    return d.x * d.x + d.y * d.y + d.z * d.z > kAtStop * kAtStop;
  }
  if (l.kind == NavKind::ride) {
    const float up = now->absmin.z - rest->absmin.z;
    if (p.set.under_plat && !aboard(p, s, l.mover) && under_raised(p, s, l.mover)) return true;
    return p.set.floor_swim && s.waterlevel >= 2 && (up > kRideRest || up < -kRideRest) &&
           s.origin.z + kPlayerMins.z < now->absmax.z - kStandUp;
  }
  const NavNode& na = node(*p.g, a);
  const NavNode& nb = node(*p.g, b);
  if (ridden(*rest)) {
    // A ridden mover is stepped onto only at its rest and silent (down, and still a beat: a plat
    // on its way or settling is waited for clear of it, as a person waits for it to come down and
    // stop); stepped off any time, unless it hangs over the player (raised over the shaft he
    // walked into: out from under it first). With a node on it reached only on its top
    // (`reached`) and a plan never started under it (`plan_from`): dm2, 12 ten-minute seeds,
    // speed 182 -> 203 u/s, frags a minute 8.5 -> 10.4, waits 382 -> 197 a match, time stood at
    // a mover 7.4 -> 5.0 %; head to head the side with it +16 and +19 u/s, +2.4 frags a match.
    const float up = now->absmin.z - rest->absmin.z;
    const bool away = up > kRideRest || up < -kRideRest;
    // A node it holds where it stands now, away from its rest, is filled by it: bunk1's floor that
    // sinks, half way down its pit, came down onto him there and stalled on him for good.
    if (nb.mover == l.mover && away && !aboard(p, s, l.mover) &&
        now->absmax.z > nb.origin.z + kPlayerMins.z + kStepHeight &&
        now->absmin.z < nb.origin.z + kPlayerMaxs.z)
      return true;
    if (nb.mover != l.mover || na.mover == l.mover)
      return away && s.onground && now->absmin.z > s.origin.z + kPlayerMins.z &&
             s.origin.x + kPlayerMaxs.x > now->absmin.x &&
             s.origin.x + kPlayerMins.x < now->absmax.x &&
             s.origin.y + kPlayerMaxs.y > now->absmin.y &&
             s.origin.y + kPlayerMins.y < now->absmax.y;
    // Already aboard (it starts up under him as he steps on): no longer in his way.
    if (aboard(p, s, l.mover)) return false;
    if (away || up > kBoardRest || up < -kBoardRest || !silent(p, l.mover)) return true;
    // A cage (its floor inside its box: base2's lift, whose box reaches 132 over its floor) is
    // boarded at its floor, where its node was found at rest.
    if (nb.origin.z + kPlayerMins.z < rest->absmax.z - kStepHeight) return false;
    // Its top already more than a step over the feet (rising off its rest under another rider):
    // walked on, the player goes in under it, and standing in its field holds it up for good.
    return s.onground && now->absmax.z > s.origin.z + kPlayerMins.z + kStepHeight;
  }
  // A link inside a hollow door at its rest (`Pilot::hollow`, a lift's car with its nodes on the
  // car's floor: base1's exit, base2's start) was found clear with the car there: walked as
  // found. Out of its rest it is in the way like any door; a shut door's doorway always is.
  bool hollow = false;
  for (const int32_t h : p.hollow) hollow = hollow || h == l.mover;
  if (hollow && na.mover == l.mover && nb.mover == l.mover && now->absmin.x == rest->absmin.x &&
      now->absmin.y == rest->absmin.y && now->absmin.z == rest->absmin.z)
    return false;
  // A door sunk to within a step of the floor (one that opens downward) is walked over.
  return leaf_in_way(na.origin, nb.origin, flies(l.kind), now->absmin, now->absmax);
}

bool leaf_in_way(Vec3f a, Vec3f b, bool leap, Vec3f lo, Vec3f hi) {
  Vec3f wlo = a, whi = a;
  for (int i = 0; i < 3; i++) {
    if (b[i] < wlo[i]) wlo[i] = b[i];
    if (b[i] > whi[i]) whi[i] = b[i];
  }
  if (leap) wlo.z = a.z;  // the take-off's height: a jumper rises from it
  return boxes_meet(wlo + kPlayerMins + Vec3f{0, 0, kStepOver}, whi + kPlayerMaxs, lo, hi);
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
  if (rest == nullptr || now == nullptr || !ridden(*rest)) return false;
  const float up = now->absmin.z - rest->absmin.z;
  const float feet = s.origin.z + kPlayerMins.z;
  // Or inside its box's height (a floor that sinks into the pit he stands in, coming down round
  // him: bunk1's stalled on him there for good).
  const bool inside = now->absmin.z < feet && now->absmax.z > feet + kStepHeight;
  return (up > kRideRest || up < -kRideRest) &&
         (feet < std::min(now->absmin.z + kStepHeight, now->absmax.z - kStepHeight) || inside);
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

}  // namespace ge::shooterbots::pilot_parts
