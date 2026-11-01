// THE WAY OUT OF THE WATER: a field over a map's water that tells a swimming player which way
// leads to the nearest dry node a route can start from. The nav graph's links prove walking,
// jumping and riding but never swimming, so a pool a player falls or is knocked into (or an
// underwater passage between two rooms) is a place no route leaves. The field fills that gap.
//
// The water is sampled on a cube grid (`cell` units a side, at the nav grid's corner): a cell is
// WET when the player's box is free at its centre and the centre is water, slime or lava. The
// way out is a breadth-first search over the wet cells (six neighbours, both centres wet and the
// box free half-way between) seeded from every wet cell within `exit_reach` of an EXIT: a dry
// node the caller marks (one byte a node: the nodes a route reaches from the map's main part).
// Each wet cell keeps the next cell on its way out and the exit it ends at; `swim_next` answers
// for a point: the cell's next cell's centre, or the exit node's origin at the end.
//
// Built once a map (it allocates); `swim_next` does not. Deterministic: integer order throughout
// and the move's own contents queries. Used by the Shooter's Edge fighter (bots/fighter.cpp) and
// drawn by the `se-team` lab.
//
// ge:summary The way out of a map's water: a breadth-first field over wet cells to the exits
// ge:keywords swim water pool underwater escape flow-field wet-cell exit swim_field swim_next
// swim_wet swim_steps dive ge:skill shooter bots ge:test tests/shooter_swim_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "shooter/nav.hpp"
#include "shooter/world.hpp"

namespace ge::shooter {

struct SwimField {
  Vec3f lo;         // the grid's corner
  float cell = 32;  // a cell's side, units
  int32_t nx = 0, ny = 0, nz = 0;
  std::vector<int32_t> next;       // each cell's next cell on its way out; -1 dry or no way out;
                                   // -2 an exit cell (its way goes to `exit_node`)
  std::vector<int32_t> exit_node;  // each wet cell's exit (a nav node), -1 none
  std::vector<uint16_t> steps;     // each wet cell's steps to its exit
  std::vector<uint8_t> wet_cells;  // 1 where the cell is wet
  int64_t wet = 0, leading = 0;    // wet cells, and how many of them have a way out
};

struct SwimSettings {
  float cell = 32;
  float exit_reach = 112;  // a wet cell this near an exit node (at or under it) is one step from it
  float climb = 80;        // and the exit node no higher above the cell's centre than this (a ledge
                           // a swimmer climbs out onto from the surface)
};

// Fills `f` over the water of `world` inside the graph's bounds; `exits` marks the nav nodes a
// swimmer may leave the water at (its size the graph's node count).
// `same_water`, a field built over the same world and graph, lends its wet cells (another set of
// exits over the same water: a field INTO a pocket only the water reaches).
void build_swim(const MoveWorld& world, const NavGraph& g, std::span<const uint8_t> exits,
                const SwimSettings& set, SwimField& f, const SwimField* same_water = nullptr);

// Where a swimmer at `p` heads next: true with `to` the next cell's centre (or the exit node's
// origin from an exit cell), false when `p` is in no wet cell that has a way out.
bool swim_next(const SwimField& f, const NavGraph& g, Vec3f p, Vec3f& to);

// Whether the cell holding `p` itself is wet with a way out (no neighbour asked).
bool swim_wet(const SwimField& f, Vec3f p);

// The steps from `p` to its exit (one more from a neighbouring cell), -1 when no way.
int32_t swim_steps(const SwimField& f, Vec3f p);

}  // namespace ge::shooter
