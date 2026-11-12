// The game CLI's frame and its commands on Shooter's Edge's Spec: help, the catalog and the skill
// cover every row, refusals keep the answer's shape, the browser, one server's players and the
// remote console against a stand-in server on the loopback, hosting and joining as dry runs, a
// started program asked after and stopped, a hosted server's record, the logs, the demos, and
// classic enhance's content refusing a folder with no packs.
//
// ge:summary Tests the game CLI: catalog, skill, refusals, servers, players, rcon, host, join
// ge:keywords test gamecli shooters_edge_cli help catalog skill servers players rcon host join
// ge:keywords spawn_detached process_alive process_stop demos content logs demos-pack ged
// ge:skill agent-interface testing
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>

#include "check.hpp"
#include "core/json_read.hpp"
#include "gamecli/gamecli.hpp"
#include "gamecli/process.hpp"
#include "net/udp_socket.hpp"
#include "shooters_edge/cli/spec.hpp"
#include "shooters_edge/client/demo_write.hpp"

namespace fs = std::filesystem;
namespace gc = ge::gamecli;
namespace gn = ge::net;

namespace {

fs::path fresh(const char* name) {
  const fs::path p = fs::temp_directory_path() / name;
  std::error_code ec;
  fs::remove_all(p, ec);
  fs::create_directories(p, ec);
  return p;
}

const gc::Spec& se() { return shooters_edge::cli::spec(); }

// Every call names its own install and its own state and logs, never the machine's.
int run(std::vector<std::string> args, std::string& out, const fs::path& root) {
  args.insert(args.begin(), {"--basedir=" + root.string(), "--state=" + (root / "state").string(),
                             "--logs=" + (root / "logs").string()});
  return gc::run(se(), args, out, (root / "bin").string());
}

ge::core::FlatJson parse(const std::string& out) {
  ge::core::FlatJson j;
  std::string error;
  if (!ge::core::read_flat(out, j, error, true)) ge::test::fail(__FILE__, __LINE__, error + out);
  return j;
}

// A server on the loopback answering the status query and the remote console as a classic
// server does, for `seconds`.
struct StandIn {
  gn::UdpSocket sock;
  std::atomic<bool> stop{false};
  std::thread thread;
  bool open() {
    std::string error;
    if (!gn::udp_open(sock, gn::loopback_v4(0), &error)) return false;
    thread = std::thread([this] {
      uint8_t buf[1500];
      while (!stop) {
        gn::Address from;
        const int n = gn::udp_receive(sock, buf, sizeof buf, from);
        if (n <= 4) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
          continue;
        }
        const std::string text(reinterpret_cast<char*>(buf) + 4, static_cast<size_t>(n - 4));
        std::string reply = "\xff\xff\xff\xffn";
        if (text.rfind("status", 0) == 0) {
          reply +=
              "\\hostname\\Friday frags\\map\\e1m2\\maxclients\\8\\deathmatch\\1\n"
              "3 12 5 40 \"Ann\" \"base\" 4 4 \"red\"\n"
              "4 7 3 55 \"Bo\" \"base\" 13 13 \"blue\"\n";
        } else if (text.rfind("rcon secret ", 0) == 0) {
          reply += "ran: " + std::string(text.c_str() + 12) + "\n";
        } else if (text.rfind("rcon ", 0) == 0) {
          reply += "Bad rcon_password.\n";
        } else {
          continue;
        }
        gn::udp_send(sock, from, reinterpret_cast<const uint8_t*>(reply.data()), reply.size());
      }
    });
    return true;
  }
  std::string address() const { return "127.0.0.1:" + std::to_string(sock.local.port); }
  ~StandIn() {
    stop = true;
    if (thread.joinable()) thread.join();
    gn::udp_close(sock);
  }
};

}  // namespace

TEST(help_catalog_and_skill_cover_every_row) {
  const fs::path root = fresh("ge_gamecli_help");
  std::string out;
  CHECK(run({"help", "--json"}, out, root) == 0);
  const ge::core::FlatJson j = parse(out);
  std::vector<std::string> names;
  for (int i = 0; j.count("result.commands." + std::to_string(i) + ".name") != 0; i++)
    names.push_back(j.at("result.commands." + std::to_string(i) + ".name").text);
  for (const char* want : {"settings set", "servers", "players", "rcon", "host", "host stop",
                           "join", "logs", "content", "demos play", "demos pack", "demos unpack"})
    CHECK_MSG(std::find(names.begin(), names.end(), want) != names.end(), want);
  CHECK(run({"skill"}, out, root) == 0);
  CHECK(out.rfind("---\nname: shooters-edge\n", 0) == 0);
  for (const std::string& n : names)
    CHECK_MSG(out.find("`shooters_edge_cli " + n) != std::string::npos, "skill lacks " + n);
  CHECK(run({"help", "servers"}, out, root) == 0);
  CHECK(out.find("--has=<text>") != std::string::npos);
  fs::remove_all(root);
}

