// A REAL MAP'S NAV FOR THE BOTS: the graph a map's fighters plan on, built off its world model and
// what its entity text says of the rest, one way for every caller (the team game, the co-op party,
// the campaign's run-through) and either map family (clip hulls, or a version 38 map's brushes
// traced through one tracer the nav keeps), off what every rules runner keeps alike (the edicts,
// the model list: `RulesCommon`), so the first game's rules and the sequel's build it the same:
// - a door that starts open (spawnflags 1) is a solid of the nav's world where it stands (a wall
//   drawn aside, a trap's bars); one that is a plate (thin, wide: a BRIDGE that a button slides
//   out) stands where the map drew it, out, and the nodes on it are kept so a caller can shut them
//   while the bridge is still in; every other door (a secret one too) and every plat is a MOVER
//   (shooter/nav.hpp), a plat ridden from its foot to its top;
// - a teleporter is a link from every node its trigger holds to the node at its destination (one
//   another entity switches on, by its name, is left out: nothing says when it works); a trigger
//   brush and its destination lifted 27 units, or the sequel's pad (its touch box 16 units square
//   over the pad's centre) and its destination lifted 10;
// - lava and slime: no way leads into a floor node standing in them (`harm`), nor onto a bridge
//   that moves when touched (a trap over its pit), no swim node is
//   made in them and no bridge or trick lands there; a jump over them or landing at an edge
//   over them is flown again past its target and from a slower run, kept only when both hold,
//   and costs more (`nav_harden_jumps`);
// - water: part of the graph (shooter/swim.hpp `nav_add_swim`): its cells swim nodes, joined to a
//   pool's floor and, by the climb out the move proves, to the dry edges round it; the SWIM FIELD
//   still leads a swimmer off the graph out to the map's main part (the most nodes a start spot
//   reaches; `start_class` names the starts);
// - OUT OF A PIT BY ROCKET (shooter/escape.hpp `nav_add_escapes`): from every liquid at least
//   waist deep, rockets fired down at the surface proven onto the ledges round it (the main part's
//   dry floor, never a bridge), and a link from each liquid node to the cheapest; dm2's lava pits
//   have no other way out;
// - the map's ROCKET-JUMP TRICKS (rocket_tricks.hpp: the jumps people make there again and
//   again), each proven by the move into a rocket link at its level (shooter/nav.hpp
//   `nav_add_tricks`), after the pockets are bridged, landing only where a way back leads out;
//   each trick, and each rocket jump a pocket was bridged by, as a grenade jump too where the
//   move flies one (a player with the grenade launcher and no rocket launcher);
// - the map's GAP TRICKS (gap_tricks.hpp: people's speed jumps over a pit, the strafe jump over
//   dm2's lava), each proven into a jump link at a hopper's speed (shooter/nav_gaps.hpp
//   `nav_add_gap_tricks`) after the risky jumps are hardened, landing where a way back leads.
//
// ge:summary A real map's nav for the bots: movers, teleporters, water closed, the swim field
// ge:keywords map-nav build_map_nav movers doors plats teleporters water swim-field nav-graph
// ge:keywords open-doors-as-floor start-open real-map bridge swim-nodes water-in-graph
// ge:keywords rocket-tricks tricks-in-nav escapes lava-escape gap-tricks speed-jump
// ge:keywords either-family version-38 rules-common teleporter-pad brushes-nav
// ge:skill shooter bots
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ai/path.hpp"
#include "brush/map.hpp"
#include "brush/trace.hpp"
#include "shooter/escape.hpp"
#include "shooter/nav.hpp"
#include "shooter/nav_gaps.hpp"
#include "shooter/swim.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::bots {

struct NavBridge {
  int ent = -1;                // the door's edict
  ge::shooter::Vec3f spawn;    // where it stood when the map began (in: no bridge yet)
  std::vector<int32_t> nodes;  // the nodes standing on it, out
  // Of those, the ones it alone holds up: no floor of the map's own under the player's box there
  // without it (`nodes` takes a box beyond its edge too, so a walkway's nodes beside its root:
  // shut with it, a bot standing there planned on across it; `bridgeseen` takes these).
  std::vector<int32_t> own;
  bool touch = false;  // no name: it moves when touched (a trap: in, under whoever steps on)
  // A FLOOR DOOR (`floor_door`): a plate resting shut as a floor that a button slides away (dm2's
  // big room). Its nodes stand on it at rest (`spawn`) and are shut while it is away from there.
  bool floor = false;
  float wait = 0;  // a bridge's: how long it stays out once moved out, s (<= 0: for good)
};

