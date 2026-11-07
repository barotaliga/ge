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
// target (refused: one flight; a standing jump may land 24 units short and step on). The kinds,
// tried in this order until one reaches (a neighbour: walk, then jump when it is higher than a
// step; anything further: jump):
//   walk   no button; the target within a step's height of the start or below it (a drop when it
//          is more than a step below: it walks off and falls);
//   jump   a jump on the move: the player starts moving toward the target at the full
//          `maxspeed`, then half and a quarter of it (the link keeps the speed that reached: the
//          pilot arrives at it), the jump pressed on the first command; kept only when the floor
//          behind the start has room to run up (`run_up` units of free, level ground straight
//          back: the move reaches full speed from standing in about a tenth of a second, some 16
//          units); last, onto a neighbour, a STANDING jump (speed 0, no run-up): stairs too
//          high to climb and too shallow to run up on, hopped against the riser as people do.
// A link's TIME is the proof's, in seconds; the graph's search costs links by it.
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
// ge:keywords reachability mover door plat ride route path a-star bot swim swim-node water
// ge:keywords rocket-jump flip back-rocket-jump rocket_burst rocket_blast turn-room tricks
// ge:keywords nav_add_tricks rocket-jump-level nav-jumper jump-level grenade-jump
// ge:keywords prove_grenade_jump grenade_at grenade-fuse blast-jump
// ge:skill shooter bots
// ge:test tests/shooter_nav_test.cpp
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

// `ride`: stand on a mover's top at rest and be carried to its far end (a plat). `swim`: through
// the water, between two of its cells, from a pool's floor up into it or down out of it, into it
// off a dry edge, or out of it onto a dry node by the climb the move proved (shooter/swim.hpp
// `nav_add_swim`).
// `rocket`: a ROCKET JUMP (`prove_rocket_jump`): only for a player with the rocket launcher, its
// rockets and the health for the blast; left out of routes and reach unless asked for.
// `grenade`: a GRENADE JUMP (`prove_grenade_jump`): the same, for a player with the grenade
// launcher (a slow way up: the grenade's fuse is waited out).
enum class NavKind : uint8_t { walk, drop, jump, ride, swim, rocket, grenade };
inline constexpr int kNavKinds = 7;
const char* nav_kind_name(NavKind k);

// MOVERS (doors, plats, anything pushed) are left out of the proofs: the caller lists each as an
// entity and its box (`NavMover`), and a node or a link whose swept box meets a mover's box carries
// that entity (`mover`): the way is open only while the mover is out of it, which the bot checks
// at run time. A mover the caller gives a `travel` (a plat) is also RIDDEN: for every node its box
// holds at rest, a node on its top at the far end of the travel (the columns traced again with
// the mover's model placed there), a `ride` link up to it costing the mover's `ride_s`, and links
// off that top node to the nodes beside it, proven with the mover there like any other.
//
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
  bool swim = false;         // a swim node: a wet cell's centre (no floor under it)
  bool wet = false;          // in water: a swim node, or a floor node under water (a pool's floor)
};

struct NavLink {
  int32_t to = 0;
  NavKind kind = NavKind::walk;
  float time_s = 0;    // the proof's time
  int32_t mover = -1;  // a mover the proof's path crosses, or -1
  float speed = 0;     // a jump's speed toward the target at its start, units/s
  bool back = false;   // a rocket jump played as the flip (`RocketJumpSpec::back`)
  float pitch = 0;     // a rocket jump's shot, degrees down (0: the player's own)
  uint8_t level = 0;   // a rocket jump's: the least jumper's level that takes it (`NavJumper`)
  float lead = 0;      // a grenade jump's: the jumper stands this far past the grenade, units
};

// A blast jump: a rocket jump or a grenade jump.
inline bool nav_blast(NavKind k) { return k == NavKind::rocket || k == NavKind::grenade; }

