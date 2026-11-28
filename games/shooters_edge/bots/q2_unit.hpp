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
// buttons pressed and the steps of the plan done; his track, a point every 0.2 s, for the
// pictures. A digest folds every frame (the time, his origin, view and health), so a run replays
// to the bit.
//
// The maps come from the caller's loader (the player's own packs or a test's fixtures).
//
// ge:summary Quake II's unit played through: a fighter finding each map's way to the unit's end
// ge:keywords q2 unit play-through scripted-playthrough run_q2_unit way-through exit key
// ge:keywords objective target_goal target_help secret target_changelevel god leader party partners
// ge:skill shooter bots
// ge:test tests/shooters_edge_q2_unit_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shooterbots/aim.hpp"
#include "shooterbots/party.hpp"
#include "shooterbots/way_through.hpp"
#include "shooters_edge/bots/arrival_gear.hpp"
#include "shooters_edge/bots/arsenals.hpp"
#include "shooters_edge/server/q2_pack.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::bots {

namespace sb = ge::shooterbots;

// The map named `name` read into `out`; false with `error` when there is none.
using UnitMapLoader = bool (*)(void* ctx, const std::string& name, server::MapSpec& out,
                               std::string& error);

struct UnitOptions {
  UnitMapLoader load = nullptr;
  void* load_ctx = nullptr;
  std::string first = "base1";
  server::Q2Pack pack = server::Q2Pack::base;  // the rules played: the base game's or a pack's
  int maps = 12;                               // at most this many maps entered
  double map_seconds = 600;                    // a map's time before it is called unfinished, s
  sb::AimSkill skill = sb::AimSkill::expert;
  int game_skill = 1;  // the rules' `skill`
  uint32_t seed = 1;
  bool god = true;
  bool plan = false;  // keep his changes of plan
  // The 100% run (way_through.hpp): every monster he can reach and every secret before the exit;
  // the last quarter of `map_seconds` is for the way out.
  bool complete = false;
  // THE PARTY (plugins/shooterbots/party.hpp): partners who run the way through beside him in
  // co-op (the rules' `coop` 1), one each, the map's work split among them (nearest takes it) and
  // the exit waiting for all of it.
  std::vector<sb::AimSkill> partners;
  // (`partners`) BESIDE A PERSON: he plays as a person would (not of the party) and the partners
  // are HELPERS beside him, as a hosted co-op game's `addbot helper`: leashed to him by
  // kPartyLeash, the exit his (party.hpp).
  bool beside = false;
  // (`beside`) the helpers leave what heals to him when he needs it more (party.hpp
  // `party_leave_items`); off: they take it as a bot alone does (the lab's before)
  bool share_heals = true;
  // GEAR ON ARRIVAL (arrival_gear.hpp): the maps a playthrough plays before `first`, in order.
  // Each one's pickups are taken and its monsters paid for out of the ammunition on a scratch
  // server, and he enters `first` with that body. Empty: the new game's.
  std::vector<std::string> before;
  void* gear_ctx = nullptr;  // the loader's context for `before` (null: `load_ctx`)
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
  int64_t breaks = 0;   // glass he shot out of his way
  int64_t edges = 0;    // commands a pit's lip turned his keys back (way_edge)
  int64_t touches = 0;  // commands he walked into a trigger past its node (way_touch)
  int64_t forgot = 0;   // times he forgot the links given up on, with no way on
  std::vector<std::string> took_keys;         // the keys he took, by their class
  sh::Vec3f lo, hi;                           // the nav's bounds
  std::vector<sh::Vec3f> nodes;               // every node (the picture's floor)
  std::vector<sh::Vec3f> track;               // a point every 0.2 s
  std::vector<std::vector<sh::Vec3f>> party;  // each partner's, the same
  int64_t shares = 0, left_to_mates = 0;      // the party's splits; plans that left a stop to one
  std::vector<sh::Vec3f> exits;               // the exits' middles
  std::vector<sh::Vec3f> keys_at;             // every key where it lay
  std::vector<std::string> plan;              // his changes of plan (UnitOptions::plan)
  // (`beside`) the helpers' share of the time within kPartyLeash of him, the farthest one was,
  // the plans they went back to him, the steps they took off an exit.
  double helper_near_share = 0;
  float helper_farthest = 0;
  int64_t helper_stays = 0, helper_exits_turned = 0, helper_gave_way = 0;
  // (`beside`) what heals, shared (party.hpp `party_leave_items`): the health he took, the health
  // and armour (points stopped) the helpers took while he had no more than the taker and could
  // use it, and the plans a helper left an item to him.
  double leader_healed = 0, helper_heal_needed = 0, helper_armour_needed = 0;
  int64_t helper_left_items = 0;
};

struct UnitReport {
  bool ok = false;
  std::string error;
  std::vector<GearLeg> gear;  // each map of `before`: what it gave and cost
  std::string arrived;        // his gear as he entered `first` (q2_gear_text)
  std::vector<UnitMap> maps;
  bool unit_done = false;  // an exit out of the unit was taken
  std::string out_to;      // and where it led
  uint64_t digest = 0;
};

UnitReport run_q2_unit(const UnitOptions& opt);

}  // namespace shooters_edge::bots
