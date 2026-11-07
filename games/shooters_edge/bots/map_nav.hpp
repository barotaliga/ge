// A REAL MAP'S NAV FOR THE BOTS: the graph a map's fighters plan on, built off its world model and
// what its entity text says of the rest, one way for every caller (the team game, the co-op party,
// the campaign's run-through):
// - a door that starts open (spawnflags 1) is a solid of the nav's world where it stands (a wall
//   drawn aside, a trap's bars); one that is a plate (thin, wide: a BRIDGE that a button slides
//   out) stands where the map drew it, out, and the nodes on it are kept so a caller can shut them
//   while the bridge is still in; every other door (a secret one too) and every plat is a MOVER
//   (shooter/nav.hpp), a plat ridden from its foot to its top;
// - a teleporter is a link from every node its trigger holds to the node at its destination (one
//   another entity switches on, by its name, is left out: nothing says when it works);
// - lava and slime: no way leads into a floor node standing in them (`harm`), no swim node is
//   made in them and no bridge or trick lands there;
// - water: part of the graph (shooter/swim.hpp `nav_add_swim`): its cells swim nodes, joined to a
//   pool's floor and, by the climb out the move proves, to the dry edges round it; the SWIM FIELD
//   still leads a swimmer off the graph out to the map's main part (the most nodes a start spot
//   reaches; `start_class` names the starts);
// - the map's ROCKET-JUMP TRICKS (rocket_tricks.hpp: the jumps people make there again and
//   again), each proven by the move into a rocket link at its level (shooter/nav.hpp
//   `nav_add_tricks`), after the pockets are bridged, landing only where a way back leads out;
//   each trick, and each rocket jump a pocket was bridged by, as a grenade jump too where the
//   move flies one (a player with the grenade launcher and no rocket launcher).
//
// ge:summary A real map's nav for the bots: movers, teleporters, water closed, the swim field
// ge:keywords map-nav build_map_nav movers doors plats teleporters water swim-field nav-graph
// ge:keywords open-doors-as-floor start-open real-map bridge swim-nodes water-in-graph
// ge:keywords rocket-tricks tricks-in-nav
// ge:skill shooter bots
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ai/path.hpp"
#include "shooter/nav.hpp"
#include "shooter/swim.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::bots {

struct NavBridge {
  int ent = -1;                // the door's edict
  ge::shooter::Vec3f spawn;    // where it stood when the map began (in: no bridge yet)
  std::vector<int32_t> nodes;  // the nodes standing on it, out
};

struct MapNav {
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
  std::vector<uint8_t> trap;  // the nodes no way on foot leads back from to the main part
  std::vector<ge::shooter::NavExtraLink> entries;  // the links into the water the graph closed
  std::vector<NavBridge> bridges;
  ge::ai::Search main;  // what the map's main part reaches (the most a start spot reaches)
  int32_t main_n = 0;
  int64_t teleport_links = 0, closed_links = 0, harm_links = 0;
  double build_ms = 0;  // wall time, never in a digest
};

// The doors and plats of `spec` as movers, off its entity text and the edicts `s` made of them.
std::vector<ge::shooter::NavMover> map_movers(const server::QcServer& s,
                                              const server::MapSpec& spec);
// The map's nav built into `out` (whose world and graph the movers point into: keep it where it
// is). False with `error` when no floor holds a node.
bool build_map_nav(const server::QcServer& s, const server::MapSpec& spec, const char* start_class,
                   MapNav& out, std::string& error);

}  // namespace shooters_edge::bots
