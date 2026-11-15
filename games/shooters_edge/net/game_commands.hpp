// THE GAME'S OWN COMMANDS FROM THE WINDOW'S CONSOLE: the words a player types (or binds a key to)
// that go to the game's rules as the player's command, on the server joined or the window's own.
//
// - `cmd <line>`: the line as it is, to whichever game is played (a mod's own commands).
// - The words every game's rules answer: `kill`, `god`, `give <what>` and `noclip`, to whichever
//   game is played, with what follows them.
// - The sequel's own words, with what follows them: its screens and inventory
//   (server/sequel_screens.hpp: `score`, `inven`, `putaway`, `invnext`, `invprev`, `invnextw`,
//   `invprevw`, `invnextp`, `invprevp`, `invuse`, `invdrop`) and the player's commands
//   (server/sequel_commands.hpp: `use <item>`, `drop <item>`, `weapnext`, `weapprev`,
//   `weaplast`, `wave <n>`, `notarget`, `players`, `playerlist`). They go to the sequel's rules
//   only; in another game they do nothing, so a key the sequel's own config binds to one (TAB to
//   `inven`, 1 to `use Blaster`, H to `wave 0`) costs the first game nothing. The help computer
//   is `cmd help` (the console's `help` explains its words).
//
// `add_server_commands` adds these and the administrator's words (classic_admin.hpp).
//
// ge:summary The game's own commands from the window's console: cmd, the sequel's screen words
// ge:keywords cmd client-command forward score inven putaway invnext invprev invuse invdrop
// ge:keywords sequel game-commands console-words use drop weapnext weapprev weaplast wave give
// ge:keywords god noclip notarget kill playerlist
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

// The words every game's rules answer (above).
inline constexpr const char* kGameWords[] = {"kill", "god", "give", "noclip"};

// The sequel's words (above), in the order `add_server_commands` adds them.
inline constexpr const char* kSequelWords[] = {
    "score",    "inven",    "putaway", "invnext",  "invprev", "invnextw",  "invprevw",
    "invnextp", "invprevp", "invuse",  "invdrop",  "use",     "drop",      "weapnext",
    "weapprev", "weaplast", "wave",    "notarget", "players", "playerlist"};

// `line` to the game played as the player's command: to the server `r` joined, else to the match
// `l` hosts; false when there is neither.
bool send_game_command(client::Listen& l, Remote& r, const std::string& line);

// `cmd`, the games' words, the sequel's and the administrator's (classic_admin.hpp) on `con`.
void add_server_commands(ge::console::Console& con, client::Listen& l, Remote& r);

}  // namespace shooters_edge::net
