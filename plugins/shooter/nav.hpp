// NAVIGATION PROVEN BY THE MOVE: where a player can go on a map, found by asking the player move
// itself. A bot plans on this graph and then plays through the same move a person does, so a link
// exists only where a person's commands would really get there, and a change to the move (a step
// height, the friction, the air) changes the graph the next time it is built.
//
// THE NODES (`build_nav`): the map's floor is sampled on a grid of `spacing` units across the world
// solid's bounds. Down each grid column, from the top, the player's box is traced down
// (`player_trace`); a hit on a plane with normal z >= 0.7 (a floor a player stands on) is a node at
// the trace's end, the box's origin 24 above the feet and 1/32 above the floor, as the move itself
// snaps a standing player. The column is searched again below that floor for the next free space,
// so a room under a ledge gets its own nodes. A node is numbered in column order (x outer, y inner)
// and then from the top down, so the graph is a pure function of the world and the settings.
//
// THE LINKS, in two passes. First from every node to every node of the eight neighbouring columns
// (at most 64 units up). Then to nodes further away but within `reach` and 64 units up, where the
// first pass's walks do not already get there in 1.5 times a straight run's time and a quarter
// second: the target higher than a step and at most 2.5 columns across, or a gap between them (a
// column on the straight line with no floor within 24 units of either end's height, where the
// player's box is free at the higher end's height: a pit, not a wall), tried only between two EDGE
// nodes (those with a neighbouring column that has no floor near their height). Each
// candidate is PROVEN (`prove_link`): a player standing still at the start is given commands of
// `msec` (77 a second at 13 ms), the view turned at the target every command and the forward axis
// pressed to the full, and is moved by `player_move` over the same solids until it stands on the
// ground within `arrive` units across and 2 up or down of the target (reached), or runs out of
// `limit_s`, or falls below the target, or gets no nearer for a quarter second on the ground (in
// the air a player overshoots and comes back), or (a jump) touches the ground anywhere before the
// target: a jump link is one flight (refused). The kinds,
// tried in this order until one reaches (a neighbour: walk, then jump when it is higher than a
// step; anything further: jump):
//   walk   no button; the target within a step's height of the start or below it (a drop when it
//          is more than a step below: it walks off and falls);
//   jump   a jump on the move: the player starts moving toward the target at the full
//          `maxspeed`, then half and a quarter of it (the link keeps the speed that reached: the
//          pilot arrives at it), the jump pressed on the first command; kept only when the floor
//          behind the start has room to run up (`run_up` units of free, level ground straight
//          back: the move reaches full speed from standing in about a tenth of a second, some 16
//          units). A standing jump is not tried: its 30 u/s of air control carries a player less
//          than the 16 units its box needs to get over an edge, so it reaches nothing a moving
//          jump does not.
// A link's TIME is the proof's, in seconds; the graph's search costs links by it.
//
// MOVERS (doors, plats, anything pushed) are left out of the proofs: the caller lists each as an
// entity and its box (`NavMover`), and a node or a link whose swept box meets a mover's box carries
// that entity (`mover`): the way is open only while the mover is out of it, which the bot checks
// at run time. A mover the caller gives a `travel` (a plat) is also RIDDEN: for every node its box
// holds at rest, a node on its top at the far end of the travel (the columns traced again with
// the mover's model placed there), a `ride` link up to it costing the mover's `ride_s`, and links
// off that top node to the nodes beside it, proven with the mover there like any other.
//
// Built once a map, off the hot path (it allocates); `nav_route`, `nav_nearest` and the reads do
// not allocate beyond the route's own vector. Deterministic to the bit: floats in the move's own
// order, the view's yaw through core's libm. Held by tests/shooter_nav_test.cpp and drawn by the
// `se-nav` lab.
//
// ge:summary Navigation proven by the move: floor nodes off the hulls, links proven by player_move
// ge:keywords nav navigation nav-graph navmesh waypoint waypoints node link walk drop jump
// ge:keywords running-jump moving-jump run-up prove prove_link build_nav nav_route nav_nearest
// ge:keywords nav_add_links nav_close_entries teleport-link nav_reach nav_hash map_world
// reachability mover door plat ride route path a-star bot ge:skill shooter bots ge:test
// tests/shooter_nav_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "ai/path.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter/pmove.hpp"

