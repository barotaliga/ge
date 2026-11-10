// The game CLI's programs (commands.hpp): a game hosted in the window or as a dedicated server
// process, kept in the CLI's state folder (`hosted-<port>.txt`: its process, port, kind and log)
// so `host list` and `host stop` find it from any later call; a friend's game joined; the logs of
// everything started; and `where`, the places and programs the CLI works with.
//
// A program is started detached (process.hpp) with its output in `<logs>/<kind>-<port>.log`
// (`join.log` for the window on a server); a dedicated server is then asked for its status once,
// so `host` answers only when it serves (or with the end of its log when it died).
//
// ge:summary The game CLI's programs: hosting, the hosted servers' records, joining, logs, where
// ge:keywords gamecli host host-list host-stop join logs where dedicated listen-server hosted
// ge:keywords record pid port log-tail programs
// ge:skill agent-interface net
// ge:test tests/gamecli_cli_test.cpp
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

#include "core/json_write.hpp"
#include "gamecli/commands.hpp"
#include "gamecli/process.hpp"
#include "net/transport.hpp"
#include "net/udp_socket.hpp"

namespace ge::gamecli {

namespace fs = std::filesystem;
namespace gn = ge::net;
using core::json_arr;
using core::json_bool;
using core::json_num;
using core::json_obj;
using core::json_str;

namespace {

struct Hosted {
  long pid = 0;
  int port = 0;
  std::string kind;  // dedicated, listen
  std::string log;
  long long started = 0;  // seconds since the epoch
  std::string command;
};

fs::path record_path(const Ctx& ctx, int port) {
  return fs::path(ctx.places.state) / ("hosted-" + std::to_string(port) + ".txt");
}

bool read_record(const fs::path& p, Hosted& h) {
  std::ifstream f(p);
  if (!f) return false;
  std::string key;
  while (f >> key) {
    std::string rest;
    std::getline(f, rest);
    if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
    if (key == "pid") h.pid = std::atol(rest.c_str());
    if (key == "port") h.port = std::atoi(rest.c_str());
    if (key == "kind") h.kind = rest;
    if (key == "log") h.log = rest;
    if (key == "started") h.started = std::atoll(rest.c_str());
    if (key == "command") h.command = rest;
  }
  return h.pid > 0 && h.port > 0;
}

bool write_record(const Ctx& ctx, const Hosted& h) {
  std::error_code ec;
  fs::create_directories(ctx.places.state, ec);
  std::ofstream f(record_path(ctx, h.port));
  f << "pid " << h.pid << "\nport " << h.port << "\nkind " << h.kind << "\nlog " << h.log
    << "\nstarted " << h.started << "\ncommand " << h.command << "\n";
  return static_cast<bool>(f);
}

std::vector<Hosted> records(const Ctx& ctx) {
  std::vector<Hosted> out;
  std::error_code ec;
  for (const auto& e : fs::directory_iterator(ctx.places.state, ec)) {
    const std::string n = e.path().filename().string();
    if (n.rfind("hosted-", 0) != 0) continue;
    Hosted h;
    if (read_record(e.path(), h)) out.push_back(h);
  }
  std::sort(out.begin(), out.end(),
            [](const Hosted& a, const Hosted& b) { return a.port < b.port; });
  return out;
}

std::string hosted_json(const Hosted& h, bool alive) {
  return json_obj({{"port", json_num(h.port)},
                   {"kind", json_str(h.kind)},
                   {"pid", json_num(h.pid)},
                   {"running", json_bool(alive)},
                   {"log", json_str(h.log)},
                   {"started", json_num(h.started)},
                   {"command", json_str(h.command)}});
}

std::string bin_dir(const Ctx& ctx) { return ctx.flag("bin", ctx.self_dir); }

std::string tail(const std::string& path, int lines) {
  std::ifstream f(path, std::ios::binary);
  std::vector<std::string> all;
  std::string l;
  while (f && std::getline(f, l)) all.push_back(l);
  std::string out;
  const size_t from =
      all.size() > static_cast<size_t>(lines) ? all.size() - static_cast<size_t>(lines) : 0;
  for (size_t i = from; i < all.size(); i++) out += all[i] + "\n";
  return out;
}

std::string joined(const std::vector<std::string>& argv) {
  std::string s;
  for (const std::string& a : argv) {
    const bool q = a.find_first_of(" \t\"") != std::string::npos || a.empty();
    s += (s.empty() ? "" : " ") + (q ? "\"" + a + "\"" : a);
  }
  return s;
}

// Whether a server answers its status query on this machine's `port` within `wait_ms`.
bool answers(const Ctx& ctx, int port, int wait_ms) {
  if (ctx.spec.query.status == nullptr) return true;
  gn::Transport t;
  if (!gn::open_udp_transport(t, gn::ipv4(0, 0, 0, 0, 0), nullptr)) return false;
  uint8_t q[256], buf[gn::kMaxDatagram];
  const size_t n = ctx.spec.query.status(q, sizeof q);
  const gn::Address to = gn::loopback_v4(static_cast<uint16_t>(port));
  // Timed by the clock, not by counting sleeps: a loaded machine sleeps longer than asked.
  const auto start = std::chrono::steady_clock::now();
  const auto elapsed_ms = [&] {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                 start)
        .count();
  };
  bool ok = false;
  for (long long sent_at = -400; elapsed_ms() < wait_ms && !ok;) {
    if (elapsed_ms() - sent_at >= 400) {
      gn::transport_send(t, to, q, n, 0);
      sent_at = elapsed_ms();
    }
    gn::Address from;
    for (int got; (got = gn::transport_receive(t, buf, sizeof buf, from, 0)) > 0;) {
      gn::ServerInfo info;
      if (from == to && ctx.spec.query.read_status(buf, static_cast<size_t>(got), info)) ok = true;
    }
    if (!ok) std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  gn::close_transport(t);
  return ok;
}

int int_flag(const Ctx& ctx, const char* name, int fallback) {
  const std::string v = ctx.flag(name);
  return v.empty() ? fallback : std::atoi(v.c_str());
}

}  // namespace

int cmd_host(Ctx& ctx) {
  if (!need_places(ctx)) return 1;
  HostAsk ask;
  ask.map = ctx.flag("map");
  ask.name = ctx.flag("name");
  ask.password = ctx.flag("password");
  ask.port = int_flag(ctx, "port", ctx.spec.default_port);
  ask.maxclients = int_flag(ctx, "maxclients", 0);
  ask.bots = int_flag(ctx, "bots", -1);
  if (ask.port <= 0 || ask.port > 65535)
    return refuse(
        ctx, 2, "--port=" + ctx.flag("port") + " is no port (1..65535)",
        std::string(ctx.spec.cli) + " host --port=" + std::to_string(ctx.spec.default_port));
  const bool dedicated = ctx.has("dedicated");
  const char* program = dedicated ? ctx.spec.server : ctx.spec.window;
  if (program[0] == '\0' || (!dedicated && ctx.spec.listen_words == nullptr))
    return refuse(ctx, 1,
                  std::string(ctx.spec.title) + " has no " +
                      (dedicated ? "dedicated server" : "listen server in its window"),
                  std::string(ctx.spec.cli) + " host" + (dedicated ? "" : " --dedicated"));
  Hosted was;
  if (read_record(record_path(ctx, ask.port), was) && process_alive(was.pid))
    return refuse(ctx, 1,
                  "this CLI already runs a server on port " + std::to_string(ask.port) +
                      " (process " + std::to_string(was.pid) + ")",
                  std::string(ctx.spec.cli) + " host stop " + std::to_string(ask.port) +
                      ", or another --port");
  std::vector<std::string> argv = {program_path(bin_dir(ctx), program)};
  if (dedicated) {
    argv.push_back("--root=" + ctx.places.install);
    argv.push_back("--port=" + std::to_string(ask.port));
    argv.push_back("--for=0");
    argv.push_back("--realtime");
    argv.push_back("--status=60");
    if (!ask.map.empty()) argv.push_back("--map=" + ask.map);
    if (!ask.name.empty()) argv.push_back("--name=" + ask.name);
    if (ask.maxclients > 0) argv.push_back("--maxclients=" + std::to_string(ask.maxclients));
    if (ask.bots >= 0) argv.push_back("--bots=" + std::to_string(ask.bots));
    if (!ask.password.empty()) argv.push_back("--password=" + ask.password);
  } else {
    ctx.spec.listen_words(ctx, ask, argv);
  }
  const std::string kind = dedicated ? "dedicated" : "listen";
  const std::string log =
      (fs::path(ctx.places.logs) / (kind + "-" + std::to_string(ask.port) + ".log")).string();
  // The password is the host's to share, never kept in the record or printed back.
  std::string shown = joined(argv);
  if (!ask.password.empty()) {
    const size_t at = shown.find("--password=" + ask.password);
    if (at != std::string::npos) shown.replace(at + 11, ask.password.size(), "***");
  }
  if (ctx.has("dry-run"))
    return answer(ctx,
                  json_obj({{"kind", json_str(kind)},
                            {"port", json_num(ask.port)},
                            {"command", json_str(shown)},
                            {"log", json_str(log)}}),
                  shown, "would start a " + kind + " server (--dry-run: nothing started)");
  if (!fs::exists(argv[0]))
    return refuse(ctx, 1, argv[0] + " is not there",
                  "build it, or pass --bin=<the folder holding " + std::string(program) + ">");
  std::error_code ec;
  fs::create_directories(ctx.places.logs, ec);
  long pid = 0;
  std::string error;
  if (!spawn_detached(argv, dedicated ? ctx.places.install : "", log, pid, error))
    return refuse(ctx, 1, error, "read " + log);
  Hosted h{pid, ask.port, kind, log, static_cast<long long>(std::time(nullptr)), shown};
  write_record(ctx, h);
  // A dedicated server is asked once whether it serves; one that died says why in its log.
  bool serving = true;
  if (dedicated) {
    serving = answers(ctx, ask.port, 10000);
    if (!serving && !process_alive(pid))
      return refuse(
          ctx, 1, "the server stopped as it started:\n" + tail(log, 12),
          "fix what its log says, then " + std::string(ctx.spec.cli) + " host --dedicated");
  }
  return answer(ctx,
                json_obj({{"kind", json_str(kind)},
                          {"port", json_num(ask.port)},
                          {"pid", json_num(pid)},
                          {"answering", json_bool(serving)},
                          {"log", json_str(log)},
                          {"command", json_str(shown)}}),
                kind + " server on port " + std::to_string(ask.port) + ", process " +
                    std::to_string(pid) + ", log " + log,
                serving
                    ? "friends join this machine's address on port " + std::to_string(ask.port) +
                          " (over the internet, with the port forwarded on the router)"
                    : "started, but not answering yet; " + std::string(ctx.spec.cli) + " logs " +
                          kind + "-" + std::to_string(ask.port) + " says why",
                std::string(ctx.spec.cli) + " host stop " + std::to_string(ask.port) + " ends it");
}

int cmd_host_list(Ctx& ctx) {
  if (!need_places(ctx)) return 1;
  std::vector<std::string> rows;
  std::string text;
  int running = 0;
  for (const Hosted& h : records(ctx)) {
    const bool alive = process_alive(h.pid);
    running += alive ? 1 : 0;
    rows.push_back(hosted_json(h, alive));
    text += std::to_string(h.port) + "  " + h.kind + "  " + (alive ? "running" : "stopped") +
            "  process " + std::to_string(h.pid) + "  " + h.log + "\n";
  }
  return answer(
      ctx, json_obj({{"servers", json_arr(rows)}}), text,
      std::to_string(running) + " of " + std::to_string(rows.size()) + " started servers run");
}

int cmd_host_stop(Ctx& ctx) {
  if (!need_places(ctx)) return 1;
  std::vector<Hosted> stop;
  if (ctx.has("all")) {
    stop = records(ctx);
  } else if (ctx.words.size() == 1) {
    Hosted h;
    if (!read_record(record_path(ctx, std::atoi(ctx.words[0].c_str())), h))
      return refuse(ctx, 1, "this CLI started no server on port " + ctx.words[0],
                    std::string(ctx.spec.cli) + " host list");
    stop.push_back(h);
  } else {
    return refuse(ctx, 2, "host stop takes a port, or --all",
                  std::string(ctx.spec.cli) + " host list");
  }
  std::vector<std::string> rows;
  std::string text;
  for (const Hosted& h : stop) {
    std::string error;
    if (!process_stop(h.pid, 3000, error)) return refuse(ctx, 1, error, "stop it by hand");
    std::error_code ec;
    fs::remove(record_path(ctx, h.port), ec);
    rows.push_back(json_num(h.port));
    text += "stopped the " + h.kind + " server on port " + std::to_string(h.port) + "\n";
  }
  return answer(ctx, json_obj({{"stopped", json_arr(rows)}}), text,
                std::to_string(rows.size()) + " stopped");
}

int cmd_join(Ctx& ctx) {
  if (ctx.words.size() != 1)
    return refuse(
        ctx, 2, "join takes one address, host[:port]",
        std::string(ctx.spec.cli) + " join 192.168.1.20:" + std::to_string(ctx.spec.default_port));
  if (ctx.spec.window[0] == '\0' || ctx.spec.join_words == nullptr)
    return refuse(ctx, 1, std::string(ctx.spec.title) + " has no window to join with",
                  std::string(ctx.spec.cli) + " help");
  if (!need_places(ctx)) return 1;
  std::vector<std::string> argv = {program_path(bin_dir(ctx), ctx.spec.window)};
  ctx.spec.join_words(ctx, ctx.words[0], argv);
  const std::string log = (fs::path(ctx.places.logs) / "join.log").string();
  const std::string shown = joined(argv);
  if (ctx.has("dry-run"))
    return answer(
        ctx, json_obj({{"address", json_str(ctx.words[0])}, {"command", json_str(shown)}}), shown,
        "would start the game on " + ctx.words[0] + " (--dry-run: nothing started)");
  if (!fs::exists(argv[0]))
    return refuse(
        ctx, 1, argv[0] + " is not there",
        "build it, or pass --bin=<the folder holding " + std::string(ctx.spec.window) + ">");
  std::error_code ec;
  fs::create_directories(ctx.places.logs, ec);
  long pid = 0;
  std::string error;
  if (!spawn_detached(argv, "", log, pid, error)) return refuse(ctx, 1, error, "read " + log);
  return answer(
      ctx,
      json_obj({{"address", json_str(ctx.words[0])},
                {"pid", json_num(pid)},
                {"log", json_str(log)},
                {"command", json_str(shown)}}),
      "the game is starting on " + ctx.words[0] + " (process " + std::to_string(pid) + ")", "",
      std::string(ctx.spec.cli) + " logs join says what it did");
}

int cmd_logs(Ctx& ctx) {
  if (!need_places(ctx)) return 1;
  const fs::path dir = ctx.places.logs;
  if (ctx.words.empty()) {
    std::vector<std::string> rows;
    std::string text;
    std::vector<fs::path> files;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec))
      if (e.path().extension() == ".log") files.push_back(e.path());
    std::sort(files.begin(), files.end());
    for (const fs::path& f : files) {
      const auto size = fs::file_size(f, ec);
      rows.push_back(json_obj({{"name", json_str(f.stem().string())},
                               {"file", json_str(f.string())},
                               {"bytes", json_num(static_cast<long long>(size))}}));
      text += f.stem().string() + "  " + std::to_string(size) + " bytes  " + f.string() + "\n";
    }
    return answer(ctx, json_obj({{"logs", json_arr(rows)}, {"folder", json_str(dir.string())}}),
                  text, std::to_string(rows.size()) + " logs in " + dir.string(),
                  rows.empty() ? "" : std::string(ctx.spec.cli) + " logs <name> prints its end");
  }
  std::string name = ctx.words[0];
  if (fs::path(name).extension() != ".log") name += ".log";
  const fs::path file = dir / name;
  if (!fs::exists(file))
    return refuse(ctx, 1, "no log " + file.string(), std::string(ctx.spec.cli) + " logs");
  const int lines = int_flag(ctx, "lines", 30);
  const std::string text = tail(file.string(), lines > 0 ? lines : 30);
  return answer(ctx, json_obj({{"file", json_str(file.string())}, {"text", json_str(text)}}), text,
                "");
}

