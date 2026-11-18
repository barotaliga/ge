// THE SERVER'S CHAT ROUTE for any game with spectators and teams: who reads a `say` or a
// `say_team`, and the prefix it reads with, by the match's phase.
//
// - A player's `say` reaches everyone as "<name>: <text>"; a `say_team` reaches the seats on the
//   player's team as "(<name>): <text>".
// - A spectator's `say_team` reaches only the spectators, as "[SPEC] <name>: <text>"; a `say`
//   reaches everyone, unless `sv_spectalk` is 0 while a match counts down or plays, when it too
//   reaches only the spectators with that prefix. Before and after a match a spectator always talks
//   to everyone.
// - A player's team REPORT (a line the player's message keys wrote, which the game recognises:
//   `report`) also reaches the spectators following a player on that team, or following nobody
//   with that team as their own, when `sv_sayteam_to_spec` allows it in this phase: 0 never, 1 in a
//   match (the default), 2 outside one, 3 always. A team line typed by hand stays private.
//
// THE SETTINGS (`add_chat_settings`, read by `chat_settings`): `sv_spectalk` and
// `sv_sayteam_to_spec`, on whichever console the game's chat reads them from.
//
// ge:summary The server's chat route: say and say_team, spectators, sv_spectalk, sv_sayteam_to_spec
// ge:keywords chat say say_team spectator-chat sv_spectalk sv_sayteam_to_spec chat_route
// ge:keywords spec-prefix team-chat team-report add_chat_settings chat_settings
// ge:skill net
// ge:test tests/server_chat_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "console/cvar.hpp"

namespace ge::server {

struct ChatSeat {
  bool active = false;  // someone holds the seat
  bool spectator = false;
  std::string name, team;
  int follows = -1;  // the seat a spectator follows (-1 none)
};

struct ChatSettings {
  bool spectalk = true;     // sv_spectalk
  int sayteam_to_spec = 1;  // sv_sayteam_to_spec 0..3
};

struct ChatLine {
  uint32_t to = 0;   // the seats that read it, a bit each (32 at most)
  std::string text;  // the line as printed, its newline included
};

void add_chat_settings(console::Cvars& cvars);
// The two settings from `cvars` (unregistered: their defaults).
ChatSettings chat_settings(const console::Cvars& cvars);

// `text` said by `slot` (`team`: a `say_team`; `report`: a team report the game recognised),
// `in_match` while a match counts down or plays.
ChatLine chat_route(std::span<const ChatSeat> seats, int slot, bool team, bool report,
                    bool in_match, const ChatSettings& s, const std::string& text);

}  // namespace ge::server