namespace ge::shooter {

struct NavSettings {
  float spacing = 32;    // the grid's columns, units apart
  float reach = 224;     // the furthest a jump link is tried, units across
  float arrive = 8;      // a proof arrives within this across, units
  float limit_s = 2.5f;  // a proof gives up after this long, s
  float run_up = 32;     // a jump needs this much free, level floor behind its start
  int msec = 13;         // a proof's command length, ms
};

// `ride`: stand on a mover's top at rest and be carried to its far end (a plat).
enum class NavKind : uint8_t { walk, drop, jump, ride };
const char* nav_kind_name(NavKind k);

// A mover the proofs leave out: its entity and its box in the world.
struct NavMover {
  int ent = 0;
  Vec3f absmin, absmax;  // its box at rest
  // A mover that carries a player (a plat): its model, where it goes from rest and how long a
  // ride takes (its wait at rest, then the travel), s. Without a model it is only an obstacle.
  const BrushModel* model = nullptr;
  Vec3f travel;
  Vec3f rest;  // where its model stands at rest (a map's plat is built at its top and lowered)
  float ride_s = 0;
};

struct NavNode {
  Vec3f origin;  // the standing player's origin
  int32_t first_link = 0, links = 0;
  int32_t col = 0, row = 0;  // its grid column
  int32_t mover = -1;        // the entity a mover whose box holds this node, or -1
  int32_t ride = -1;         // a node on a ridden mover's top at the far end: that mover, or -1
};

struct NavLink {
  int32_t to = 0;
  NavKind kind = NavKind::walk;
  float time_s = 0;    // the proof's time
  int32_t mover = -1;  // a mover the proof's path crosses, or -1
  float speed = 0;     // a jump's speed toward the target at its start, units/s
};

// How the build went, for the lab.
struct NavCounts {
  int64_t columns = 0, proofs = 0, commands = 0;
  int64_t refused = 0;      // candidates no kind reached
  int64_t no_run_up = 0;    // jumps that reached but had no room to run up
  int64_t by_kind[4] = {};  // links kept, by NavKind
};

struct NavGraph {
  NavSettings set;
  Vec3f lo, hi;  // the grid's corner and far corner (the world's bounds)
  int32_t cols = 0, rows = 0;
  std::vector<NavNode> nodes;
  std::vector<NavLink> links;             // each node's run [first_link, first_link + links)
  std::vector<int32_t> col_first, col_n;  // each grid column's nodes (top first); a ridden
                                          // mover's top nodes come after every column's
  std::vector<NavMover> movers;           // the movers it was built with, at rest
  NavCounts counts;
};

// One proof's result.
struct NavProof {
  bool reached = false;
  float time_s = 0;  // to arrival (or to the refusal)
  int commands = 0;
  Vec3f end;     // where the player stood at the end
  Vec3f lo, hi;  // the player's box swept over the proof
};

// The move's solids of a map alone: solids[0] its world model (hulls 0 and 1, its bounds).
void map_world(const BrushModel& map, MoveWorld& world);

// Moves a player from `from` toward `to` by commands as the header says; `kind` picks the buttons
// and the start (walk and drop: standing, no button; jump: moving at `speed` toward `to`, jump
// pressed first). `path`, when given, gets each command's
// origin.
NavProof prove_link(const MoveWorld& world, const MoveSettings& move, Vec3f from, Vec3f to,
                    NavKind kind, float speed, const NavSettings& set,
                    std::vector<Vec3f>* path = nullptr);

// The graph of `world` (solids[0] the world, its mins and maxs its bounds; any other solid in the
// list is proven against too), the movers left out and tagged.
void build_nav(const MoveWorld& world, std::span<const NavMover> movers, const MoveSettings& move,
               const NavSettings& set, NavGraph& g);

// The node nearest `p` among the columns round it and the ridden movers' top nodes, within 64
// units up or down; -1 when none.
int32_t nav_nearest(const NavGraph& g, Vec3f p);

struct NavRoute {
  std::vector<int32_t> nodes;  // from the start to the goal; empty: no way
  float time_s = 0;            // the links' proof times summed
  size_t expanded = 0;
};

// The quickest way from node `from` to node `to` (A* on the proofs' times, the heuristic the
// straight distance at the fastest speed any link has); links through a mover cost
// `mover_penalty_s` more, and links through a mover in `barred` (a door found shut) are not taken.
NavRoute nav_route(const NavGraph& g, int32_t from, int32_t to, ai::Search& search,
                   float mover_penalty_s = 1, std::span<const int32_t> barred = {});

// Every node a player at node `from` can get to (`search.reached(n)` after; costs the links'
// times), and how many, `from` itself included.
int32_t nav_reach(const NavGraph& g, int32_t from, ai::Search& search);

// A link a map adds that no proof finds (a teleporter: from every node its trigger holds to the
// node at its destination), at `time_s`.
struct NavExtraLink {
  int32_t from = 0, to = 0;
  NavKind kind = NavKind::walk;
  float time_s = 0.1f;
};
// Adds `extra` to each node's run of links (after the proven ones), keeping the runs contiguous.
void nav_add_links(NavGraph& g, std::span<const NavExtraLink> extra);

// Closes the nodes `closed` marks (one byte a node, nonzero closed) to entry from the rest: every
// link from an open node into a closed one is cut, the links out of them and among them kept (a
// pool no route should lead into, left by its own way out). Returns how many links were cut, and
// lists them in `cut_links` when given (the ways in, for a caller that enters on purpose).
int64_t nav_close_entries(NavGraph& g, std::span<const uint8_t> closed,
                          std::vector<NavExtraLink>* cut_links = nullptr);

// The graph's bits: every node's origin and mover (and the mover it rides, where any does), every
// link's target, kind, time, mover and speed.
uint64_t nav_hash(const NavGraph& g);

}  // namespace ge::shooter
