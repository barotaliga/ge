// A CO-OP EPISODE PLAYED THROUGH: our campaign in coop on a chain of real maps, a fighter leading
// (the way through each map, way_through.hpp: its exit, or the button, key or trigger that opens
// the way; he fights what he meets) and the companions (companion.hpp) beside him, from the first
// map's start until the episode's last exit or the time a map is given runs out. Each map entered
// builds its nav (map_nav.hpp) and makes everyone afresh on it, their inventories carried in their
// spawn parms as the campaign host carries any player's; at a map's end everyone presses the attack
// through the intermission.
//
// THE REPORT, a map a row: whether the leader got out and how long it took; the monsters, killed by
// whom (the one whose sight a monster fell in); the deaths; how near the companions kept (their
// share of the time within kEscortFar, the farthest); the buttons they pressed; what the leader's
// plan went for; and for the pictures the map's nodes, everyone's track and the modes, a point
// every half second. A digest folds every frame (the time, every player's origin, view and health),
// so a run replays to the bit.
//
// A PARTY OF ONE (`skills` empty, `single`): the campaign in single player, the leader alone; with
// `complete` the 100% run (way_through.hpp: every monster, every secret, then the exit) and `god`
// on, the run proves each map can be cleared. The level's own count (the program's
// `total_monsters`, `killed_monsters`, `total_secrets`, `found_secrets`) is the report's, and the
// time to the exit is the time the intermission began. A map not left within `map_seconds` is a
// failure ("the time ran out"), as Quake II's unit calls it.
//
// The maps come from the caller's loader (the player's own packs: none ships in the tree).
//
// ge:summary A co-op episode played through: a leader finding each map's exit, companions beside
// ge:keywords coop-episode run_coop_episode episode play-through e1m1 leader way-through
// ge:keywords companions intermission changelevel parms real-maps
// ge:skill shooter bots
// ge:test tests/shooters_edge_companion_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shooterbots/companion.hpp"
#include "shooterbots/way_through.hpp"
#include "shooters_edge/bots/arsenals.hpp"
#include "shooters_edge/server/campaign.hpp"

namespace shooters_edge::bots {

namespace sb = ge::shooterbots;

struct EpisodeOptions {
  server::MapLoader load = nullptr;  // the maps by name
  void* load_ctx = nullptr;
  std::string first = "e1m1";
  int maps = 8;              // at most this many maps
  double map_seconds = 600;  // a map's time before it is called unfinished, s
  std::vector<sb::AimSkill> skills = {sb::AimSkill::expert};  // one companion each
  sb::AimSkill leader = sb::AimSkill::expert;
  int skill = 1;  // the campaign's `skill`
  uint32_t seed = 1;
  std::string root = ".";                   // the repository's root (the campaign's sources)
  const ge::progs::Progs* progs = nullptr;  // a program of the caller's (none: our campaign)
  bool plan = false;                        // keep the leader's changes of plan
  bool wide_models = false;        // a game's own content: its movers sized as the window does
  bool registered = false;         // the content is the full game
  bool single = false;             // single player (coop 0), the leader alone
  bool complete = false;           // the 100% run (way_through.hpp)
  bool tour = true;                // and its stops taken in the tour's order (tour.hpp)
  bool god = false;                // the leader's `god` on every map
  std::vector<std::string> avoid;  // exits to these maps are not taken (a secret level)
  std::vector<std::string> stop;   // an exit to one of these ends the run (the episode's end)
};

struct EpisodeMap {
  std::string name, next;  // the map, and the one its exit led to ("" none)
  bool finished = false;
  double seconds = 0;
  double exit_s = -1;  // when the intermission began (-1 never)
  // The level's own count at its end (the program's globals; -1 a program without them).
  int total_monsters = -1, killed_monsters = -1, total_secrets = -1, found_secrets = -1;
  std::string end;  // why it ended (the exit, the time, a fault)
  int monsters = 0, killed_by_leader = 0, killed_by_mates = 0, killed_unseen = 0;
  int leader_deaths = 0, mate_deaths = 0;
  double near_share = 0;  // the companions' share of the time within kEscortFar of him
  float farthest = 0;
  int64_t mate_buttons = 0, mate_threats = 0, mate_idle = 0;
  int64_t leader_shoves = 0, mate_shoves = 0;  // commands spent shoving free at a corner
  // Frames a companion was further than kEscortCatchUp from him, by its mode then, and of those
  // the ones where one of them was in the water.
  int64_t far_by_mode[kEscortModes] = {};
  int64_t far_in_water = 0;
  WayStats way;
  int64_t nav_nodes = 0, movers = 0, faults = 0;
  std::string last_fault;  // the program's newest fault on this map, with its stack
  double nav_ms = 0;       // wall time
  sh::Vec3f lo, hi;
  std::vector<sh::Vec3f> nodes;               // every node (the picture's floor)
  std::vector<sh::Vec3f> leader;              // a point every half second
  std::vector<std::vector<sh::Vec3f>> mates;  // each companion's, the same
  std::vector<uint8_t> way_kind;              // the plan's WayKind, every half second
  std::vector<sh::Vec3f> kills;               // where each monster fell
  std::vector<sh::Vec3f> secrets;             // where the leader stood as each secret counted
  std::vector<sh::Vec3f> exits;               // the exits' middles
  std::vector<std::string> plan;              // the leader's changes of plan (EpisodeOptions::plan)
};

struct EpisodeReport {
  bool ok = false;
  std::string error;
  std::vector<EpisodeMap> maps;
  int64_t faults = 0;
  uint64_t digest = 0;
};

EpisodeReport run_coop_episode(const EpisodeOptions& opt);

}  // namespace shooters_edge::bots
