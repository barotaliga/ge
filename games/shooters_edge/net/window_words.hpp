// THE WINDOW'S OWN WORDS OVER THE GAME IT PLAYS: what a player types in the window's console that
// goes to the match the window hosts or to the server it joined.
//
// - `map <name>`: the map played next (no name: the one played now).
// - `sv <line>`: a line for the hosted server's console (the administrator's words first).
// - The chat line (`say`, `say_team`, `say_near`): to the server joined, else to the match hosted;
//   refused while watching a broadcast or with nobody to read it.
// - `v_cshift <r> <g> <b> <strength>`: a tint over the view in open air (a mod stuffs it).
// - `vote`, `yes`, `no`: the vote on a setting (engine/server/vote.hpp).
// - Any other word the window does not know: to Quake II's rules, then to the campaign's.
//
// ge:summary The window's own words over the game it plays: map, sv, chat, v_cshift, vote, the rest
// ge:keywords map sv chat say say_team say_near v_cshift vote yes no unknown-word window-words
// ge:skill shooter console
// ge:test tests/shooters_edge_game_commands_test.cpp
#pragma once

#include <string>

namespace ge::console {
class Console;
class ConsoleView;
}  // namespace ge::console
namespace shooters_edge::client {
struct Listen;
struct TeamChat;
}  // namespace shooters_edge::client

namespace shooters_edge::net {

struct Remote;

// The words above on `con` and the chat line on `view`, over `listen`, `remote` and the map asked
// for next (`next_map`); each must outlive `con`.
void add_window_words(ge::console::Console& con, ge::console::ConsoleView& view,
                      client::Listen& listen, Remote& remote, std::string& next_map);

// The team chat's lines (client/team_chat.hpp) to the server joined, else to the match hosted: its
// sending, whether a teammate is in the game, and the scene its %-codes read.
void wire_team_chat(client::TeamChat& chat, client::Listen& listen, Remote& remote);

}  // namespace shooters_edge::net
