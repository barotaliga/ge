// THE SERVER'S OWN CHAT, for every runner whose mod leaves a `say` or `say_team` to the server (the
// classic rules in QC when the program names no chat hook, a compiled module that answers 0): who
// hears a line and what it reads, alike on both.
//
// THE ROUTE (`chat_route`): a player's `say` reaches everyone as "<name>: <text>"; a `say_team`
// reaches those with the same `team` in their user info as "(<name>): <text>". A spectator's
// `say_team` reaches only the spectators, as "[SPEC] <name>: <text>"; his `say` reaches everyone
// unless `sv_spectalk` is 0 while a match counts down or plays, when it too reaches only the
// spectators with that prefix, so a match's players are not told what the watchers see; before
// and after a match a spectator always talks to everyone.
//
// A PLAYER'S TEAM CHAT TO THE SPECTATORS (`sv_sayteam_to_spec`): a spectator also reads a player's
// `say_team` when the setting allows it in this phase, the line holds a carriage return (a
// client's team-message macros put one in, so a team line typed by hand stays private), and the
// spectator follows a player on the sender's team (`tracks`), or, following nobody, has the
// sender's team in his own user info.
//
// THE PHASE is the lobby's, from the local info `ge_phase` (`announce_phase`); a server with no
// lobby has none and is always in its match.
//
// THE SETTINGS (`add_chat_settings`): `sv_spectalk`, 0 or 1, 1 by default; `sv_sayteam_to_spec`,
// 0 never, 1 in a match (the default), 2 outside one, 3 always; rules a server.cfg, an
// administrator or a vote may change.
//
// ge:summary The server's own chat for every runner: who hears a say or say_team, sv_spectalk
// ge:keywords chat say say_team spectator-chat sv_spectalk spec-prefix chat_route
// ge:keywords add_chat_settings team-chat sv_sayteam_to_spec k_sayteam_to_spec spectalk
// ge:keywords team-chat-to-spectators
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

// `sv_spectalk` (1) and `sv_sayteam_to_spec` (1) registered in a runner's settings.
void add_chat_settings(ge::console::Cvars& cvars);
// Whether the match counts down or plays: the local info's `ge_phase`, a match without one.
bool chat_in_match(const RulesCommon& s);
// Who hears `text` said by `slot` (`team`: a `say_team`), and what they read. `tracks`: each
// slot's followed player (-1 none), a spectator's; null when the endpoint keeps none.
ChatLine chat_route(const RulesCommon& s, int slot, bool team, const std::string& text,
                    const int* tracks = nullptr);

}  // namespace shooters_edge::server
