// THE WAY OUT OF THE WATER: a field over a map's water that tells a swimming player which way
// leads to the nearest dry node a route can start from. The nav graph's links prove walking,
// jumping and riding but never swimming, so a pool a player falls or is knocked into (or an
// underwater passage between two rooms) is a place no route leaves. The field fills that gap.
//
// AN EXIT IS PROVEN BY THE MOVE: a wet cell is one step from an exit node only when a swimmer
// starting at its centre and climbing out the way a person does (`swim_out`: the view level, held
// chest deep, forward at the node, so the move's own water jump lifts him over the edge) stands on
// the node within `prove_s`. A node the field chose by distance alone sat behind a wall from water
// under it, and a swimmer pushed at the wall for ever.
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
// ge:keywords swim_wet swim_steps dive swim_out water-jump proven-exit swim_exiting
// ge:keywords nav_add_swim swim-node swim-link water-in-graph
// ge:skill shooter bots
// ge:test tests/shooter_swim_test.cpp
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
  float prove_s = 2.5f;    // a climb out proven within this long, s (0: exits by distance alone)
};

// The command that climbs a swimmer in state `s` out of the water onto `exit` (a dry node's
// origin): the view on it, level while chest deep, pitched up with jump held while the head is
// under, pitched down while only the feet are wet, forward; `msec` long. The proof of an exit and
// a bot's last leg out both use it, so what is proven is what is done.
MoveCmd swim_out(const MoveState& s, Vec3f exit, int msec);

// Fills `f` over the water of `world` inside the graph's bounds; `exits` marks the nav nodes a
// swimmer may leave the water at (its size the graph's node count).
// `same_water`, a field built over the same world and graph, lends its wet cells (another set of
// exits over the same water: a field INTO a pocket only the water reaches).
void build_swim(const MoveWorld& world, const NavGraph& g, std::span<const uint8_t> exits,
                const SwimSettings& set, SwimField& f, const SwimField* same_water = nullptr);

// SWIMMING IN THE GRAPH (`nav_add_swim`): the water made part of the nav, so one route search
// goes through it as through any room. Every wet cell of water (never slime or lava: they harm
// the swimmer) becomes a SWIM NODE at its centre (after every other node, found by its cell:
// NavGraph::swim_node), and `swim` links join:
// - each wet cell to its six neighbours that are wet with the box free half-way (`cell_s` a cell);
// - a floor node standing in water (a pool's floor) and the wet cell it stands in, both ways;
// - a dry node at the water's edge and a wet cell under it within `exit_reach`, both ways, where
//   a climb out from that cell onto the node is proven by the move (`swim_out`, the one a bot
//   swims), tried on at most `exit_tries` cells, the nearest first: out at the proof's time, in
//   (off the edge into the water) at `in_s`;
// - a dry node with no such cell and the water's top under it further down, within `fall_across`:
//   in alone, where a walk off the edge toward it lands the player swimming in it (the move's
//   proof, its time).
// Links walked or dropped into a pool's floor are kept as they were proven.
struct SwimNavSettings {
  float cell = 32;
  float cell_s = 0.25f;    // a cell's swim, s (about 128 units/s: a bot's swim, turns and drag in)
  float floor_s = 0.3f;    // between a pool's floor and the water above it, s
  float in_s = 0.5f;       // off a dry edge into the water, s
  float exit_reach = 112;  // a wet cell this near a dry node (at or under it) may climb out onto it
  float climb = 80;        // and the node no higher above the cell's centre than this
  float prove_s = 2.5f;    // a climb out proven within this long, s
  int exit_tries = 3;      // cells tried for a dry node's climb out
  float fall_across = 96;  // a dry node with no climb out falls into water this near across
};
struct SwimNavCounts {
  int64_t cells = 0, cell_links = 0, floor_links = 0, exits = 0, falls = 0, proofs = 0;
};
// Adds the water of `world` inside the graph's bounds to `g` as above; `wet_floor` marks the floor
// nodes standing in water (one byte a node of `g` as it was). Call once, after every other node.
SwimNavCounts nav_add_swim(const MoveWorld& world, NavGraph& g, std::span<const uint8_t> wet_floor,
                           const SwimNavSettings& set = {});

// Where a swimmer at `p` heads next: true with `to` the next cell's centre (or the exit node's
// origin from an exit cell), false when `p` is in no wet cell that has a way out.
bool swim_next(const SwimField& f, const NavGraph& g, Vec3f p, Vec3f& to);

// Whether the cell holding `p` itself is wet with a way out (no neighbour asked).
bool swim_wet(const SwimField& f, Vec3f p);

// Whether `p` is in an exit cell (its way goes straight to its exit node), and that node.
bool swim_exiting(const SwimField& f, Vec3f p, int32_t& exit_node);

// The steps from `p` to its exit (one more from a neighbouring cell), -1 when no way.
int32_t swim_steps(const SwimField& f, Vec3f p);

}  // namespace ge::shooter
