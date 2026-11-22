// The game's own commands from the window's console (game_commands.hpp).
//
// ge:summary The game's own commands from the window's console: cmd, Quake II's screen words
// ge:keywords cmd client-command send_game_command add_server_commands
// ge:skill shooter console
// ge:test tests/shooters_edge_game_commands_test.cpp
#include "shooters_edge/net/game_commands.hpp"

#include "console/console.hpp"
#include "server/server.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/qw_admin.hpp"
#include "shooters_edge/net/qw_remote.hpp"

namespace shooters_edge::net {

bool send_game_command(client::Listen& l, Remote& r, const std::string& line) {
  if (r.active) {
    remote_command(r, line);
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
  // The server settings page's sheet (qw_remote_sheet.hpp): the window's own match server's read in
  // process, else the server joined's.
  r.sheet.local = [&l, &r](ge::server::SettingsSheet& out) {
    if (r.active || !l.match || l.match->game == nullptr || l.match->game->sheet == nullptr)
      return false;
    l.match->game->sheet(l.match->state, 0, out);
    return true;
  };
  con.add_command("serversheet",
                  "serversheet open|refresh|changed|close: the server settings page's sheet asked "
                  "for, kept, asked again after a change, or let go",
                  [&r](Con&, const Args& a) {
                    remote_sheet_command(r, a.size() > 1 ? a[1] : std::string("refresh"),
                                         static_cast<double>(r.now_us) * 1e-6 + r.owed_s);
                  });
  for (const char* word : kQ2Words)
    con.add_command(word,
                    std::string(word) + ": Quake II's " +
                        (std::string(word) == "score" ? "scoreboard" : "inventory") +
                        " (nothing in another game)",
                    [&l](Con&, const Args& a) {
                      if (client::listen_is_q2(l)) client::listen_say(l, a[0]);
                    });
}

}  // namespace shooters_edge::net