TEST(refusals_keep_the_answer_shape) {
  const fs::path root = fresh("ge_gamecli_refuse");
  std::string out;
  CHECK(run({"fly", "--json"}, out, root) == 2);
  ge::core::FlatJson j = parse(out);
  CHECK(j["ok"].boolean == false && j["exit"].text == "2");
  CHECK(j["next"].text == "shooters_edge_cli help");
  CHECK(run({"servers", "--colour=red", "--json"}, out, root) == 2);
  j = parse(out);
  CHECK(j["error"].text.find("--colour") != std::string::npos);
  CHECK(run({"players"}, out, root) == 2);
  fs::remove_all(root);
}

TEST(servers_players_and_rcon_on_the_loopback) {
  const fs::path root = fresh("ge_gamecli_servers");
  StandIn s;
  if (!s.open()) {
    ge::test::skip("no loopback socket here");
    return;
  }
  std::string out;
  CHECK(run({"servers", "--lan=0", "--server=" + s.address(), "--wait=3", "--json"}, out, root) ==
        0);
  ge::core::FlatJson j = parse(out);
  CHECK_MSG(j["result.servers.0.name"].text == "Friday frags", out);
  CHECK(j["result.servers.0.map"].text == "e1m2");
  CHECK(j["result.servers.0.mode"].text == "ffa");
  CHECK(j["result.servers.0.players"].text == "2");
  CHECK(j["result.servers.0.from"].text == "named");
  // A filter that passes none.
  CHECK(run({"servers", "--lan=0", "--server=" + s.address(), "--has=ctf", "--json"}, out, root) ==
        0);
  CHECK(parse(out).count("result.servers.0.name") == 0);
  CHECK(run({"players", s.address(), "--json"}, out, root) == 0);
  j = parse(out);
  CHECK(j["result.players.0.name"].text == "Ann" && j["result.players.0.frags"].text == "12");
  CHECK(j["result.players.1.team"].text == "blue");
  CHECK(run({"server", "info", s.address(), "--json"}, out, root) == 0);
  CHECK(parse(out)["result.keys.deathmatch"].text == "1");
  CHECK(run({"--json", "rcon", s.address(), "--password=secret", "--", "kick", "Bo"}, out, root) ==
        0);
  CHECK_MSG(parse(out)["result.reply"].text == "ran: kick Bo\n", out);
  CHECK(run({"rcon", s.address(), "status"}, out, root) == 1);  // no password anywhere
  // A server that does not answer is said with the next step.
  CHECK(run({"players", "127.0.0.1:1", "--wait=0.3", "--json"}, out, root) == 1);
  CHECK(parse(out)["error"].text.find("did not answer") != std::string::npos);
  fs::remove_all(root);
}

TEST(host_and_join_as_dry_runs) {
  const fs::path root = fresh("ge_gamecli_dry");
  std::string out;
  CHECK(run({"host", "--dedicated", "--map=dm4", "--name=Friday", "--password=pw", "--dry-run",
             "--json"},
            out, root) == 0);
  ge::core::FlatJson j = parse(out);
  const std::string cmd = j["result.command"].text;
  CHECK(cmd.find("shooters_edge_server") != std::string::npos);
  CHECK(cmd.find("--root=" + root.string()) != std::string::npos);
  CHECK(cmd.find("--map=dm4") != std::string::npos && cmd.find("--for=0") != std::string::npos);
  CHECK_MSG(cmd.find("pw") == std::string::npos && cmd.find("--password=***") != std::string::npos,
            cmd);
  CHECK(run({"host", "--map=e1m2", "--maxclients=4", "--dry-run", "--json"}, out, root) == 0);
  const std::string listen = parse(out)["result.command"].text;
  CHECK_MSG(listen.find("+sv deathmatch 1 +sv maxclients 4 +map e1m2") != std::string::npos,
            listen);
  CHECK(run({"join", "10.0.0.5:27500", "--dry-run", "--json"}, out, root) == 0);
  CHECK(parse(out)["result.command"].text.find("+connect 10.0.0.5:27500") != std::string::npos);
  CHECK(run({"host", "--port=70000"}, out, root) == 2);
  // A program that is not there is said, with the flag that points at it.
  CHECK(run({"join", "10.0.0.5"}, out, root) == 1);
  CHECK(out.find("--bin=") != std::string::npos);
  fs::remove_all(root);
}

