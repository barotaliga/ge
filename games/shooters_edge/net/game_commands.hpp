// THE GAME'S OWN COMMANDS FROM THE WINDOW'S CONSOLE: the words a player types (or binds a key to)
// that go to the game's rules as the player's command, on the server joined or the window's own.
//
// - `cmd <line>`: the line as it is, to whichever game is played (a mod's own commands).
// - The sequel's screens and inventory (server/sequel_screens.hpp): `score`, `inven`, `putaway`,
//   `invnext`, `invprev`, `invnextw`, `invprevw`, `invnextp`, `invprevp`, `invuse` and `invdrop`.
//   They go to the sequel's rules only; in another game they do nothing, so a key the sequel's
//   own config binds to one (TAB to `inven`) costs the first game nothing. The help computer is
//   `cmd help` (the console's `help` explains its words).
//
// `add_server_commands` adds these and the administrator's words (classic_admin.hpp).
//
// ge:summary The game's own commands from the window's console: cmd, the sequel's screen words
// ge:keywords cmd client-command forward score inven putaway invnext invprev invuse invdrop
// ge:keywords sequel game-commands console-words
// ge:skill shooter console
// ge:test tests/shooters_edge_game_commands_test.cpp
#pragma once

#include <string>

namespace ge::console {
class Console;
}
namespace shooters_edge::client {
struct Listen;
}

namespace shooters_edge::net {

struct Remote;

// The sequel's words (above), in the order `add_server_commands` adds them.
inline constexpr const char* kSequelWords[] = {"score",    "inven",    "putaway",  "invnext",
                                               "invprev",  "invnextw", "invprevw", "invnextp",
                                               "invprevp", "invuse",   "invdrop"};

// `line` to the game played as the player's command: to the server `r` joined, else to the match
// `l` hosts; false when there is neither.
bool send_game_command(client::Listen& l, Remote& r, const std::string& line);

// `cmd`, the sequel's words and the administrator's (classic_admin.hpp) on `con`.
void add_server_commands(ge::console::Console& con, client::Listen& l, Remote& r);

}  // namespace shooters_edge::net
