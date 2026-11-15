// The host page's dedicated server (games/shooters_edge/client/host_dedicated.hpp): the ask the
// page's rules make, a missing server program falling back to hosting in the window, a stand-in
// program started, joined, found again by a second start and stopped, all in a player's words.
//
// ge:summary Tests the host page's dedicated server: the ask, the fallback, start, join and stop
// ge:keywords test hostdedicated stopdedicated dedicated_ask host-page sesv stand-in
// ge:skill net testing
#include <filesystem>
#include <fstream>
#include <string>

#include "check.hpp"
#include "console/console.hpp"
#include "server/process.hpp"
#include "server/spawn.hpp"
#include "shooters_edge/client/host_dedicated.hpp"

namespace fs = std::filesystem;
namespace cl = shooters_edge::client;

namespace {

bool printed(const ge::console::Console& con, const std::string& text) {
  for (const std::string& line : con.output())
    if (line.find(text) != std::string::npos) return true;
  return false;
}

}  // namespace

TEST(the_host_pages_rules_are_handed_over_as_a_cfg) {
  cl::DedicatedHost h{"/opt/se/sesv", "/games/install", "/prefs/cli", "/prefs/logs", 2, 27500};
  const ge::server::SpawnAsk ask =
      cl::dedicated_ask(h, "q2dm1", {{"fraglimit", "20"}, {"maxclients", "8"}});
  CHECK(ask.map == "q2dm1" && ask.root == "/games/install" && ask.state == "/prefs/cli");
  CHECK(ask.extra.size() == 2 && ask.extra[0] == "-game" && ask.extra[1] == "2");
  CHECK_MSG(ask.cfg == "fraglimit \"20\"\nmaxclients \"8\"\n", ask.cfg);
  h.game = 1;
  CHECK(cl::dedicated_ask(h, "dm4", {}).extra.empty());
}

TEST(no_server_program_hosts_in_the_window_instead) {
  const fs::path root = fs::temp_directory_path() / "ge_se_host_none";
  fs::remove_all(root);
  cl::DedicatedHost h{(root / "sesv").string(),
                      root.string(),
                      (root / "cli").string(),
                      (root / "logs").string(),
                      1,
                      27941};
  ge::console::Console con(false);
  std::string mapped;
  con.add_command("map", "map",
                  [&](ge::console::Console&, const ge::console::Args& a) { mapped = a[1]; });
  cl::add_dedicated_commands(con, h);
  con.execute("hostdedicated dm4 fraglimit 20");
  CHECK(printed(con, "not installed beside the game"));
  CHECK(mapped == "dm4");
  con.execute("hostdedicated dm4 fraglimit");
  CHECK(printed(con, "Name a map, then each rule and its value."));
  con.execute("stopdedicated");
  CHECK(printed(con, "No server you started is running."));
  fs::remove_all(root);
}

#ifndef _WIN32
TEST(a_started_server_is_joined_found_again_and_stopped) {
  const fs::path root = fs::temp_directory_path() / "ge_se_host_run";
  fs::remove_all(root);
  fs::create_directories(root);
  const fs::path program = root / "sesv";
  std::ofstream(program) << "#!/bin/sh\necho \"serving $*\"\nexec sleep 600\n";
  fs::permissions(program, fs::perms::owner_all);
  cl::DedicatedHost h{
      program.string(), root.string(), (root / "cli").string(), (root / "logs").string(), 1, 27942};
  ge::console::Console con(false);
  std::string joined;
  con.add_command("connect", "connect",
                  [&](ge::console::Console&, const ge::console::Args& a) { joined = a[1]; });
  cl::add_dedicated_commands(con, h);
  con.execute("hostdedicated dm4 fraglimit 20 timelimit 10");
  CHECK_MSG(printed(con, "Started your server on port 27942"),
            con.output().empty() ? "" : con.output().back());
  CHECK(joined == "127.0.0.1:27942");
  const std::vector<ge::server::Hosted> found = ge::server::hosted_list(h.state);
  CHECK(found.size() == 1 && ge::server::hosted_alive(found[0]));
  {
    std::ifstream f(fs::path(h.state) / "server-27942.cfg");
    std::string all((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    CHECK_MSG(all == "fraglimit \"20\"\ntimelimit \"10\"\n", all);
  }
  // A second NEW GAME finds it running and joins it.
  joined.clear();
  con.execute("hostdedicated dm2");
  CHECK(printed(con, "already running on port 27942: joining it"));
  CHECK(joined == "127.0.0.1:27942");
  con.execute("stopdedicated");
  CHECK(printed(con, "Stopped your server on port 27942."));
  CHECK(!ge::server::process_alive(found[0].pid));
  CHECK(ge::server::hosted_list(h.state).empty());
  fs::remove_all(root);
}
#endif
