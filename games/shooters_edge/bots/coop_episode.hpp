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
// every fifth of a second. A digest folds every frame (the time, every player's origin, view and
// health), so a run replays to the bit.
//
// A PARTY OF BOTS (`partners`): the leader and his partners each run the way through, the map's
// work split between them by plugins/shooterbots/party.hpp (each stop to the nearest; seen places
// and secrets shared; the exit once nothing is left for any), so a party clears a map at 100%
// sooner than one bot alone.
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
// THE CENSUS (`census`): every entity of the map counted by its class name as the run sees it,
// so a pack's new monsters, items, movers and traps are shown at work: how many stood after the
// spawn, how many more the run made (shots, gibs, spawned helpers), how many moved and how many
// went (killed, picked up, used up), how many had their model taken away while they stayed (an
// item picked up), and the largest number alive at once.
//
// ge:summary A co-op episode played through: a leader finding each map's exit, companions beside
// ge:keywords coop-episode run_coop_episode episode play-through e1m1 leader way-through
// ge:keywords companions intermission changelevel parms real-maps party partners
// ge:keywords census class-name mission-pack spawned moved gone
// ge:skill shooter bots
// ge:test tests/shooters_edge_companion_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shooterbots/companion.hpp"
#include "shooterbots/party.hpp"
#include "shooterbots/way_through.hpp"
#include "shooters_edge/bots/arrival_gear.hpp"
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
  // THE PARTY (plugins/shooterbots/party.hpp): partners who run the way through beside the
  // leader, one each, the map's work split among them (nearest takes it), the exit waiting for
  // all of it; players 2.. after the leader, before the companions.
  std::vector<sb::AimSkill> partners;
  // (`partners`) BESIDE A PERSON: the leader plays as a person would (his own way through, not of
  // the party) and the partners are HELPERS beside him, as a hosted co-op game's `addbot helper`
  // (party.hpp's `PartyWork` round the people): leashed to him by kPartyLeash, the exit his.
  bool beside = false;
  // (`beside`) the helpers leave what heals to him when he needs it more (party.hpp
  // `party_leave_items`); off: they take it as a bot alone does (the lab's before)
  bool share_heals = true;
  sb::AimSkill leader = sb::AimSkill::expert;
  int skill = 1;  // the campaign's `skill`
  uint32_t seed = 1;
  std::string root = ".";                   // the repository's root (the campaign's sources)
  const ge::progs::Progs* progs = nullptr;  // a program of the caller's (none: our campaign)
  // The weapons and items the bots know (arsenals.hpp; none: Quake's own): a mission pack's
  // (`arsenal_for_mod`) when its program runs.
  const sb::BotArsenal* arsenal = nullptr;
  bool plan = false;  // keep the leader's changes of plan
  // A file every frame's line is written to (empty: none): the map, the frame, the digest so far,
  // the leader's place, view and health, his velocity, his flags and his command (msec, angles,
  // the three axes, the buttons), floats in exact hex, so two machines' runs can be diffed to the
  // first frame they part on and the field that parts first.
  std::string frames;
  bool wide_models = false;        // a game's own content: its movers sized as the window does
  bool registered = false;         // the content is the full game
  bool single = false;             // single player (coop 0), the leader alone
  bool complete = false;           // the 100% run (way_through.hpp)
  bool tour = true;                // and its stops taken in the tour's order (tour.hpp)
  bool god = false;                // the leader's (and his partners') `god` on every map
  std::vector<std::string> avoid;  // exits to these maps are not taken (a secret level)
  std::vector<std::string> stop;   // an exit to one of these ends the run (the episode's end)
  bool census = false;             // count each class name's entities (EpisodeMap::census)
  // GEAR ON ARRIVAL (arrival_gear.hpp): the maps a playthrough plays before `first`, in order. A
  // new game starts on the first of them; on each the leader takes every pickup and pays for its
  // monsters out of his ammunition, then the program's own level change carries him on, until
  // `first` is entered with that gear. Empty: `first` from a new game. The companions and
  // partners join on `first`.
  std::vector<std::string> before;
};

// One class name's entities on a map, as the run saw them (EpisodeOptions::census).
struct ClassCensus {
  std::string name;
  int spawned = 0;  // standing after the map's spawn
  int made = 0;     // made later in the run
  int moved = 0;    // moved more than a unit from where it first stood
  int gone = 0;     // removed (killed, picked up, used up)
  int taken = 0;    // its model taken away while it stayed (an item picked up)
  int most = 0;     // the most alive at once
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
  int killed_by_partners = 0;  // the party's partners' kills (EpisodeOptions::partners)
  sb::PartyStats party;        // the party's sharing
  int64_t partner_steps = 0;   // the steps the partners did
  int64_t left_to_mates = 0;   // plans that left a stop to a mate, the party's
  // (`beside`) the helpers' share of the time within kPartyLeash of the leader, the farthest one
  // was, the plans they went back to him and the steps they took off an exit.
  double helper_near_share = 0;
  float helper_farthest = 0;
  int64_t helper_stays = 0, helper_exits_turned = 0, helper_gave_way = 0;
  // (`beside`) WHAT HEALS, SHARED (party.hpp `party_leave_items`): the health and the armour
  // (points stopped) the helpers took while the leader had no more than the taker and could use it
  // (taken from a person who needed it more), what the leader took himself, and the plans a helper
  // left an item to him.
  double helper_heal_needed = 0, helper_armour_needed = 0, leader_healed = 0;
  int64_t helper_left_items = 0;
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
  // The map's tricks the move proved into blast jumps (map_nav.hpp `tricks_made`): rocket, grenade.
  int64_t nav_rockets = 0, nav_grenades = 0, nav_combos = 0;
  std::string last_fault;  // the program's newest fault on this map, with its stack
  double nav_ms = 0;       // wall time
  sh::Vec3f lo, hi;
  std::vector<sh::Vec3f> nodes;               // every node (the picture's floor)
  std::vector<sh::Vec3f> leader;              // a point every fifth of a second
  std::vector<std::vector<sh::Vec3f>> mates;  // each partner's and companion's, the same
  std::vector<uint8_t> way_kind;              // the plan's WayKind, every fifth of a second
  std::vector<sh::Vec3f> kills;               // where each monster fell
  std::vector<sh::Vec3f> secrets;             // where the leader stood as each secret counted
  // The leader's rocket and grenade jumps: each flight, the take-off first and the landing last.
  std::vector<std::vector<sh::Vec3f>> blasts;
  std::vector<sh::Vec3f> exits;     // the exits' middles
  std::vector<std::string> plan;    // the leader's changes of plan (EpisodeOptions::plan)
  std::vector<ClassCensus> census;  // by class name, in the order first seen (`census`)
};

struct EpisodeReport {
  bool ok = false;
  std::string error;
  std::vector<GearLeg> gear;  // each map of `before`: what it gave and cost
  std::string arrived;        // the leader's gear as he entered `first` (q1_gear_text)
  std::vector<EpisodeMap> maps;
  int64_t faults = 0;
  uint64_t digest = 0;
};

EpisodeReport run_coop_episode(const EpisodeOptions& opt);

}  // namespace shooters_edge::bots
