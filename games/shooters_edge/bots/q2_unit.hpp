// A UNIT OF QUAKE II'S CAMPAIGN PLAYED THROUGH: a fighter (fighter.hpp) playing Quake II's
// single-player game on its own rules (server/q2_server.hpp) from a map's start, finding each
// map's way out off nothing but its entities (way_through.hpp: the exit, else the button, key or
// trigger that opens the way, else a place not seen yet), until an exit leads out of the unit (a
// `target_changelevel` whose map begins `*`) or the time a map is given runs out. A level change
// within the unit starts the next map on the same rules, as the window does, so a level left and
// entered again is kept (server/q2_save.hpp) and what he carries goes with him.
//
// THE SCRIPT is the plan alone: no route is written down for a map. God mode (`god`) is on by
// default, so the run is about the way through and not about the fight; the damage he took is
// still counted.
//
// THE REPORT, a map a row: whether he got out and where to; the keys on the map and the ones he
// took; the objectives (`target_goal`) and secrets (`target_secret`) the level counts and those
// found; the help messages the rules gave (`target_help`); the monsters and those killed; the
// buttons pressed and the steps of the plan done; his track, a point every half second, for the
// pictures. A digest folds every frame (the time, his origin, view and health), so a run replays
// to the bit.
//
// The maps come from the caller's loader (the player's own packs or a test's fixtures).
//
// ge:summary Quake II's unit played through: a fighter finding each map's way to the unit's end
// ge:keywords q2 unit play-through scripted-playthrough run_q2_unit way-through exit key
// ge:keywords objective target_goal target_help secret target_changelevel god leader
// ge:skill shooter bots
// ge:test tests/shooters_edge_q2_unit_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shooter/aim.hpp"
#include "shooters_edge/bots/way_through.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::bots {

// The map named `name` read into `out`; false with `error` when there is none.
using UnitMapLoader = bool (*)(void* ctx, const std::string& name, server::MapSpec& out,
                               std::string& error);

struct UnitOptions {
  UnitMapLoader load = nullptr;
  void* load_ctx = nullptr;
  std::string first = "base1";
  int maps = 12;             // at most this many maps entered
  double map_seconds = 600;  // a map's time before it is called unfinished, s
  sh::AimSkill skill = sh::AimSkill::expert;
  int game_skill = 1;  // the rules' `skill`
  uint32_t seed = 1;
  bool god = true;
  bool plan = false;  // keep his changes of plan
};

struct UnitMap {
  std::string name, next;  // the map, and the one its exit led to ("" none)
  bool finished = false;
  double seconds = 0;  // on this map, s
  std::string end;     // why it ended (the exit, the time)
  int keys = 0, keys_taken = 0;
  int goals = 0, goals_found = 0, secrets = 0, secrets_found = 0;
  int monsters = 0, killed = 0;
  int helps = 0;  // help messages the rules gave on this map
  int damage = 0;
  WayStats way;
  int64_t shoves = 0;
  int64_t breaks = 0;                  // glass he shot out of his way
  std::vector<std::string> took_keys;  // the keys he took, by their class
  sh::Vec3f lo, hi;                    // the nav's bounds
  std::vector<sh::Vec3f> nodes;        // every node (the picture's floor)
  std::vector<sh::Vec3f> track;        // a point every half second
  std::vector<sh::Vec3f> exits;        // the exits' middles
  std::vector<sh::Vec3f> keys_at;      // every key where it lay
  std::vector<std::string> plan;       // his changes of plan (UnitOptions::plan)
};

struct UnitReport {
  bool ok = false;
  std::string error;
  std::vector<UnitMap> maps;
  bool unit_done = false;  // an exit out of the unit was taken
  std::string out_to;      // and where it led
  uint64_t digest = 0;
};

UnitReport run_q2_unit(const UnitOptions& opt);

}  // namespace shooters_edge::bots
