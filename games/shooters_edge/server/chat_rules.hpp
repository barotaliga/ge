// THE SERVER'S OWN CHAT, for every runner whose mod leaves a `say` or `say_team` to the server (the
// Quake's rules in QC when the program names no chat hook, a compiled module that answers 0): the
// engine's route (engine/server/chat.hpp: who reads a line, the spectators' prefix, `sv_spectalk`,
// `sv_sayteam_to_spec`) fed from the runner's clients. This file only wires it:
//
// - each seat's name and team from its user info, a spectator's followed player from `tracks`;
// - a team REPORT is a line holding a carriage return (a client's team-message macros put one in,
//   so a team line typed by hand stays private);
// - THE PHASE is the lobby's, from the local info `ge_phase` (`announce_phase`); a server with no
//   lobby has none and is always in its match.
//
// ge:summary The server's own chat for every runner: the engine's chat route fed from the clients
// ge:keywords chat say say_team spectator-chat sv_spectalk spec-prefix chat_route
// ge:keywords add_chat_settings team-chat sv_sayteam_to_spec k_sayteam_to_spec spectalk
// ge:keywords team-chat-to-spectators chat_in_match team-report
// ge:skill shooter
// ge:test tests/shooters_edge_chat_rules_test.cpp
#pragma once

#include <string>

#include "server/chat.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::server {

using ChatLine = ge::server::ChatLine;
using ge::server::add_chat_settings;  // sv_spectalk and sv_sayteam_to_spec

// Whether the match counts down or plays: the local info's `ge_phase`, a match without one.
bool chat_in_match(const RulesCommon& s);
// Who hears `text` said by `slot` (`team`: a `say_team`), and what they read. `tracks`: each
// slot's followed player (-1 none), a spectator's; null when the endpoint keeps none.
ChatLine chat_route(const RulesCommon& s, int slot, bool team, const std::string& text,
                    const int* tracks = nullptr);

}  // namespace shooters_edge::server
