// A MAP'S TEAM PLAN, WORKED OUT FROM THE MAP ITSELF: which areas a team holds, which it rushes and
// where it falls back to, found from the map's shape (the nav graph's proven links and their times)
// and its items, so a map nobody wrote a plan for is played with one.
//
// THE AREAS: the key items (the armours, the mega health, the launcher, the arc, the grenades,
// the quad, the shield) clustered by travel time: two key items within `cluster_s` of each other
// over the nav's links are one area, a power always one alone (it is rushed, not held). Each area
// has:
// - its WORTH a minute: each item's worth to a team (`plan_worth`) over its time back
//   (60 / respawn), summed: what holding the area yields;
// - its REGION: every node within `region_s` of one of its items;
// - its WAYS IN: the links from outside its region into it, counted once per entry spot (the outer
//   ends within 128 units of each other are one way in): the fewer, the easier to hold;
// - its HEIGHT over the map's median floor (holding the high ground);
// - its time from the map's spawn points (the mean of the quickest), and from every other area.
//
// THE PLAN:
// - the BASE: the area worth most for its ways in, `worth / (1 + 0.1 x ways in squared)` (each
//   way in is watched, and the enemy comes in by several at once), raised 10 % for every 64 units
//   of height (to 30 %), the powers' areas left out (a power is rushed, not held), and any area no
//   way leads to from half the spawn points (taken when it can be, never held);
//   the team's own area, held by two, and where a hurt or beaten player falls back and regroups;
// - the FORWARD area: the next by the same measure, held by one;
// - the RUSH: each power's area, gone for by the two nearest, leaving `rush_lead_s` plus the run
//   from the base before it is due;
// - every other area is taken on the way.
//
// A pure function of the graph and the items: the same plan every run.
//
// ge:summary A map's team plan from its shape and items: areas, worth, ways in, base, rush
// ge:keywords map-plan strategy areas control worth ways-in entrances base forward rush quad
// ge:keywords regroup fall-back travel-time dijkstra cluster key-items map-analysis
// ge:skill bots
// ge:test tests/shooters_edge_map_plan_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shooter/locs.hpp"
#include "shooter/nav.hpp"
#include "shooters_edge/bots/fighter.hpp"

namespace shooters_edge::bots {

struct PlanSettings {
  double cluster_s = 2;    // key items this near by the run are one area (a power is always alone)
  double region_s = 2.5;   // an area's region: the nodes this near one of its items
  double rush_lead_s = 3;  // a rush arrives this long before the power is due
};

struct PlanArea {
  std::string name;            // the place name at its middle ("" none)
  std::vector<int32_t> items;  // into the items it was made from
  sh::Vec3f at;                // its items' middle
  double worth = 0;            // a minute
  int32_t ways_in = 0;
  std::vector<int32_t> entries;  // each way in's outer node (the first link's start found for it)
  float height = 0;              // over the map's median floor, units
  double from_spawns_s = 0;      // the mean quickest run from a spawn point
  double hold = 0;               // the base's measure
  bool power = false;            // holds the quad or the shield
  bool reached = true;           // a way leads there from at least half the spawn points
  std::vector<uint8_t> region;   // by node: 1 inside
};

struct MapPlan {
  std::vector<PlanArea> areas;       // by worth, most first
  int32_t base = -1, forward = -1;   // into `areas`
  std::vector<int32_t> rushes;       // the powers' areas
  std::vector<double> between_s;     // areas x areas: the quickest run between their middles
  std::vector<double> rush_leave_s;  // by rush: how long before the power is due to leave the base
  int64_t nodes = 0;
};

// What an item is worth to a team, before its time back: an armour by what it stops, the mega by
// the health it gives over 100, a weapon by how it wins fights, a power by what it does in 30 s.
double plan_worth(const ItemSpot& it);

// The plan for `g` and the map's `items` (a fighter's, after `fighter_init`), from the map's spawn
// points `spawns`, its areas named off `locs` when it has them.
MapPlan make_map_plan(const sh::NavGraph& g, const std::vector<ItemSpot>& items,
                      const std::vector<sh::Vec3f>& spawns, const sh::Locs* locs,
                      const PlanSettings& set = {});

// The area of `plan` the node `n` stands in (-1: none).
int32_t plan_area_of(const MapPlan& plan, int32_t n);

// The plan as a few lines: each area, its items, worth and ways in, then the base, the forward
// area and the rushes.
std::string plan_text(const MapPlan& plan, const std::vector<ItemSpot>& items);

}  // namespace shooters_edge::bots
