// THE SERVER'S OWN CHAT, for every runner whose mod leaves a `say` or `say_team` to the server (the
// classic rules in QC when the program names no chat hook, a compiled module that answers 0): who
// hears a line and what it reads, alike on both.
//
// THE ROUTE (`chat_route`): a player's `say` reaches everyone as "<name>: <text>"; a `say_team`
// reaches those with the same `team` in their user info as "(<name>): <text>". A spectator's
// `say_team` reaches only the spectators, as "[SPEC] <name>: <text>"; his `say` reaches everyone
// unless `sv_spectalk` is 0, when it too reaches only the spectators with that prefix, so a match's
// players are not told what the watchers see.
//
// THE SETTING (`add_chat_settings`): `sv_spectalk`, 0 or 1, 1 by default; a rule a server.cfg,
// an administrator or a vote may change.
//
// ge:summary The server's own chat for every runner: who hears a say or say_team, sv_spectalk
// ge:keywords chat say say_team spectator-chat sv_spectalk spec-prefix chat_route
// ge:keywords add_chat_settings team-chat
// ge:skill shooter
// ge:test tests/shooters_edge_chat_rules_test.cpp
#pragma once

#include <cstdint>
#include <string>

#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::server {

struct ChatLine {
  uint32_t to = 0;   // the slots that hear it, a bit each
  std::string text;  // the line as printed, its newline included
};

// `sv_spectalk` registered in a runner's settings (1).
void add_chat_settings(ge::console::Cvars& cvars);
// Who hears `text` said by `slot` (`team`: a `say_team`), and what they read.
ChatLine chat_route(const RulesCommon& s, int slot, bool team, const std::string& text);

}  // namespace shooters_edge::server