int cmd_where(Ctx& ctx) {
  if (!need_places(ctx)) return 1;
  const Places& p = ctx.places;
  const auto exists = [](const std::string& f) { return !f.empty() && fs::exists(f); };
  const std::string window = ctx.spec.window[0] ? program_path(bin_dir(ctx), ctx.spec.window) : "";
  const std::string server = ctx.spec.server[0] ? program_path(bin_dir(ctx), ctx.spec.server) : "";
  const std::string config = (fs::path(p.settings) / "config.cfg").string();
  const std::string catalog = (fs::path(p.settings) / "cvars.txt").string();
  std::string next;
  if (!exists(catalog))
    next = "start " + std::string(ctx.spec.title) +
           " once and quit it: it writes the settings' "
           "catalog the CLI checks names and ranges by";
  return answer(ctx,
                json_obj({{"install", json_str(p.install)},
                          {"install_exists", json_bool(exists(p.install))},
                          {"settings", json_str(p.settings)},
                          {"config", json_bool(exists(config))},
                          {"catalog", json_bool(exists(catalog))},
                          {"logs", json_str(p.logs)},
                          {"state", json_str(p.state)},
                          {"window", json_str(window)},
                          {"window_exists", json_bool(exists(window))},
                          {"server", json_str(server)},
                          {"server_exists", json_bool(exists(server))},
                          {"masters", core::json_strs(p.masters)}}),
                "install   " + p.install + (exists(p.install) ? "" : "  (missing)") +
                    "\nsettings  " + p.settings + (exists(config) ? "" : "  (no config.cfg yet)") +
                    "\nlogs      " + p.logs + "\nwindow    " + window +
                    (exists(window) ? "" : "  (missing)") + "\nserver    " + server +
                    (exists(server) || server.empty() ? "" : "  (missing)") + "\n",
                "", next);
}

}  // namespace ge::gamecli