#ifndef _WIN32
TEST(a_started_program_is_kept_listed_and_stopped) {
  const fs::path root = fresh("ge_gamecli_host");
  fs::create_directories(root / "bin");
  fs::create_directories(root / "logs");
  long pid = 0;
  std::string error;
  const std::string log = (root / "logs" / "sh.log").string();
  CHECK_MSG(
      gc::spawn_detached({"/bin/sh", "-c", "echo started; exec sleep 30"}, "", log, pid, error),
      error);
  CHECK(gc::process_alive(pid));
  for (int i = 0; i < 200 && fs::file_size(log) == 0; i++)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  CHECK(gc::process_stop(pid, 2000, error));
  CHECK(!gc::process_alive(pid));
  {
    std::ifstream f(log);
    std::string line;
    std::getline(f, line);
    CHECK(line == "started");
  }
  // A stand-in for the dedicated server: it runs and never answers, so `host` says it started
  // but does not answer yet, `host list` sees it run and `host stop` ends it.
  const fs::path server = root / "bin" / "shooters_edge_server";
  std::ofstream(server) << "#!/bin/sh\necho \"serving $*\"\nexec sleep 600\n";
  fs::permissions(server, fs::perms::owner_all);
  std::string out;
  CHECK_MSG(run({"host", "--dedicated", "--port=27931", "--json"}, out, root) == 0, out);
  ge::core::FlatJson j = parse(out);
  CHECK(j["result.answering"].boolean == false);
  CHECK(run({"host", "--dedicated", "--port=27931"}, out, root) == 1);  // the port is taken
  CHECK(run({"host", "list", "--json"}, out, root) == 0);
  j = parse(out);
  CHECK(j["result.servers.0.port"].text == "27931" && j["result.servers.0.running"].boolean);
  CHECK(run({"logs", "--json"}, out, root) == 0);
  CHECK(parse(out)["result.logs.0.name"].text == "dedicated-27931");
  CHECK(run({"logs", "dedicated-27931", "--json"}, out, root) == 0);
  CHECK(parse(out)["result.text"].text.find("serving --root=") != std::string::npos);
  CHECK(run({"host", "stop", "27931"}, out, root) == 0);
  CHECK(run({"host", "list", "--json"}, out, root) == 0);
  CHECK(parse(out).count("result.servers.0.port") == 0);
  fs::remove_all(root);
}
#endif

TEST(where_demos_and_content) {
  const fs::path root = fresh("ge_gamecli_where");
  fs::create_directories(root / "id1" / "demos");
  std::ofstream(root / "id1" / "final.mvd") << "x";
  std::ofstream(root / "id1" / "demos" / "duel.mvdz") << "xy";
  std::ofstream(root / "id1" / "notes.txt") << "no";
  std::string out;
  CHECK(run({"where", "--json"}, out, root) == 0);
  ge::core::FlatJson j = parse(out);
  CHECK(j["result.settings"].text.find("settings") != std::string::npos);
  CHECK(j["result.catalog"].boolean == false);
  CHECK(run({"demos", "--json"}, out, root) == 0);
  j = parse(out);
  CHECK(j["result.demos.0.name"].text == "duel" || j["result.demos.0.name"].text == "final");
  CHECK(j.count("result.demos.1.name") == 1 && j.count("result.demos.2.name") == 0);
  CHECK(run({"demos", "play", "nothing"}, out, root) == 1);
  // No packs in the folder: refused with the next step.
  CHECK(run({"content", "--json"}, out, root) == 1);
  CHECK(parse(out)["ok"].boolean == false);
  fs::remove_all(root);
}

TEST(demos_pack_and_unpack_a_replay_file) {
  const fs::path root = fresh("ge_gamecli_pack");
  fs::create_directories(root / "id1");
  namespace sc = shooters_edge::client;
  const auto rec = sc::write_multiview_demo(sc::loop_script("halls", {64, 32, 24}, 160, 600));
  std::ofstream(root / "id1" / "final.mvd", std::ios::binary)
      .write(reinterpret_cast<const char*>(rec.data()), static_cast<std::streamsize>(rec.size()));
  std::string out;
  CHECK_MSG(run({"demos", "pack", "final", "--json"}, out, root) == 0, out);
  ge::core::FlatJson j = parse(out);
  CHECK(j["result.format"].text == "multiview");
  CHECK(fs::exists(root / "id1" / "final.ged"));
  CHECK(run({"demos", "pack", "final"}, out, root) == 1);  // final.ged is already there
  fs::rename(root / "id1" / "final.mvd", root / "id1" / "kept.mvd");
  CHECK_MSG(run({"demos", "unpack", "final", "--json"}, out, root) == 0, out);
  {
    // Closed before the folder is removed: Windows refuses to delete a file that is open.
    std::ifstream in(root / "id1" / "final.mvd", std::ios::binary);
    const std::vector<uint8_t> back((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
    CHECK(back == rec);
  }
  CHECK(run({"demos", "unpack", "kept"}, out, root) == 1);  // not a replay file
  fs::remove_all(root);
}
