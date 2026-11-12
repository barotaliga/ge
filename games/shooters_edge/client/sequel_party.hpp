// THE SEQUEL'S COMPANIONS: a co-op companion (bots/companion.hpp) beside the window's player in the
// sequel's single-player or co-op game, which runs on the rules themselves
// (server/sequel_server.hpp keeps the campaign: the body carried, the coop spots) rather than on a
// campaign host. The match path (net/classic_game.cpp) owns one once `addbot` is typed outside
// deathmatch:
//
// - `addbot [novice|average|expert]` seats a companion in the lowest free slot of the rules (its
//   player connected as a bot), the window's player (slot 0) its leader; `removebot` lets the
//   highest one go;
// - the NAV is built off the map's brushes (bots/map_nav.hpp, from `info_player_start`), and the
//   fighter reads the sequel's arsenal (bots/bot_arsenal.hpp) and the sequel's monsters;
// - every map entered (a level change on the same rules, or a new game on new ones) rebuilds the
//   nav and makes each companion afresh on it; a map no companion can walk sends them away;
// - each frame a companion's packet goes into the frame after the player's, in slot order, made
//   from what its client is sent.
//
// Deterministic: the companions' hands are seeded off the party's seed and their slot.
//
// ge:summary The sequel's companions: addbot seats a co-op escort on the sequel's own rules
// ge:keywords sequel companion coop co-op addbot removebot escort party sequel-party brushes-nav
// ge:keywords sequel_party_add sequel_party_remove sequel_party_enter sequel_party_packets
// ge:skill shooter bots
// ge:test tests/shooters_edge_listen_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "shooter/aim.hpp"
#include "shooters_edge/bots/fighter.hpp"
#include "shooters_edge/bots/map_nav.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::client {

namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

inline constexpr int kSequelMatesMost = 7;  // companions at most (a coop game's eight players)

struct SequelMate {
  bots::Fighter fighter;
  sh::AimSkill skill = sh::AimSkill::average;
  std::string name;
  sh::MoveCmd cmds[3];  // the three newest, oldest first
  uint32_t sequence = 0;
};

struct SequelParty {
  std::array<std::unique_ptr<SequelMate>, sh::kMaxClients> mates;  // by slot (null: none)
  std::unique_ptr<bots::MapNav> nav;                               // the map's
  std::string entities;  // the map's entity text the fighters learn its items from
  uint32_t seed = 1;
  int named = 0;             // companions ever seated (their names count up)
  bool turned_coop = false;  // the game was single player until the first one came
};

// How many companions are seated.
int sequel_party_size(const SequelParty& p);
// A companion of `skill` seated on `rules` (playing `map`) in the lowest slot that is free on
// them and outside `held` (a bit a slot taken elsewhere): its slot, or -1 with `error` (the party
// or the server full, or a map with no floor to walk).
int sequel_party_add(SequelParty& p, const sv::RulesRunner& rules, const sv::MapSpec& map,
                     sh::AimSkill skill, uint32_t held, std::string& error);
// The highest companion's slot (its seat not yet let go), or -1 when none is seated.
int sequel_party_last(const SequelParty& p);
// Companion `slot` forgotten (the caller drops its player).
void sequel_party_forget(SequelParty& p, int slot);
// A map entered on `rules`: the nav rebuilt and every companion made afresh on it, connected again
// first when `reconnect` (new rules). A map no companion can walk empties the party, said in
// `error`, the slots it held in `gone`.
void sequel_party_enter(SequelParty& p, const sv::RulesRunner& rules, const sv::MapSpec& map,
                        bool reconnect, std::vector<int>& gone, std::string& error);
// The companions' packets for the next frame (each `msec` long), appended to `out`. A view the
// rules turned (a spawn, a teleport) is taken as the companion's own, as the window's player takes
// it: its next command sends the turned angles back, so the rules spend the turn.
void sequel_party_packets(SequelParty& p, const sv::RulesRunner& rules, int msec,
                          std::vector<sh::Packet>& out);

}  // namespace shooters_edge::client
