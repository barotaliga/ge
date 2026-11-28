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
  // Quake II's `help` alone is the help computer, a word its rules answer: with a Quake II game
  // up it goes there; `help <name>` stays the console's.
  if (const auto it = con.commands().find("help"); it != con.commands().end()) {
    const Con::Command ours = it->second;
    con.add_command(
        "help", ours.help + "; alone in Quake II, the help computer",
        [&l, &r, fn = ours.fn](Con& c, const Args& a) {
          const bool q2 = r.active ? r.q2 != nullptr : client::listen_is_q2(l);
          if (a.size() == 1 && q2 && send_game_command(l, r, "help")) return;
          fn(c, a);
        },
        ours.flags);
  }
  // Quake II's words, to its rules: the server joined's, else the window's own game's.
  for (const char* word : kQ2Words) {
    const std::string w(word);
    const char* what = w == "score"                         ? "scoreboard"
                       : w == "weapnext" || w == "weapprev" ? "next or previous weapon"
                                                            : "inventory";
    con.add_command(word, w + ": Quake II's " + what + " (nothing in another game)",
                    [&l, &r](Con&, const Args& a) {
                      const bool q2 = r.active ? r.q2 != nullptr : client::listen_is_q2(l);
                      if (q2) send_game_command(l, r, a[0]);
                    });
  }
}

}  // namespace shooters_edge::net
