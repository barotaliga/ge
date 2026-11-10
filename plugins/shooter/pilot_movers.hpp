// THE PILOT ON THE MOVERS (pilot.hpp): when a plat is stepped onto (at its rest and silent, waited
// for clear of it till then), whether a player is on its top or under it raised, what keeps him
// at a link's start, and the node a plan starts from. The pilot's own parts (`pilot_parts`): only
// pilot.cpp and pilot_movers.cpp include this.
//
// ge:summary The pilot on the movers: a plat boarded at rest and silent, on its top or under it
// ge:keywords pilot plat mover ride board silent at-rest under raised plan_from mover_in_way
// ge:keywords on_mover_top see_movers stepping-clear legs slab rider client-lag
// ge:skill shooter bots
// ge:test tests/shooter_pilot_test.cpp
#pragma once

#include <cstdint>
#include <span>

#include "shooter/nav.hpp"
#include "shooter/pilot.hpp"

namespace ge::shooter::pilot_parts {

inline constexpr float kStandUp = 8;      // a node is arrived at within this up or down, units
inline constexpr float kStepHeight = 18;  // the move's step

inline const NavNode& node(const NavGraph& g, int32_t n) { return g.nodes[static_cast<size_t>(n)]; }

inline const NavMover* mover_of(std::span<const NavMover> movers, int32_t ent) {
  for (const NavMover& m : movers)
    if (m.ent == ent) return &m;
  return nullptr;
}

inline bool boxes_meet(Vec3f alo, Vec3f ahi, Vec3f blo, Vec3f bhi) {
  return alo.x < bhi.x && alo.y < bhi.y && alo.z < bhi.z && blo.x < ahi.x && blo.y < ahi.y &&
         blo.z < ahi.z;
}

// Each ridden mover's height noted, and when it moved (`Pilot::movers_seen`): once a command.
void see_movers(Pilot& p, const PilotSense& s);
// Whether ridden mover `m` has not moved for a beat (a plat's wait at its rest is silent).
bool silent(const Pilot& p, int32_t m);
// Whether the player stands on mover `ent`'s top as it is now (true for one that is not ridden).
bool on_mover_top(const Pilot& p, const PilotSense& s, int32_t ent);
// Whether the link a -> b's mover keeps the pilot at its start now.
bool mover_in_way(const Pilot& p, const PilotSense& s, const NavLink& l, int32_t a, int32_t b);
// Whether the player is under ridden mover `m`, raised off its rest.
bool under_raised(const Pilot& p, const PilotSense& s, int32_t m);
// The node a plan starts from: the nearest, or the nearest off a ridden mover he is under.
int32_t plan_from(const Pilot& p, const PilotSense& s);

}  // namespace ge::shooter::pilot_parts
