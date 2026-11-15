// The game's own commands from the window's console (game_commands.hpp).
//
// ge:summary The game's own commands from the window's console: cmd, the sequel's screen words
// ge:keywords cmd client-command send_game_command add_server_commands
// ge:skill shooter console
// ge:test tests/shooters_edge_game_commands_test.cpp
#include "shooters_edge/net/game_commands.hpp"

#include "console/console.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_admin.hpp"
#include "shooters_edge/net/classic_remote.hpp"

namespace shooters_edge::net {

bool send_game_command(client::Listen& l, Remote& r, const std::string& line) {
  if (r.active) {
    client_command(r.end, line);
    return true;
  }
  return client::listen_say(l, line);
}

void add_server_commands(ge::console::Console& con, client::Listen& l, Remote& r) {
  using Con = ge::console::Console;
  using Args = ge::console::Args;
  add_admin_commands(con, l, r);
  con.add_command("cmd", "cmd <line>: the line to the game's rules as your own command",
                  [&l, &r](Con& c, const Args& a) {
                    const std::string line = a.rest(0);
                    if (line.empty())
                      c.print("cmd: type the command to send after it.");
                    else if (!send_game_command(l, r, line))
                      c.print("You need to be in a game for that.");
                  });
  for (const char* word : kSequelWords)
    con.add_command(word,
                    std::string(word) + ": the sequel's " +
                        (std::string(word) == "score" ? "scoreboard" : "inventory") +
                        " (nothing in another game)",
                    [&l](Con&, const Args& a) {
                      if (client::listen_is_sequel(l)) client::listen_say(l, a[0]);
                    });
}

}  // namespace shooters_edge::net
