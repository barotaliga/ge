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
// ge:keywords classic-server simulated-network nav doors plats team-game record client_fed mirror
// ge:skill shooter bots net
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "progs/file.hpp"
#include "shooter/aim.hpp"
#include "shooter/features.hpp"
#include "shooter/locs.hpp"
#include "shooter/pilot.hpp"
#include "shooter/temper.hpp"
#include "shooter/traits.hpp"
#include "shooter/vec.hpp"
#include "shooters_edge/net/classic_hits.hpp"

namespace shooters_edge::bots {
struct Fighter;
}
namespace shooters_edge::server {
struct RulesCommon;
}

namespace shooters_edge::net {

namespace sh = ge::shooter;

struct TeamSide {
  TeamSide(std::string n = "red", int c = 4) : name(std::move(n)), colour(c) {}
  std::string name;
  int colour;  // top and bottom
  std::vector<sh::AimSkill> skills = {sh::AimSkill::expert, sh::AimSkill::average,
                                      sh::AimSkill::average, sh::AimSkill::novice};
  std::vector<sh::Temper> tempers;  // each player's temperament (missing: balanced)
  // Each player's traits over his skill's and temperament's preset ("aim_rl=9/timing=3",
  // shooter/traits.hpp; missing or empty: the preset).
  std::vector<std::string> traits;
  // The side's bot features over each skill's preset (shooter/features.hpp: "-dodge,+rj",
  // "plain"; empty: the preset), for every player on it.
  std::string features;
  std::vector<std::string> names;  // the players' (empty: the team's name and a number)
};

struct TeamMatchOptions {
  std::span<const uint8_t> map;  // the map file
  std::string map_name = "halls";
  std::string entities;  // in place of the map's entity text (empty: the map's own)
  sh::Locs locs;         // the map's place names, `from` where (none: named off its items)
  const ge::progs::Progs* progs = nullptr;  // the baseline mod, compiled
  TeamSide teams[2] = {TeamSide("red", 4), TeamSide("blue", 13)};
  int frames = 46154;  // server frames of 13 ms (ten minutes)
  int timelimit = 0;   // minutes the mod ends the match at (0: none)
  // The deathmatch mode: 1 the items on the map; 4 every spawn armed with everything and full of
  // it, nothing to gather (the baseline mod's common/dmm4.qc): the duel that tests aim and move.
  int deathmatch = 1;
  // More of the server's settings, each a console line ("k_disallow_weapons 16"), set before
  // the map starts.
  std::vector<std::string> settings;
  uint32_t seed = 1;    // the mod's generator; fighter k's hands seed + 31 k
  bool record = true;   // keep the multiview recording
  bool packed = false;  // record it packed (classic_record.hpp: a `.mvdz`)
  int path_every = 8;   // a path point every this many frames
  // A weapon drill: every player holds only this weapon (its bit in the baseline mod), its
  // ammunition topped up, so the hit rates measure one weapon's aim over the whole match (0: a
  // normal match).
  int32_t drill = 0;
  // Every fighter reads only what its own client was sent (classic_mirror.hpp), as it must on a
  // server it cannot read; false: it reads our server's world, as before.
  bool client_fed = false;
  // The network between every client and the server, one way (0: a LAN), with a jitter of a
  // tenth of it: 50 is a round trip of about 100 ms, a player across an ocean.
  double latency_ms = 0;
  bool antilag = false;  // the server rewinds hitscan to what the shooter saw (its antilag)
  bool lead_lag = true;  // the fighters lead their shots on the round trip (false: as on a LAN)
  // Called after each fighter's command (player `k` of the match, the world it read, its
  // command) with `on_command_ctx`: a lab's per-command trace. Null: none.
  void (*on_command)(void* ctx, int k, const bots::Fighter& f, const server::RulesCommon& w,
                     const sh::MoveCmd& cmd) = nullptr;
  void* on_command_ctx = nullptr;
  // Every fighter's decisions recorded on this (`follow` by its slot; engine/ai/why.hpp), for a
  // lab's decision log (labs/why_log.hpp); recording decides nothing. Null: none.
  ge::ai::Why* why = nullptr;
};

struct TeamPlayer {
  std::string name;
  int team = 0;
  int slot = -1;
  sh::AimSkill skill = sh::AimSkill::average;
  sh::Temper temper = sh::Temper::balanced;
  sh::Traits traits;      // what he played with
  sh::Features features;  // and the behaviours he had (shooter/features.hpp)
  float frags = 0;
  int deaths = 0;
  int64_t shots = 0, pickups = 0, switches = 0, packs = 0, held_fire = 0;
  int64_t reactions = 0, pre_aimed = 0;  // sightings reacted to, and pre-aimed (shooter/bot.hpp)
  int64_t strokes = 0, lifts = 0;        // the mouse hand's strokes and lifts (ai/mouse.hpp)
  int64_t target_kept = 0;   // commands the one in hand was kept over a nearer one (bot.hpp)
  int64_t sound_swipes = 0;  // the view swiped at a sound (bot.hpp `listen`)
  double react_s = 0;        // the reactions' times, summed
  std::array<HitCount, kHitWeapons> hits{};  // shots and hits by weapon (classic_hits.hpp)
  double given = 0;                          // damage given to enemies (classic_hits.hpp)
  double lead_sum = 0, lead_abs = 0;         // hitscan shots' lead miss (classic_hits.hpp)
  int64_t lead_n = 0;
  // Sampled alive every 4 frames: an enemy in plain view (any way he looks), one seen (in his
  // cone), and an enemy in view while he roams with nobody known.
  int64_t view_n = 0, in_view = 0, seen_n = 0, unnoticed = 0;
  // Of the samples with an enemy in plain view: those it saw one (in its cone), aimed, and was
  // still reacting (no shot yet).
  int64_t in_view_seen = 0, in_view_aiming = 0, in_view_reacting = 0;
  // Of those with none seen: the nearest one in plain view by his yaw off the view, degrees:
  // within 60 (the screen's half width: out of its height, or not yet taken in), 60 to 90, 90 to
  // 135, behind; and of all of them, those while its view was the calm walker's.
  int64_t unseen_off[4] = {}, unseen_calm = 0;
  // Moving (over 100 units/s) with no enemy in plain view, by what held the view (kViewHolders:
  // the aim, a look at a spot, the calm walker, the pilot walking, the pilot flying or hopping,
  // the rest): the samples, and those with the view over 45 degrees off the way it moves.
  int64_t moving_n[6] = {}, moving_off[6] = {};
  int64_t calm_route_off = 0;  // and of the calm walker's, the pilot's own yaw over 45 off
  int64_t trigger_checks = 0, trigger_far = 0, trigger_off = 0;  // bots/fighter.hpp's
  // The launcher: taken (a pack's counted), and sampled with the view's: had, and had loaded.
  int64_t rl_pickups = 0, rl_samples = 0, rl_loaded = 0;
  int64_t dives = 0,
          off_graph = 0;       // dives begun into a pocket; thinks spent off the graph or in a pool
  int64_t dives_given_up = 0;  // dives turned back, late or short of air
  double dive_wet_s = 0;       // time in the water on a dive
  int64_t mode_commands[4] = {}, role_commands[5] = {};  // by bots::Role
  int64_t weapon_commands[9] = {};
  int64_t pilot_modes[sh::kPilotModes] =
      {};  // live commands in each pilot mode (shooter/pilot.hpp)
  int64_t plans = 0, stuck = 0, lost = 0, waits = 0, barred = 0;
  int64_t goal_changes = 0;  // its item goal changed (bots/fighter.hpp's commitment)
  int64_t surfaced = 0, surface_failed = 0;  // up for air, rises given up (bots/water.hpp)
  int64_t on_the_way = 0;                    // short goals taken up on the way (bots/fighter.hpp)
  int64_t took[12] = {};                     // pickups by the item's kind (bots/fighter.hpp)
  int64_t swim_fights = 0;                   // commands swum with the view on an enemy in sight
  // Every death's pack it came to know of, by how its place was known (bots/packs.hpp
  // `PackFrom`); its guesses of the weapon in them, right, launchers guessed and truly in them;
  // its trips to one, packs reached and seen gone.
  int64_t packs_known[5] = {};
  int64_t pack_guesses = 0, pack_right = 0, pack_guess_rl = 0, pack_truth_rl = 0;
  int64_t pack_trips = 0, pack_reached = 0, pack_seen_gone = 0;
  int64_t wet = 0;           // commands alive waist deep in water
  int64_t wet_why[12] = {};  // and by cause (bots/fighter.hpp kWetWhy)
  double wet_speed[3] = {};  // wet and aiming, swimming, walking (bots/fighter.hpp)
  int64_t wet_n[3] = {}, wet_still[3] = {};
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
  int64_t outmatched = 0;                     // commands turned from a fight it believed it loses
  int64_t sound_chases = 0;                   // commands going at an enemy it only heard
  int64_t dodging = 0, dodge_turns = 0;       // commands side-stepping in a fight, and turns back
  // The press's commands: weighed, on at him, back, held off near him, not clear (footwork.hpp).
  int64_t press_weighed = 0, press_on = 0, press_back = 0, press_near = 0, press_blocked = 0;
  int64_t rocket_jumps = 0, flips = 0;    // rocket jumps fired on the way, and flips of them
  int64_t escapes = 0, escapes_out = 0;   // rockets out of a pit's liquid, and those landed out
  int64_t gap_jumps = 0, gap_landed = 0;  // gap tricks jumped out of a hop, and landed across
  int64_t gap_refused = 0;                // gap tricks given up (no jump lands across)
  int64_t hops = 0, hop_jumps = 0;        // hop stretches entered, and jumps pressed in them
  int64_t circles = 0;                    // hops started by a circle jump
  double circle_speed = 0;                // and their takeoff speeds, summed
  int64_t watching = 0;  // commands standing with the view on a way in (bots/watch.hpp)
  double speed_sum = 0;  // its speed across the floor, summed over its commands alive
  double mode_speed[sh::kPilotModes + 1] = {};  // and by the pilot's mode, with each mode's count
  int64_t mode_n[sh::kPilotModes + 1] = {};
  int64_t mirrored = 0, predicted = 0;  // client-fed: commands on its client's world, predicted
  double lag_s = 0;                     // client-fed: the round trips its aim led by, summed
  int64_t lag_n = 0;                    // and their count
  double mirror_err = 0;                // client-fed: summed units between its own place as its
  int64_t mirror_err_n = 0;             // world holds it and the server's, alive, and the count
};

struct TeamTotals {
  float frags = 0;
  int deaths = 0;
  int64_t plans = 0, rushes = 0, quads = 0, held_shots = 0, callouts = 0;
  int64_t quad_pickups = 0;                            // the quad taken by a member
  int64_t messages = 0;                                // team messages said
  int64_t lost_seen = 0, retakes = 0, spam_shots = 0;  // areas worked out lost, retaken; spam
  int64_t pushes = 0, joins = 0;                       // pushes on the base called, and joined
};

// A button on the map, as the fighters read it (bots/buttons.hpp).
struct TeamButton {
  sh::Vec3f at;
  bool shoot = false;              // opened by a shot, not a touch
  std::vector<sh::Vec3f> doors;    // the middles of the doors it opens
  std::vector<std::string> opens;  // the items behind them
  sh::Vec3f from;                  // where a player presses (or shoots) it from
};

// The key items whose lying untaken is broken down (TeamMatchReport::lying), and the causes.
constexpr int kLyingItems = 6, kLyingWhy = 6;
inline constexpr const char* kLyingItemNames[kLyingItems] = {"ga", "ya", "ra", "mh", "quad", "rl"};
inline constexpr const char* kLyingWhyNames[kLyingWhy] = {"unknown", "unwanted", "busy",
                                                          "going",   "near",     "on-it"};

struct TeamMatchReport {
  bool ok = false;
  std::string error;  // why it did not run, with the next step
  int frames = 0;
  double seconds = 0;  // the server's time at the end
  std::vector<TeamPlayer> players;
  TeamTotals teams[2];
  // Shots, hits and damage by the target's state, and the frames lived in each (classic_hits.hpp).
  std::array<HitCount, kTargetStates> pellets_at{}, rockets_at{};
  std::array<double, kTargetStates> given_at{};
  std::array<int64_t, kTargetStates> frames_at{};
  // The key items (kLyingItems) sampled every 4 frames: the samples, those it lay on its spot, and
  // of those, by what the fighters made of it (kLyingWhy): nobody alive believed it there; some
  // believed it there and none wanted it; one wanted it while fighting; one on its way to it,
  // within 400 units of it, or standing on it (within 64: an armour no better than his own).
  std::array<int64_t, kLyingItems> lying_n{}, lying{};
  std::array<std::array<int64_t, kLyingWhy>, kLyingItems> lying_why{};
  std::vector<std::string> frag_log;
  std::vector<std::string> chat;    // every team message, "t team name: words", in time order
  std::vector<std::string> prints;  // every print the first player's client was sent (obituaries)
  // Every death: "t player x y z liquid mode link x y z mode" (the liquid the body was in: water,
  // slime, lava or -; the pilot's mode and the kind of link it was on; where it was 3 s before
  // and its mode then), in time order.
  std::vector<std::string> deaths;
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
  std::vector<sh::Loc> places;               // each place once, at its points' middle
  int64_t lines_read = 0, lines_missed = 0;  // team lines the fighters read and missed
  std::string plan;                          // the map's team plan as lines (bots/map_plan.hpp)
  std::string goals;                         // and its goals, a line each (bots/goals.hpp)
  int32_t plan_areas = 0, plan_base = -1;
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
