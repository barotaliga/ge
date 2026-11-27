// GEAR ON ARRIVAL: what a player plausibly carries into a campaign map he did not start the game
// on, so a campaign bot run map by map plays each one with the weapons, ammunition and armour a
// person would bring to it, not with the new game's shotgun.
//
// THE RULE: the earlier maps (those a normal playthrough plays before this one, in order, from the
// start of its episode or the new game) are walked one by one. On each, every pickup it places for
// the skill played is taken by the game's own pickup rule (weapons with their ammunition,
// ammunition boxes and packs, armour; never health or powers), as if the player had found
// them all. Then the map's monsters are paid for: their health in damage, each ammunition kind
// worth its weapon's damage a unit (`kQ1Damage`, `kQ2Damage`), every kind spent by the same share
// (`gear_kept_share`); then a further 20% of what was held (`kGearMargin`) for the shots that
// miss. What is left is carried into the next earlier map, and from the last one into this.
// Health is full on arrival. Quake's keys stay behind (its level change takes them); Quake II's
// carry on, less those an earlier map's locks took, as a single player's do.
//
// QUAKE (`q1_take_map_gear`): run on the campaign host's map with the player in it; the pickups
// are touched by the program's own touch functions (so a mission pack's weapons and ammunition
// come in by its own rules), the ammunition fields scaled, and the next map entered by the
// program's own level change (SetChangeParms and the parms), as a real arrival is.
// QUAKE II (`q2_arrival_body`): each earlier map started on a scratch server and its item rows
// taken into a body by `sq_pick_up`; the body is then the player's on the map he arrives on.
//
// ge:summary Gear on arrival: the earlier maps' pickups and keys taken,
// ge:summary less the ammo their monsters cost
// ge:keywords arrival-gear realistic-gear starting-gear loadout carried-gear campaign each-map
// ge:keywords ammo-spent monster-health kept-share earlier-maps spawn-parms q1 q2 mission-pack
// ge:skill shooter bots
// ge:test tests/shooters_edge_arrival_gear_test.cpp
#pragma once

#include <span>
#include <string>
#include <vector>

#include "shooters_edge/server/campaign.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_pack.hpp"

namespace shooters_edge::bots {

// The share of what was held spent besides the monsters' cost: the shots that miss.
constexpr double kGearMargin = 0.2;

// Quake's four ammunition kinds' damage a unit (shells, nails, rockets, cells): a shell's pellets,
// a nail, a rocket's hit and blast, a cell's bolt.
inline constexpr double kQ1Damage[4] = {24, 9, 100, 30};
// Dissolution of Eternity's three more kinds (lava nails, multi rockets, plasma), after the four.
inline constexpr double kQ1PackDamage[3] = {15, 100, 60};
// Quake II's by `SqAmmo` (0 for a kind that is no shot: none, traps, teslas).
inline constexpr double kQ2Damage[server::kSqAmmoKinds] = {0,  48, 8,  120, 100, 20, 125,
                                                           90, 0,  10, 90,  0,   45};

// The share of each kind kept: 1 less the monsters' `health` over the damage `have` holds (each
// kind's count by its `damage` a unit), less `kGearMargin`; 0 at least. With no damage held and
// monsters to kill, nothing is kept.
double gear_kept_share(std::span<const int> have, std::span<const double> damage, double health);

// What one earlier map gave and cost (the run's report).
struct GearLeg {
  std::string map;
  int pickups = 0;            // items taken
  double monster_health = 0;  // its monsters' health
  double capacity = 0;        // the damage the ammunition held could deal before they were paid
  double kept = 1;            // the share of each kind kept
  std::string after;          // the gear after it (`q1_gear_text`, `q2_gear_text`)
};

// QUAKE: on the campaign's map now, the player in `slot` takes every pickup the map placed (its
// program's touch, with him as `other`), then pays for the map's monsters (their health) and the
// margin out of each ammunition kind. Health is set full. Call it before the level change that
// carries the gear on.
GearLeg q1_take_map_gear(server::Campaign& c, int slot);
// The gear the player in `slot` holds now: his weapons by name, his ammunition, his armour.
std::string q1_gear_text(const server::Campaign& c, int slot);

// QUAKE II: the body a player arrives on the map after `before` with (the new game's body, then
// each earlier map's pickups taken and its monsters paid for, at `skill` with `pack`'s rules),
// one leg each in `legs`. False with `error` when a map does not load.
bool q2_arrival_body(server::MapLoader load, void* ctx, server::Q2Pack pack, int skill,
                     const std::vector<std::string>& before, server::SqBody& out,
                     std::vector<GearLeg>& legs, std::string& error);
// The body's gear: its weapons by name, its ammunition, its armour.
std::string q2_gear_text(const server::SqBody& body, server::Q2Pack pack);

// QUAKE II's campaign in the order a playthrough first enters its maps, read off the maps
// themselves: from `first`, each map's level changes followed within its unit (breadth first) and
// a change to a new unit (`*`) taken once the unit is done. Maps the loader lacks are left out.
std::vector<std::string> q2_campaign_order(server::MapLoader load, void* ctx,
                                           const std::string& first);

}  // namespace shooters_edge::bots
