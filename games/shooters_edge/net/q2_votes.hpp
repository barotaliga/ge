// QUAKE II'S DEDICATED SERVER'S VOTES AND SETTINGS SHEET: the engine's votes
// (engine/server/vote.hpp) on our Quake II server (q2_game.hpp), and the sheet the server settings
// page reads (engine/server/sheet.hpp), as our QuakeWorld server has them (qw_game.cpp,
// qw_sheet.hpp).
//
// THE VOTES: a player's `vote <setting> <value>`, `vote yes|no|cancel`, `vote`, `vote rules` (and
// `yes`, `no` alone while one is open) are the vote's before the rules see them: the hook sits on
// the server's `line_hook` after whatever was there (the logins'). The voters are the players on
// the server (signed on, a slot held), each told the vote's lines. What may change: the rules'
// settings past the vote's lists, except the owner's (`kQ2OwnerRules`: the game's mode, skill and
// the map list); `vote map <name>` (a map the server has) and `vote kick <id|name>` are acts. A
// passed change is set on the rules at once; the few the rules read only as a map starts
// (`k_midair`, `sv_fps`, `sv_wear`, `sv_physics`) restart the map. The next map puts every change
// back
// (`q2_votes_map_over`, which the level change calls; a restart for a vote keeps them).
//
// THE SHEET: a client that agreed the `ge` set's `settings-sheet` bit (`*ge_ext` 2048) is offered
// it as it enters (svc 120, an empty sheet: begin, end), so its window knows the server sends one
// and never sends a stock server a word it would print as chat. It asks with `ge_sheet` (`ge_sheet
// stop`: no more) and gets svc 120's parts (begin, a row a message, end) and the
// sheet again when a vote changes a setting or it enters again after a level change. The parts go
// as the channel drains, so a packet stays within the 1400 bytes a client reads.
//
// ge:summary Our Quake II server's votes and the settings sheet it sends on svc 120
// ge:keywords q2-votes q2_votes_open q2_votes_frame q2_votes_map_over q2_votes_sheet vote
// ge:keywords settings-sheet ge_sheet svc-120 vote-map vote-kick server-settings
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_votes_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "server/server.hpp"
#include "server/sheet.hpp"
#include "server/vote.hpp"
#include "shooters_edge/net/q2_net_server.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::net {

constexpr uint8_t kSvcQ2GeSheet = 120;  // the game's own: a part of the settings sheet
// The rules' settings a vote never changes on Quake II's server: the owner's.
constexpr const char* kQ2OwnerRules = "deathmatch coop skill sv_maplist nextserver";

struct Q2Votes {
  ge::server::VoteBook book;
  Q2NetServer* net = nullptr;
  shooters_edge::server::RulesRunner* rules = nullptr;
  std::function<std::string()> map;                      // the map now
  std::function<bool(const std::string& name)> has_map;  // `vote map`: a map the server has
  // The hook that was on the server before ours (the logins'), run first.
  bool (*prior)(void* ctx, int slot, const std::string& line) = nullptr;
  void* prior_ctx = nullptr;
  // The sheet: the slots keeping it, each one's parts still to go, and the user id each slot was
  // offered it as (-1: not yet, or not active since).
  uint32_t watchers = 0;
  std::array<std::vector<std::pair<ge::server::SheetPart, std::string>>, 32> out;
  std::array<size_t, 32> out_next{};
  std::array<int, 32> offered{};
  std::array<int, 32> watcher_id{};
  bool changed = false;  // a vote changed a setting this frame: the sheet goes again
};

// The rules the votes change and the sheet reads, by the server's sv_vote settings (`settings`);
// the sheet reads them from here on, the votes once `q2_votes_open` hooks them to the network.
void q2_votes_rules(Q2Votes& v, shooters_edge::server::RulesRunner& rules,
                    const ge::server::VoteSettings& settings);
// The votes taken on `net`'s server (its line hook, after the one there).
void q2_votes_open(Q2Votes& v, Q2NetServer& net);
// Each network frame: the voters kept, the vote's time checked, its lines told and its passed
// changes made; the sheet offered, sent and paced.
void q2_votes_frame(Q2Votes& v);
// The map is over (not restarted): every change a vote made is put back on the rules.
void q2_votes_map_over(Q2Votes& v);
// The sheet as the server stands now.
void q2_votes_sheet(const Q2Votes& v, ge::server::SettingsSheet& out);

}  // namespace shooters_edge::net
