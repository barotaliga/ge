// THE PILOT ON THE MOVERS (pilot.hpp): when a plat is stepped onto (at its rest and silent, waited
// for clear of it till then), whether a player is on its top or under it raised, what keeps him
// at a link's start, and the node a plan starts from. The pilot's own parts (`pilot_parts`): only
// pilot.cpp and pilot_movers.cpp include this.
//
// ge:summary The pilot on the movers: a plat boarded at rest and silent, on its top or under it
// ge:keywords pilot plat mover ride board silent at-rest under raised plan_from mover_in_way
// ge:keywords on_mover_top see_movers stepping-clear legs slab rider client-lag
// ge:skill shooterbots bots
// ge:test tests/shooterbots_pilot_test.cpp
#pragma once

#include <cstdint>
#include <span>

#include "shooterbots/nav.hpp"
#include "shooterbots/pilot.hpp"

#include "shooterbots/kit.hpp"

namespace ge::shooterbots::pilot_parts {

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

// Whether a door's leaf, its box now `lo`..`hi`, stands across the way from `a` to `b` (players'
// origins): the way's swept box with its floor a step up, so a door sunk to within a step of the
// floor is walked over. A `leap`'s floor is its take-off's (`a`): a jumper rises from it, so a door
// sunk level with the ledge he jumps down from is under him (e1m4's trap room: its
// door, lowered into the slime pit it is jumped across, held the bot at the ledge for good).
bool leaf_in_way(Vec3f a, Vec3f b, bool leap, Vec3f lo, Vec3f hi);

// Each ridden mover's height noted, and when it moved (`Pilot::movers_seen`): once a command.
void see_movers(Pilot& p, const PilotSense& s);
// Whether ridden mover `m` has not moved for a beat (a plat's wait at its rest is silent).
bool silent(const Pilot& p, int32_t m);
// Whether the player stands on mover `ent`'s top as it is now (true for one that is not ridden).
bool on_mover_top(const Pilot& p, const PilotSense& s, int32_t ent);
// Whether the player stands on ridden mover `ent`'s top, inside its box: aboard it.
bool aboard(const Pilot& p, const PilotSense& s, int32_t ent);
// Whether the link a -> b's mover keeps the pilot at its start now.
// Where moving platform `rest` (the nav's) stands now, `now` its box as the player sees it.
Vec3f platform_at(const NavMover& rest, const NavMover& now);
// Whether a moving platform rises or falls between its stops (more than a step): if so, `lo` and
// `hi` are its box over its whole way, from its lowest stop to its highest.
bool rises(const NavMover& m, Vec3f& lo, Vec3f& hi);
bool mover_in_way(const Pilot& p, const PilotSense& s, const NavLink& l, int32_t a, int32_t b);
// (`under_plat`) The ridden mover (at its rest in the graph) on its way, away from its rest, whose
// box takes the player's place now, or within `margin` of it (units): one coming down on him,
// stepped clear of. Null when none.
const NavMover* over_him(const Pilot& p, const PilotSense& s, float margin);
constexpr float kCrushNear = 48;  // a mover on its way is stepped clear of this far off, units
// Whether the player is under ridden mover `m`, raised off its rest.
bool under_raised(const Pilot& p, const PilotSense& s, int32_t m);
// The node a plan starts from: the nearest, or the nearest off a ridden mover he is under.
int32_t plan_from(const Pilot& p, const PilotSense& s);

}  // namespace ge::shooterbots::pilot_parts
