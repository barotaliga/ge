// The game's own commands from the window's console (game_commands.hpp).
//
// ge:summary The game's own commands from the window's console: cmd, the sequel's screen words
// ge:keywords cmd client-command send_game_command add_server_commands
// ge:skill shooter console
// ge:test tests/shooters_edge_game_commands_test.cpp
#include "shooters_edge/net/game_commands.hpp"

#include <string_view>

#include "console/console.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_admin.hpp"
#include "shooters_edge/net/classic_remote.hpp"

namespace shooters_edge::net {

namespace {

// What each word does, as the console's `help` shows it after the word.
const char* game_word_help(std::string_view w) {
  if (w == "kill") return ": take your own life";
  if (w == "god") return ": no harm comes to you (where the server allows cheats)";
  if (w == "give") return " <what>: give yourself an item, all, weapons, ammo or health (cheats)";
  return ": fly through walls (where the server allows cheats)";
}

const char* sequel_word_help(std::string_view w) {
  if (w == "score") return ": the sequel's scoreboard (nothing in another game)";
  if (w == "use") return " <item>: use an item or take up a weapon in the sequel";
  if (w == "drop") return " <item>: drop an item or a weapon in the sequel";
  if (w == "weapnext" || w == "weapprev") return ": the next or last weapon held in the sequel";
  if (w == "weaplast") return ": the weapon held before this one in the sequel";
  if (w == "wave") return " <0-4>: a gesture in the sequel (flip off, salute, taunt, wave, point)";
  if (w == "notarget") return ": the sequel's monsters do not see you (cheats)";
  if (w == "players" || w == "playerlist") return ": the sequel's players and their scores";
  return ": the sequel's inventory (nothing in another game)";
}

}  // namespace

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
  for (const char* word : kGameWords)
    con.add_command(word, std::string(word) + game_word_help(word),
                    [&l, &r](Con& c, const Args& a) {
                      if (!send_game_command(l, r, a.raw))
                        c.print("You need to be in a game for that.");
                    });
  for (const char* word : kSequelWords)
    con.add_command(word, std::string(word) + sequel_word_help(word), [&l](Con&, const Args& a) {
      if (client::listen_is_sequel(l)) client::listen_say(l, a.raw);
    });
}

}  // namespace shooters_edge::net
