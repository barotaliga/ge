// engine/server/spawn: launching a dedicated server. The arguments it is started with (the cfg
// handed over as the last word, a password never shown); the records kept in the state folder;
// and the real thing on the sample game's server: started with a cfg, answering a ping with the
// cfg's name, found again from the record alone, a second start on its port refused, and stopped
// with its record gone.
//
// ge:summary Tests launching a dedicated server: argv, records, start, ping, find, stop
// ge:keywords test spawn launch dedicated-server hosted record ping stop gesample
// ge:skill net testing
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "check.hpp"
#include "server/process.hpp"
#include "server/sample.hpp"
#include "server/spawn.hpp"

namespace fs = std::filesystem;
namespace gs = ge::server;

namespace {

fs::path fresh(const std::string& name) {
  const fs::path p = fs::temp_directory_path() / name;
  std::error_code ec;
  fs::remove_all(p, ec);
  fs::create_directories(p);
  return p;
}

// The sample game's dedicated server, built beside the tests' folder (`make build` makes it);
// "" when this build has none.
std::string sample_server() {
  const fs::path dir = fs::path(gs::self_dir(nullptr)).parent_path();
  const std::string p = gs::program_path(dir.string(), "gesample");
  return fs::exists(p) ? p : std::string();
}

}  // namespace

TEST(server_spawn_argv_hands_the_cfg_last_and_hides_the_password) {
  gs::SpawnAsk ask;
  ask.program = "/opt/game/gesample";
  ask.root = "/opt/game";
  ask.state = "/tmp/state";
  ask.port = 27650;
  ask.mods = {"arena", "ctf"};
  ask.name = "Friday Night";
  ask.password = "hunter2";
  ask.bots = 2;
  ask.cfg = "sv_motd \"hi\"\n";
  const std::vector<std::string> argv = gs::spawn_argv(ask);
  CHECK(argv.front() == ask.program);
  CHECK(argv[argv.size() - 2] == "+exec");
  CHECK(argv.back().find("server-27650.cfg") != std::string::npos);
  bool console = false;
  for (const std::string& a : argv)
    console = console || (a.rfind("--commands=", 0) == 0 &&
                          a.find("console-27650.txt") != std::string::npos);
  CHECK(console);
  const std::string shown = gs::shown_command(argv, ask.password);
  CHECK_MSG(shown.find("--password=***") != std::string::npos, shown);
  CHECK(shown.find("hunter2") == std::string::npos);
  CHECK(shown.find("\"--name=Friday Night\"") != std::string::npos);
  CHECK(shown.find("--mod=arena,ctf") != std::string::npos);
}

TEST(server_spawn_records_are_kept_listed_and_forgotten) {
  const fs::path state = fresh("ge_server_spawn_records");
  gs::Hosted a{11, 27700, "dedicated", "a.log", 100, "gesample --port=27700"};
  gs::Hosted b{12, 27600, "listen", "b.log", 200, "game +host"};
  CHECK(gs::hosted_write(state.string(), a) && gs::hosted_write(state.string(), b));
  const std::vector<gs::Hosted> all = gs::hosted_list(state.string());
  CHECK(all.size() == 2 && all[0].port == 27600 && all[1].port == 27700);
  CHECK(all[1].command == a.command && all[1].log == "a.log" && all[1].started == 100);
  gs::hosted_forget(state.string(), 27600);
  CHECK(gs::hosted_list(state.string()).size() == 1);
  gs::Hosted none;
  gs::SpawnAsk bad;
  bad.port = 70000;
  bad.state = state.string();
  std::string error;
  CHECK(!gs::spawn_server(bad, none, error));
  CHECK_MSG(error.find("no port") != std::string::npos, error);
  fs::remove_all(state);
}

TEST(server_spawn_a_started_server_answers_is_found_again_and_stops) {
  const std::string program = sample_server();
  if (program.empty()) {
    std::printf("skipped: no gesample beside the tests (make build makes it)\n");
    return;
  }
  const fs::path state = fresh("ge_server_spawn_run");
  gs::SpawnAsk ask;
  ask.program = program;
  ask.root = state.string();
  ask.state = state.string();
  ask.port = 27651;
  ask.bots = 2;
  ask.cfg = "sv_hostname \"Launched Ring\"\n";
  gs::Hosted h;
  std::string error;
  CHECK_MSG(gs::spawn_server(ask, h, error), error);
  CHECK(h.pid > 0 && h.port == 27651 && h.kind == "dedicated");
  ge::net::Advert advert;
  const uint32_t protocol = gs::sample_game().protocol;
  CHECK_MSG(gs::server_answers(ask.port, protocol, 20000, &advert),
            "no answer on port 27651; its log: " + h.log);
  CHECK_MSG(std::strcmp(advert.name, "Launched Ring") == 0, advert.name);
  // A second program finds it from the record alone.
  const std::vector<gs::Hosted> found = gs::hosted_list(state.string());
  CHECK(found.size() == 1 && found[0].pid == h.pid && gs::hosted_alive(found[0]));
  // A second start on the port is refused while it runs.
  gs::Hosted again;
  CHECK(!gs::spawn_server(ask, again, error));
  CHECK_MSG(error.find("already runs on port 27651") != std::string::npos, error);
  // A console line by the record: the mods switched, the match restarted under them.
  fs::create_directories(state / "arena");
  {
    std::FILE* f = std::fopen((state / "arena" / "server.cfg").string().c_str(), "w");
    std::fputs("sv_maxclients 4\n", f);
    std::fclose(f);
  }
  CHECK_MSG(gs::hosted_command(state.string(), ask.port, "mod arena", error), error);
  bool switched = false;
  for (int i = 0; i < 40 && !switched; i++) {
    switched = gs::server_answers(ask.port, protocol, 500, &advert) && advert.max_players == 4;
  }
  CHECK_MSG(switched, "no switch to arena's 4 slots; its log: " + h.log);
  CHECK(!gs::hosted_command(state.string(), 27659, "mod arena", error));
  CHECK_MSG(gs::stop_hosted(state.string(), found[0], error), error);
  CHECK(!gs::process_alive(h.pid));
  CHECK(gs::hosted_list(state.string()).empty());
  CHECK(!gs::server_answers(ask.port, protocol, 600));
  fs::remove_all(state);
}
