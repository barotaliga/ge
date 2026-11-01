// A TEAM GAME OF BOTS OVER OUR SERVER, RECORDED: two teams of fighters (bots/fighter.hpp, the
// team's plan bots/team.hpp) play the baseline mod with `teamplay 1` on a map file, each fighter at
// the controls of a classic-protocol client (classic_client.hpp's `brain`) talking to our server
// (classic_server.hpp) over the simulated network, so every move goes through the protocol as a
// person's would; the server keeps a multiview recording of the whole game (classic_record.hpp),
// the file a demo player opens (`.mvd`).
//
// THE MAP: any map file (a version 29 or BSP2 map), its nav graph built off its world hull with
// its doors as movers the proofs leave out and its plats as movers ridden (the travel off the
// entity's `height`, else its model's height less 8). The fighters read the map's items off its
// entity text, as a client that loaded the map would.
//
// THE REPORT: each player's team, frags, deaths, shots, pickups and the commands spent in each
// mode and each role, the teams' totals and plans, the frag log, the paths (for a picture), and
// the recording's bytes. The server's clock and the network's are the run's only clocks, so a run
// replays to the bit: `digest` folds every player's origin and health a frame.
//
// ge:summary A recorded team game of bots over our server: fighters on clients, an MVD of it
// ge:keywords team-match run_team_match 4on4 teamplay bots fighters mvd multiview recording
// ge:keywords classic-server simulated-network nav doors plats team-game record
// ge:skill shooter bots net
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "progs/file.hpp"
#include "shooter/aim.hpp"
#include "shooter/vec.hpp"

namespace shooters_edge::net {

namespace sh = ge::shooter;

struct TeamSide {
  TeamSide(std::string n = "red", int c = 4) : name(std::move(n)), colour(c) {}
  std::string name;
  int colour;  // top and bottom
  std::vector<sh::AimSkill> skills = {sh::AimSkill::expert, sh::AimSkill::average,
                                      sh::AimSkill::average, sh::AimSkill::novice};
  std::vector<std::string> names;  // the players' (empty: the team's name and a number)
};

struct TeamMatchOptions {
  std::span<const uint8_t> map;  // the map file
  std::string map_name = "halls";
  std::string entities;   // in place of the map's entity text (empty: the map's own)
  std::string locs;       // the map's place names file's text (empty: named off its items)
  std::string locs_from;  // where it was read from
  const ge::progs::Progs* progs = nullptr;  // the baseline mod, compiled
  TeamSide teams[2] = {TeamSide("red", 4), TeamSide("blue", 13)};
  int frames = 46154;   // server frames of 13 ms (ten minutes)
  int timelimit = 0;    // minutes the mod ends the match at (0: none)
  uint32_t seed = 1;    // the mod's generator; fighter k's hands seed + 31 k
  bool record = true;   // keep the multiview recording
  bool packed = false;  // record it packed (classic_record.hpp: a `.mvdz`)
  int path_every = 8;   // a path point every this many frames
};

struct TeamPlayer {
  std::string name;
  int team = 0;
  int slot = -1;
  sh::AimSkill skill = sh::AimSkill::average;
  float frags = 0;
  int deaths = 0;
  int64_t shots = 0, pickups = 0, switches = 0, packs = 0, held_fire = 0;
  int64_t dives = 0,
          off_graph = 0;       // dives begun into a pocket; thinks spent off the graph or in a pool
  int64_t dives_given_up = 0;  // dives turned back, late or short of air
  double dive_wet_s = 0;       // time in the water on a dive
  int64_t mode_commands[4] = {}, role_commands[4] = {};
  int64_t weapon_commands[9] = {};
  int64_t pilot_modes[9] = {};  // live commands in each pilot mode (shooter/pilot.hpp)
  int64_t plans = 0, stuck = 0, lost = 0, waits = 0, barred = 0;
  float travelled = 0;            // units along the path
  std::string last;               // where he stood at the end, and what he was going for
  std::vector<sh::Vec3f> path;    // every path_every frames
  std::vector<uint8_t> roles;     // beside it: its role, 255 dead
  std::vector<int16_t> known;     // beside it: how many items it knows (recalls) of the map's
  int64_t said = 0;               // team messages it said
  int64_t read = 0, missed = 0;   // its mates' team lines it read, and missed
  int64_t sounds = 0, heard = 0;  // sounds that reached it, and those it took in (hearing.hpp)
  int64_t heard_taken = 0, heard_spawns = 0;  // items heard taken, players heard spawning
  int64_t spam_shots = 0;                     // rockets at the ways into its area
};

struct TeamTotals {
  float frags = 0;
  int deaths = 0;
  int64_t plans = 0, rushes = 0, quads = 0, held_shots = 0, callouts = 0;
  int64_t quad_pickups = 0;                            // the quad taken by a member
  int64_t messages = 0;                                // team messages said
  int64_t lost_seen = 0, retakes = 0, spam_shots = 0;  // areas worked out lost, retaken; spam
};

// A button on the map, as the fighters read it (bots/buttons.hpp).
struct TeamButton {
  sh::Vec3f at;
  bool shoot = false;              // opened by a shot, not a touch
  std::vector<sh::Vec3f> doors;    // the middles of the doors it opens
  std::vector<std::string> opens;  // the items behind them
  sh::Vec3f from;                  // where a player presses (or shoots) it from
};

struct TeamMatchReport {
  bool ok = false;
  std::string error;  // why it did not run, with the next step
  int frames = 0;
  double seconds = 0;  // the server's time at the end
  std::vector<TeamPlayer> players;
  TeamTotals teams[2];
  std::vector<std::string> frag_log;
  std::vector<std::string> chat;     // every team message, "t team name: words", in time order
  std::vector<float> team_frags[2];  // every path_every frames
  std::vector<sh::Vec3f> item_at;    // the items as the fighters know them, for a picture
  std::vector<std::string> item_name;
  std::vector<TeamButton> buttons;      // the map's buttons and what each opens
  std::vector<sh::Vec3f> item_node_at;  // the nav node each item is gone for at
  std::vector<bool> item_routed;        // whether a route leads to it from the map's main part
  sh::Vec3f lo, hi;                     // the nav grid's bounds
  int64_t nav_nodes = 0, nav_links = 0;
  std::vector<sh::Vec3f> nav_points;  // every node's origin (a picture's floor)
  std::vector<uint8_t> nav_reached;   // beside it: reached from the first spawn (1) or not
  int64_t movers = 0, teleport_links = 0;
  int64_t closed_links = 0;                // links into water cut (nav_close_entries)
  int64_t swim_wet = 0, swim_leading = 0;  // the swim field's wet cells, and those with a way out
  int64_t dives = 0;        // pockets only the water reaches, each with a field to dive in by
  double nav_build_ms = 0;  // measured
  int hold_item = -1, quad_item = -1;
  std::string locs_from;                     // where the place names came from
  int64_t locs = 0;                          // how many points they name
  int64_t lines_read = 0, lines_missed = 0;  // team lines the fighters read and missed
  uint64_t digest = 0;
  int64_t faults = 0;
  std::vector<uint8_t> mvd;  // the recording
};

TeamMatchReport run_team_match(const TeamMatchOptions& opt);

// The fixture halls (plugins/brush's sample fixture) furnished for a team game: eight deathmatch
// spawns, the 150 armour in the vault, the quad in the side room, the rocket launcher in the hall,
// a mega health in the corridor, health, shells and rockets about.
extern const char* const kTeamHallsEntities;

}  // namespace shooters_edge::net