// WHO TAKES A BLAST JUMP on a route or a reach: nobody, by default. Rocket jumps with `rockets`,
// grenade jumps with `grenades`; a jumper of `level` (0 amateur, 1 intermediate, 2 expert) takes
// none above his level, and each costs him `extra_s` more than its link's time (the health and
// the ammunition, weighed by how sure his hands are: the less sure, the more a jump must save
// before he tries it).
struct NavJumper {
  bool rockets = false;
  bool grenades = false;
  uint8_t level = 2;
  float extra_s = 0;
  // Whether he takes `l` at all (any link but a blast jump: always).
  bool takes(const NavLink& l) const {
    if (!nav_blast(l.kind)) return true;
    return (l.kind == NavKind::rocket ? rockets : grenades) && l.level <= level;
  }
  // What `l` costs him, s.
  float cost_s(const NavLink& l) const { return nav_blast(l.kind) ? l.time_s + extra_s : l.time_s; }
};

// How the build went, for the lab.
struct NavCounts {
  int64_t columns = 0, proofs = 0, commands = 0;
  int64_t refused = 0;              // candidates no kind reached
  int64_t no_run_up = 0;            // jumps that reached but had no room to run up
  int64_t by_kind[kNavKinds] = {};  // links kept, by NavKind
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
  // The swim nodes (shooter/swim.hpp `nav_add_swim`), after every other node: from `swim_first`
  // on, each a wet cell of the cube grid at `swim_lo`, `swim_cell` a side, found by its cell.
  int32_t swim_first = -1;
  Vec3f swim_lo;
  float swim_cell = 0;
  int32_t swim_nx = 0, swim_ny = 0, swim_nz = 0;
  std::vector<int32_t> swim_node;  // each cell's swim node, -1 none
  // Each node's ROOM (`nav_room`): grid steps to the nearest edge of its floor; empty until asked.
  std::vector<uint8_t> room;
};

