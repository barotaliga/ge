// The host page's dedicated server, defined (host_dedicated.hpp).
//
// ge:summary The host page's dedicated server, defined: the ask, start or join, stop
// ge:keywords hostdedicated stopdedicated dedicated_ask add_dedicated_commands
// ge:skill net ui
// ge:test tests/shooters_edge_host_dedicated_test.cpp
#include "shooters_edge/client/host_dedicated.hpp"

#include <filesystem>

#include "server/process.hpp"

namespace shooters_edge::client {

namespace gs = ge::server;

gs::SpawnAsk dedicated_ask(const DedicatedHost& host, const std::string& map,
                           const std::vector<std::pair<std::string, std::string>>& settings) {
  gs::SpawnAsk ask;
  ask.program = host.program;
  ask.root = host.root;
  ask.state = host.state;
  ask.logs = host.logs;
  ask.port = host.port;
  ask.map = map;
  if (host.game != 1) ask.extra = {"-game", std::to_string(host.game)};
  for (const auto& [name, value] : settings)
    ask.cfg.append(name).append(" \"").append(value).append("\"\n");
  return ask;
}

void add_window_dedicated(ge::console::Console& con, const std::string& prefs,
                          const std::string& root, std::string_view game_id) {
  if (prefs.empty()) return;
  DedicatedHost host;
  host.program = gs::program_path(gs::self_dir(nullptr), "sesv");
  host.root = root;
  host.state = (std::filesystem::path(prefs) / "cli").string();
  host.logs = (std::filesystem::path(prefs) / "logs").string();
  host.game = game_id == "q2" ? 2 : 1;
  add_dedicated_commands(con, host);
}

void add_dedicated_commands(ge::console::Console& con, const DedicatedHost& host) {
  const std::string here = "127.0.0.1:" + std::to_string(host.port);
  con.add_command(
      "hostdedicated",
      "hostdedicated <map> [<setting> <value>]...: start this game's own server program on this "
      "machine with those rules, and join it",
      [host, here](ge::console::Console& c, const ge::console::Args& a) {
        if (a.size() < 2 || a.size() % 2 != 0) {
          c.print("Name a map, then each rule and its value.");
          return;
        }
        gs::Hosted was;
        if (gs::hosted_read(gs::hosted_path(host.state, host.port), was) && gs::hosted_alive(was)) {
          c.print("Your server is already running on port " + std::to_string(host.port) +
                  ": joining it. Stop it from the host page to start another.");
          c.insert("connect " + here + "\n");
          return;
        }
        if (!std::filesystem::exists(host.program)) {
          c.print(
              "The dedicated server is not installed beside the game, so the match is hosted "
              "here instead.");
          c.insert("map " + a[1] + "\n");
          return;
        }
        std::vector<std::pair<std::string, std::string>> rules;
        for (size_t i = 2; i + 1 < a.size(); i += 2) rules.emplace_back(a[i], a[i + 1]);
        gs::Hosted h;
        std::string why;
        if (!gs::spawn_server(dedicated_ask(host, a[1], rules), h, why)) {
          c.print("The server could not be started: " + why + ".");
          return;
        }
        c.print("Started your server on port " + std::to_string(host.port) +
                ". It keeps running when you quit; friends join this machine on that port.");
        c.insert("connect " + here + "\n");
      });
  con.add_command(
      "stopdedicated", "stopdedicated: stop the server this machine started from the host page",
      [host](ge::console::Console& c, const ge::console::Args&) {
        gs::Hosted h;
        if (!gs::hosted_read(gs::hosted_path(host.state, host.port), h) || !gs::hosted_alive(h)) {
          c.print("No server you started is running.");
          return;
        }
        std::string why;
        if (!gs::stop_hosted(host.state, h, why)) {
          c.print("The server did not stop: " + why + ".");
          return;
        }
        c.print("Stopped your server on port " + std::to_string(host.port) + ".");
      });
}

}  // namespace shooters_edge::client
