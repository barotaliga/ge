// QUAKE II'S COMPANIONS: a co-op companion (shooterbots/companion.hpp) beside the window's player
// in the Quake II's single-player or co-op game, which runs on the rules themselves
// (server/q2_server.hpp keeps the campaign: the body carried, the coop spots) rather than on a
// campaign host. The match path (net/qw_game.cpp) owns one once `addbot` is typed outside
// deathmatch:
//
// - `addbot [novice|average|expert] [escort|helper]` seats a companion in the lowest free slot of
//   the rules (its player connected as a bot), the window's player (slot 0) its leader;
//   `removebot` lets the highest one go;
// - an ESCORT (the default) keeps at the player's shoulder; a HELPER works the level near the
//   people (shooterbots/party.hpp's `PartyWork`, as Quake's co-op party does: the helpers split
//   the map by the walk, leave what lies beyond the leash and the exit to the people);
// - the NAV is built off the map's brushes (shooterbots/map_nav.hpp, from `info_player_start`), and
// the
//   fighter reads Quake II's arsenal (shooterbots/bot_arsenal.hpp) and Quake II's monsters;
// - every map entered (a level change on the same rules, or a new game on new ones) rebuilds the
//   nav and makes each companion afresh on it; a map no companion can walk sends them away;
// - each frame a companion's packet goes into the frame after the player's, in slot order, made
//   from what its client is sent.
//
// Deterministic: the companions' hands are seeded off the party's seed and their slot.
//
// ge:summary Quake II's companions: addbot seats a co-op escort on Quake II's own rules
// ge:keywords q2 companion coop co-op addbot removebot escort party q2-party brushes-nav
// ge:keywords q2_party_add q2_party_remove q2_party_enter q2_party_packets helper
// ge:skill shooter bots
// ge:test tests/shooters_edge_listen_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "shooterbots/aim.hpp"
#include "shooterbots/fighter.hpp"
#include "shooterbots/map_nav.hpp"
#include "shooterbots/party.hpp"
#include "shooters_edge/bots/arsenals.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::client {

namespace sh = ge::shooter;

namespace sb = ge::shooterbots;
namespace sv = shooters_edge::server;

inline constexpr int kQ2MatesMost = 7;  // companions at most (a coop game's eight players)

struct Q2Mate {
  bots::Fighter fighter;
  std::unique_ptr<sb::PartyWork> work;  // a helper's (null: an escort)
  sb::AimSkill skill = sb::AimSkill::average;
  std::string name;
  sh::MoveCmd cmds[3];  // the three newest, oldest first
  uint32_t sequence = 0;
};

struct Q2Party {
  std::array<std::unique_ptr<Q2Mate>, sh::kMaxClients> mates;  // by slot (null: none)
  std::unique_ptr<bots::MapNav> nav;                           // the map's
  std::string entities;  // the map's entity text the fighters learn its items from
  uint32_t seed = 1;
  int named = 0;                     // companions ever seated (their names count up)
  bool turned_coop = false;          // the game was single player until the first one came
  const sv::MapSpec* map = nullptr;  // the map the helpers' ways were read off
  sb::Party share;                   // the helpers' split of the map (shooterbots/party.hpp)
  std::vector<sb::PartyMember> members;
  sb::PartyOptions people;
};

// How many companions are seated.
int q2_party_size(const Q2Party& p);
// A companion of `skill` seated on `rules` (playing `map`) in the lowest slot that is free on
// them and outside `held` (a bit a slot taken elsewhere): its slot, or -1 with `error` (the party
// or the server full, or a map with no floor to walk).
int q2_party_add(Q2Party& p, const sv::RulesRunner& rules, const sv::MapSpec& map,
                 sb::AimSkill skill, uint32_t held, std::string& error, bool helper = false);
// The highest companion's slot (its seat not yet let go), or -1 when none is seated.
int q2_party_last(const Q2Party& p);
// Companion `slot` forgotten (the caller drops its player).
void q2_party_forget(Q2Party& p, int slot);
// A map entered on `rules`: the nav rebuilt and every companion made afresh on it, connected again
// first when `reconnect` (new rules). A map no companion can walk empties the party, said in
// `error`, the slots it held in `gone`.
void q2_party_enter(Q2Party& p, const sv::RulesRunner& rules, const sv::MapSpec& map,
                    bool reconnect, std::vector<int>& gone, std::string& error);
// The companions' packets for the next frame (each `msec` long), appended to `out`. A view the
// rules turned (a spawn, a teleport) is taken as the companion's own, as the window's player takes
// it: its next command sends the turned angles back, so the rules spend the turn.
void q2_party_packets(Q2Party& p, const sv::RulesRunner& rules, int msec,
                      std::vector<sh::Packet>& out);

}  // namespace shooters_edge::client