// One proof's result.
struct NavProof {
  bool reached = false;
  bool landed = false;  // a blast jump's: blown into the air and down again, standing (anywhere)
  float time_s = 0;     // to arrival (or to the refusal)
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

// The node nearest `p` among the columns round it, the ridden movers' top nodes and the swim node
// of the cell `p` is in, within 64 units up or down; -1 when none.
int32_t nav_nearest(const NavGraph& g, Vec3f p);

// A link a map adds that no proof finds (a teleporter: from every node its trigger holds to the
// node at its destination), at `time_s`.
struct NavExtraLink {
  int32_t from = 0, to = 0;
  NavKind kind = NavKind::walk;
  float time_s = 0.1f;
  float speed = 0;    // a jump's speed at its start, units/s (NavLink::speed)
  bool back = false;  // a rocket jump's flip (NavLink::back)
  float pitch = 0;    // and its shot (NavLink::pitch)
  uint8_t level = 0;  // and its level (NavLink::level)
  float lead = 0;     // a grenade jump's stand past the grenade (NavLink::lead)
};

// THE ROCKET JUMP, as the baseline mod makes it (games/shooters_edge/qc: weapons.qc's `launch`,
// combat.qc's `radius_damage` and `blow_push`): the player runs at the target at `speed`, and on
// one command presses jump and fires with the view `pitch` degrees down; the rocket starts at
// the eye less 8 (and 8 down), flies at `missile_speed` and bursts `back_off` short of where it
// meets the world; the blast takes `damage` less half the distance to the player's middle, half
// of that to its own shooter, and pushes him `push` units/s a point away from the burst. From
// there the view is on the target and forward held through the flight, as in a jump.
// THE FLIP (`back`): the skilled rocket jump. The player runs at the target and turns into the
// jump, strafing through the turn (`flip_keys`), so he leaves the floor at speed with his back to
// it; he fires `back_pitch` down behind him with the jump and turns the view back to the target
// in flight at `turn` degrees a second, strafing round the other way. The burst behind pushes him
// forward as much as up: a flatter, faster flight than the shot at the feet, under a lower ceiling,
// and the pitch places it (steeper: higher and shorter), so a skilled player lands it on the spot
// he chose.
struct RocketJumpSpec {
  float pitch = 80;  // degrees down at the shot (straight down leaves no wish across the floor)
  float missile_speed = 1000, back_off = 8;
  float damage = 120, self_share = 0.5f, push = 8;
  float eye = 22;
  bool back = false;
  float back_pitch = 55;  // degrees down at the shot behind
  float turn = 720;       // the view's turn back in flight, degrees a second
  // The free floor its turn into the jump needs beyond the run-up at the full speed, units (a
  // human hand's turn round, 0.4 s, at 320; less in proportion slower).
  float turn_room = 128;
};
// The keys a player holds turning round on a run toward heading `way` with the view at `view`
// (degrees): of the eight a hand presses (forward, back, a side, or a side with either), the one
// nearest the way in the view's frame; `forward` and `side` -1, 0 or 1 (side right positive).
void flip_keys(float way, float view, float& forward, float& side);

// The rocket a player standing at `origin` fires along the view `angles` (pitch down, yaw): where
// it bursts and how long it flies first, s; `ok` false when it meets nothing within 4096.
struct RocketBurst {
  bool ok = false;
  Vec3f at;
  float flight_s = 0;
};
RocketBurst rocket_burst(const MoveWorld& world, Vec3f origin, Vec3f angles,
                         const RocketJumpSpec& rj = {});
// The blast at `at` on its own shooter `s`: the push away from it, off the ground; returns the
// points it takes from him, 0 when he stands too far for any.
float rocket_blast(MoveState& s, Vec3f at, const RocketJumpSpec& rj = {});

// The rocket jump from `from` toward `to` run on the move with the blast's push; reached as a
// jump is (standing on the target, or landed anywhere when the caller reads `end`). `health`, when
// given, gets the blast's points to the jumper.
NavProof prove_rocket_jump(const MoveWorld& world, const MoveSettings& move, Vec3f from, Vec3f to,
                           float speed, const NavSettings& set, const RocketJumpSpec& rj = {},
                           float* health = nullptr, std::vector<Vec3f>* path = nullptr);

// THE GRENADE JUMP, as the baseline mod throws a grenade (weapons.qc's `fire_grenade`): from the
// eye less 8 (and 8 down) along the view at `speed`, `up` more upward, falling at `gravity`,
// bouncing off the world (its speed into the surface turned back by `bounce` less one), at rest
// on a floor once a bounce leaves it rising slower than 60, and bursting `fuse` seconds after the
// shot with a rocket's blast (`blast`: its damage, share and push). The player stands still at
// the start and fires `pitch` down toward the target; the grenade rolls to rest ahead of him; he
// walks to `lead` units past it toward the target (its owner passes through it: the blast behind
// throws him forward as well as up), presses jump `jump_before_s` before the fuse and holds
// forward at the target through the flight.
struct GrenadeJumpSpec {
  float pitch = 80;
  float speed = 600, up = 200, gravity = 800, bounce = 1.5f, fuse = 2.5f;
  float jump_before_s = 0.1f;
  RocketJumpSpec blast;  // the burst's damage, self share and push, and the eye's height
};
// Where a grenade fired by a player standing at `origin` along the view `angles` is `t` seconds
// after the shot (its flight run in `msec` steps against the world).
Vec3f grenade_at(const MoveWorld& world, Vec3f origin, Vec3f angles, float t,
                 const GrenadeJumpSpec& gj = {}, int msec = 13);
// The grenade jump from `from` toward `to`, standing `lead` past the grenade, run on the move with
// the blast's push; reached as a rocket jump is. `health`, when given, gets the blast's points.
NavProof prove_grenade_jump(const MoveWorld& world, const MoveSettings& move, Vec3f from, Vec3f to,
                            float lead, const NavSettings& set, const GrenadeJumpSpec& gj = {},
                            float* health = nullptr, std::vector<Vec3f>* path = nullptr);

struct NavRoute {
  std::vector<int32_t> nodes;  // from the start to the goal; empty: no way
  float time_s = 0;            // the links' proof times summed
  size_t expanded = 0;
};

// The quickest way from node `from` to node `to` (A* on the proofs' times, the heuristic the
// straight distance at the fastest speed any link has); links through a mover cost
// `mover_penalty_s` more, links through a mover in `barred` (a door found shut) are not taken, and
// nor is any link in `cut` (its `from` and `to`: a way an open door's leaf still stands across);
// rocket jumps as `jumper` takes them.
NavRoute nav_route(const NavGraph& g, int32_t from, int32_t to, ai::Search& search,
                   float mover_penalty_s = 1, std::span<const int32_t> barred = {},
                   std::span<const NavExtraLink> cut = {}, const NavJumper& jumper = {});

// Every node a player at node `from` can get to (`search.reached(n)` after; costs the links'
// times), and how many, `from` itself included; rocket jumps as `jumper` takes them.
int32_t nav_reach(const NavGraph& g, int32_t from, ai::Search& search,
                  const NavJumper& jumper = {});

// Adds `extra` to each node's run of links (after the proven ones), keeping the runs contiguous.
void nav_add_links(NavGraph& g, std::span<const NavExtraLink> extra);

// Closes the nodes `closed` marks (one byte a node, nonzero closed) to entry from the rest: every
// link from an open node into a closed one is cut, the links out of them and among them kept (a
// pool no route should lead into, left by its own way out). Returns how many links were cut, and
// lists them in `cut_links` when given (the ways in, for a caller that enters on purpose).
int64_t nav_close_entries(NavGraph& g, std::span<const uint8_t> closed,
                          std::vector<NavExtraLink>* cut_links = nullptr);

// REPAIRING POCKETS (`nav_bridge_pockets`): after the build (and the water, the teleporters),
// a POCKET is a set of nodes, joined by links among themselves, that the map's main part (the
// nodes `main` marks) does not reach. For each pocket the links the build never tries are tried,
// proven by the move as any other: from a main node to a pocket node (in) and back (out), the two
// within `far` across, the target at most `up` higher and at most `down` lower, the nearest pairs
// first, at most `tries` of them a way: a walk or a drop, then a running jump at the full speed,
// three quarters and a half, then a standing jump. A jump that lands standing on another node of
// the pocket (or of the main part, going out) than the one aimed at is kept as a link to that
// node: a long jump rarely lands on a grid point. The first way that holds for a pocket is kept;
// `main` is grown by what the pocket's nodes reach, so a pocket beyond it is tried next.
struct NavBridgeSettings {
  float far = 384;   // the most across a link tried, units
  float up = 60;     // the target at most this much higher, units
  float down = 640;  // and at most this much lower, units
  int tries = 32;    // pairs proven a pocket and way
  int least = 1;     // pockets smaller than this many nodes are left alone
  // Last, ROCKET JUMPS (`prove_rocket_jump`) from a running start at the full speed and half and
  // from standing, to a target at most `rocket_up` higher, on at most `rocket_tries` pairs; only
  // once no other way adds a pocket, and only into a pocket with a way back out of it. The FLIP
  // is tried first, at each of `flip_pitches` (the skilled jump, placed by its pitch) from the
  // full speed on the nearest `rocket_tries` pairs with the floor behind for its run and turn,
  // then from half of it, then standing; then the shot at the feet on the nearest pairs. The
  // first that lands on its target, on the way to a wanted node, is kept; failing that, the best
  // landing (leading to a wanted node first, then on target).
  bool rockets = true;
  float rocket_up = 280;
  int rocket_tries = 24;
  float rocket_cost_s = 3;  // a rocket jump's link costs this more (the rocket and the health)
  float flip_pitches[5] = {55, 65, 45, 75, 35};
  RocketJumpSpec rj;
  // The nodes worth a rocket jump (one byte a node: an item's); empty: any pocket. With them, a
  // landing from which one of its pocket's is reached on foot is taken before any other.
  std::span<const uint8_t> wanted;
  // The nodes no way may lead into (one byte a node: a floor under lava or slime); empty: none.
  // They are in no pocket and no link lands on them.
  std::span<const uint8_t> avoid;
};
struct NavBridgeCounts {
  int64_t pockets = 0, bridged_in = 0, bridged_out = 0, by_rocket = 0, by_flip = 0, proofs = 0;
};
NavBridgeCounts nav_bridge_pockets(const MoveWorld& world, const MoveSettings& move, NavGraph& g,
                                   std::vector<uint8_t>& main, const NavBridgeSettings& set = {});

// THE TRICKS (`nav_add_tricks`): rocket jumps people make on a map again and again, read off
// their recorded games (the `se-rj` lab's tricks; a game keeps each map's as data), each proven by
// the move as any rocket jump: from the floor nodes within `near` of where people took off (the
// nearest `takeoffs` of them), as the flip when people flipped it, at their pitch and 8 and 16
// degrees either side, from the run they had (no faster than the move's maxspeed, with the floor
// behind to run it), three quarters of it and standing. The first proof that lands standing on a
// node within `land` across (and 32 up or down) of where people landed, one from which a way leads
// back on foot to the main part (`back`, one byte a node; empty: any), is kept as a rocket link at
// the trick's level, costing its flight and `cost_s`. With `grenades`, a GRENADE JUMP is proven
// for each too (`prove_grenade_jump`, from the same takeoffs, standing each of `leads` past the
// grenade) and kept the same way at the first level (a standing jump timed by the fuse needs no
// skilled hands): the way up for a player with the grenade launcher and no rocket launcher.
struct NavTrick {
  Vec3f from, to;     // where people took off and landed (their origins)
  bool back = false;  // flipped (`RocketJumpSpec::back`)
  float pitch = 45;   // the shot, degrees down
  float run = 320;    // the run at the shot, units/s (0: standing)
  uint8_t level = 0;  // the least level that makes it (`NavLink::level`)
};
struct NavTrickSettings {
  float near = 64;   // a takeoff node within this of people's takeoff, units
  int takeoffs = 4;  // the nearest this many tried
  float land = 64;   // a landing within this across of people's, units
  float cost_s = 0;  // the link costs its flight and this (a jumper's own price is `NavJumper`)
  RocketJumpSpec rj;
  bool rockets = true, grenades = false;
  float leads[4] = {16, 32, 0, 48};
  GrenadeJumpSpec gj;
};
struct NavTrickCounts {
  int64_t held = 0, flips = 0, grenades = 0, proofs = 0;
  // Each trick's link start, -1 where no proof landed: its rocket jump's and its grenade jump's.
  std::vector<int32_t> rocket_from, grenade_from;
};
NavTrickCounts nav_add_tricks(const MoveWorld& world, const MoveSettings& move, NavGraph& g,
                              std::span<const NavTrick> tricks, std::span<const uint8_t> back,
                              const NavTrickSettings& set = {});

// RISKY JUMPS (`nav_harden_jumps`): a jump over a floor that harms (lava, slime: the nodes `harm`
// marks, one byte a node, in a grid column under the jump's line and below both its ends), or
// landing within `beside` of one lower than its target (an edge over it: the jumper's momentum
// carries him on past where the proof stood), is one a miss of kills. Each jump of them is flown
// again at a point `overshoot` past its target (the momentum a landing keeps): one whose flight
// ends in the harm or well below its target is removed; each running jump left is proven again
// from `slow` of its speed (late hands arrive slower than the proof ran; 0: none is kept), removed
// when it no longer lands; every one left (blast jumps too) costs `risk_s` more.
struct NavHardenCounts {
  int64_t over = 0, removed = 0, proofs = 0;
};
NavHardenCounts nav_harden_jumps(const MoveWorld& world, const MoveSettings& move, NavGraph& g,
                                 std::span<const uint8_t> harm, float slow = 0.9f, float risk_s = 2,
                                 float beside = 192, float overshoot = 48);

// The nodes from which a way leads, on foot (or swimming, riding, teleported: anything but a
// rocket jump), to a node `mark` marks; one byte a node.
std::vector<uint8_t> nav_back(const NavGraph& g, const std::vector<uint8_t>& mark);

// THE ROOM a node has: how many grid steps it stands from the nearest edge of its floor (an edge
// node has fewer than eight walk links to the floor nodes of its neighbouring columns: a wall, a
// drop, water beside it), at most `most`; a walker may stray `room x spacing` off the line between
// two such nodes and stay on the floor. Swim nodes and ridden tops have none. Fills `g.room`; not
// in `nav_hash` (a read of the graph, not part of it).
void nav_room(NavGraph& g, int most = 3);

// The graph's bits: every node's origin and mover (and the mover it rides, where any does), every
// link's target, kind, time, mover and speed.
uint64_t nav_hash(const NavGraph& g);

}  // namespace ge::shooter
