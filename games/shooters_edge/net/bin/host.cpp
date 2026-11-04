// `shooters_edge_host`: a game others join. The baseline deathmatch mod on a map from your game
// folder (or the built-in arena), served over the classic protocol on this machine's UDP port
// (27500 by default), so `shooters_edge` windows on this machine or the network join it with
// `connect <this machine's address>` (net/classic_remote.hpp). Headless: it opens no window.
//
//   shooters_edge_host [-basedir <dir>] [-game <folder>] [-port <n>] [+map <name>] [+<server line>
//   ...]
//
// The `+` lines after the first run on the mod server's console (`+fraglimit 20`, `+timelimit 10`,
// `+deathmatch 3`). It prints a status line when a player joins or leaves; Ctrl-C stops it.
//
// NOT YET: the map does not change at the mod's limits (the match plays on); bots do not fill the
// empty slots; the dedicated server of the engine (`shooters_edge_server`) takes this over when its
// game speaks the classic protocol.
//
// ge:summary The host a window joins: the baseline mod on a map, served over UDP, headless
// ge:keywords host shooters_edge_host serve lan-game classic-server udp port join headless
// ge:skill net shooter
// ge:test tests/shooters_edge_remote_test.cpp
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "core/output.hpp"
#include "net/transport.hpp"
#include "shooters_edge/client/args.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/server/commands.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace cn = shooters_edge::net;
namespace gn = ge::net;
namespace sv = shooters_edge::server;
namespace out = ge::core::output;

int usage(const std::string& why) {
  std::fprintf(
      stderr,
      "shooters_edge_host: %s\nusage: shooters_edge_host [-basedir <dir>] [-game <folder>] "
      "[-port <n>] [+map <name>] [+<server line> ...]\n",
      why.c_str());
  return 2;
}

int run(int argc, char** argv) {
  out::set_output_sink([](const out::LogLine& l, void*) {
    std::printf("%s\n", l.message.c_str());
    std::fflush(stdout);  // a line at a time, also into a pipe or a log file
  });
  cl::Args a;
  int port = cn::kPortServer;
  std::vector<char*> rest{argv[0]};  // the window's own flags, -port taken out
  for (int i = 1; i < argc; i++) {
    const std::string_view w = argv[i];
    if ((w == "-port" || w == "--port") && i + 1 < argc) {
      port = std::atoi(argv[++i]);
      if (port <= 0 || port > 65535)
        return usage("the port " + std::string(argv[i]) + " is not 1..65535");
    } else {
      rest.push_back(argv[i]);
    }
  }
  std::string error;
  if (!cl::parse(static_cast<int>(rest.size()), rest.data(), a, error)) return usage(error);
  // `+map <name>` names the map; every other + line is the mod server's.
  std::string map_name;
  std::vector<std::string> lines;
  for (size_t i = 0; i < a.plus.size(); i++) {
    std::string line = a.plus[i].substr(a.plus[i][0] == '+' ? 1 : 0);
    while (i + 1 < a.plus.size() && a.plus[i + 1][0] != '+') line += " " + a.plus[++i];
    if (line.rfind("map ", 0) == 0)
      map_name = line.substr(4);
    else
      lines.push_back(line);
  }
  cl::SearchPath files;
  if (!cl::open_game_dir(a.basedir, a.game, files, error)) {
    if (!map_name.empty() && map_name != "arena")
      return usage("the game folder did not open: " + error);
    out::warn("shooters_edge_host: no game folder, so the built-in arena: " + error +
              "; next: give the folder holding the packs with -basedir <dir>");
  }
  if (map_name.empty()) map_name = cl::archive_paths(files).empty() ? "arena" : "dm6";
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, error)) return usage("the baseline mod did not build: " + error);
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, map_name, error);
  if (!map) return usage(error);
  sv::QcServerOptions so;
  so.loop.max_edicts = 1024;
  std::unique_ptr<sv::QcServer> qc = sv::make_qc_server(progs, so, error);
  if (!qc) return usage("the mod server did not start: " + error);
  sv::server_command(*qc, "deathmatch 1");
  for (const std::string& line : lines) sv::server_command(*qc, line);
  if (!sv::load_map(*qc, map->spec, error)) return usage("the map " + map_name + ": " + error);
  if (map->from_file) sv::use_map_sets(*qc, map->bsp);

  gn::Transport t;
  if (!gn::open_udp_transport(t, gn::ipv4(0, 0, 0, 0, static_cast<uint16_t>(port)), &error))
    return usage("the port " + std::to_string(port) + " did not open: " + error +
                 "; next: another -port, or stop what holds it");
  cn::ServerConfig cfg;
  cfg.gamedir = a.game;
  cn::ClassicServer server;
  if (!cn::classic_start(server, *qc, t, cfg, error)) return usage("the server: " + error);
  out::status("shooters_edge_host: " + map_name + " on UDP port " + std::to_string(port) +
              "; join with `connect <this machine's address>:" + std::to_string(port) +
              "` in a shooters_edge window");

  using clock = std::chrono::steady_clock;
  const clock::time_point start = clock::now();
  int64_t due_us = 0;
  int players = 0;
  for (;;) {
    const int64_t now_us =
        std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - start).count();
    if (now_us < due_us) {
      std::this_thread::sleep_for(std::chrono::microseconds(due_us - now_us));
      continue;
    }
    due_us += 13000;
    if (now_us - due_us > 1000000) due_us = now_us;  // a long stall: dropped, not paid down
    cn::classic_frame(server, due_us);
    if (!qc->pending_map.empty()) {
      out::status("shooters_edge_host: the mod asked for " + qc->pending_map +
                  "; map changes are not served yet, so " + map_name + " plays on");
      qc->pending_map.clear();
    }
    const int now_players = cn::classic_count(server, cn::ConnState::spawned);
    if (now_players != players) {
      players = now_players;
      out::status("shooters_edge_host: " + std::to_string(players) + " playing");
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  try {
    return run(argc, argv);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "shooters_edge_host: stopped by an uncaught exception: %s\n", e.what());
  }
  return 1;
}