struct MapNav {
  // A version 38 map's: the map the world's solids trace and the tracer they share (single
  // threaded: the nav and the bots that read it trace on one thread).
  std::shared_ptr<const ge::brush::Map> brushes;
  // The move the nav's walks and jumps are planned with: the first game's unless its caller sets
  // another before `build_map_nav` (the sequel's: `sequel_nav_move`).
  ge::shooter::MoveSettings move;
  std::unique_ptr<ge::brush::BrushTracer> tracer;
  ge::shooter::MoveWorld world;
  ge::shooter::NavGraph graph;
  ge::shooter::SwimField swim;
  std::vector<ge::shooter::NavMover> movers;
  std::vector<uint8_t> wet;             // the nodes standing in water (swim nodes too)
  std::vector<uint8_t> harm;            // the floor nodes in lava or slime: no way leads into them
  ge::shooter::SwimNavCounts swim_nav;  // the water in the graph
  ge::shooter::NavBridgeCounts bridges_made;     // the pockets bridged
  ge::shooter::NavTrickCounts tricks_made;       // the map's tricks proven into blast jumps
  ge::shooter::NavTrickCounts bridged_grenades;  // grenade jumps beside the bridges' rocket jumps
  ge::shooter::NavHardenCounts hardened;         // the jumps over lava or slime
  ge::shooter::NavEscapeCounts escapes_made;     // the rockets out of a pit's liquid
  ge::shooter::NavGapCounts gaps_made;           // the map's gap tricks proven into jump links
  std::vector<uint8_t> trap;  // the nodes no way on foot leads back from to the main part
  std::vector<ge::shooter::NavExtraLink> entries;  // the links into the water the graph closed
  std::vector<NavBridge> bridges;
  // The map's scripted routes to key items (item_routes.hpp), each proven or refused.
  struct ScriptMade {
    std::string name;
    bool held = false;
    float time_s = 0;
    int32_t from = -1, to = -1, mover = -1;
  };
  std::vector<ScriptMade> scripts_made;
  ge::ai::Search main;  // what the map's main part reaches (the most a start spot reaches)
  int32_t main_n = 0;
  int64_t teleport_links = 0, closed_links = 0, harm_links = 0;
  int64_t brink_links = 0;  // walks and drops along a lip over lava or slime (`nav_mark_harm`)
  int64_t side_brink_links = 0;  // walks and drops a side step off into it (`nav_mark_side_harm`)
  double build_ms = 0;      // wall time, never in a digest
};

// The doors and plats of `spec` as movers, off its entity text and the edicts `s` made of them.
// A version 38 map's plats ride on `tracer` (the nav's own; null: none ridden).
std::vector<ge::shooter::NavMover> map_movers(const server::RulesCommon& s,
                                              const server::MapSpec& spec,
                                              ge::brush::BrushTracer* tracer = nullptr);
// The map's nav built into `out` (whose world and graph the movers point into: keep it where it
// is), on either map family: the clip hulls of `spec.models` or the brushes of `spec.brushes`.
// False with `error` when no floor holds a node. `set`: the grid's (a trick map's narrow
// platforms need a finer one than the default 32 units: the `se-trick` lab's --spacing).
bool build_map_nav(const server::RulesCommon& s, const server::MapSpec& spec,
                   const char* start_class, MapNav& out, std::string& error,
                   const ge::shooter::NavSettings& set = ge::shooter::NavSettings{});
// The sequel's move as the nav and the pilot read it: its top speed (300) and friction (6) (its
// stop speed, acceleration and gravity are the first game's already), and its air: with the
// server's air acceleration `air_accelerate` 0 (`sequel_air_accelerate`, its default) a plain
// acceleration of 1 against the whole wish, a jump's flight barely steered; otherwise 10 against
// a wish of 30, the first game's strafing.
ge::shooter::MoveSettings sequel_nav_move(float air_accelerate);

}  // namespace shooters_edge::bots
