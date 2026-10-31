// A dedicated server's process, defined: the flags and server.cfg into the settings, the sockets,
// the run loop with its status lines and digest, the input log, the replay.
//
// ge:summary The dedicated server's process: configure, sockets, the run loop, log and replay
// ge:keywords server_main server_configure flags server-cfg run-loop realtime status digest
// ge:skill net
// ge:test tests/server_core_test.cpp
#include "server/main.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <exception>
#include <fstream>
#include <iterator>
#include <thread>

#include "console/cfg.hpp"
#include "core/num.hpp"
#include "net/address.hpp"

namespace ge::server {

namespace {

namespace gn = ge::net;

// A flag that sets a cvar.
struct CvarFlag {
  const char* flag;
  const char* cvar;
};
constexpr CvarFlag kCvarFlags[] = {
    {"--name", "sv_hostname"},
    {"--description", "sv_description"},
    {"--ip", "net_ip"},
    {"--port", "net_port"},
    {"--maxclients", "sv_maxclients"},
    {"--map", "sv_map"},
    {"--bots", "sv_bots"},
    {"--seed", "sv_seed"},
    {"--password", "sv_password"},
    {"--visibility", "sv_visibility"},
};

bool number(const std::string& key, const std::string& val, double& out, std::string& error) {
  char* end = nullptr;
  out = std::strtod(val.c_str(), &end);
  if (val.empty() || *end != '\0') {
    error = "flag " + key + " needs a number, got '" + val + "'";
    return false;
  }
  return true;
}

bool read_bytes(const std::string& path, std::vector<uint8_t>& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return true;
}

int replay(const ServerGame& game, const ServerConfig& c) {
  std::vector<uint8_t> bytes;
  if (!read_bytes(c.replay, bytes)) {
    std::fprintf(stderr, "%s: cannot read %s; next: check the path\n", game.binary,
                 c.replay.c_str());
    return 1;
  }
  if (!game.replay) {
    std::fprintf(stderr, "%s: this game keeps no input logs; next: run without --replay\n",
                 game.binary);
    return 1;
  }
  ServerReplay r;
  std::string error;
  if (!game.replay(bytes, r, error)) {
    std::fprintf(stderr, "%s: %s: %s\n", game.binary, c.replay.c_str(), error.c_str());
    return 1;
  }
  if (r.first_diverged >= 0) {
    std::printf(
        "replay: %s leaves the log at frame %d of %d; "
        "next: diff the state there with the run that wrote it\n",
        c.replay.c_str(), r.first_diverged, r.frames);
    return 2;
  }
  std::printf("replay: %s agrees on all %d frames; digest %016llx\n", c.replay.c_str(), r.frames,
              static_cast<unsigned long long>(r.digest));
  return 0;
}

int run(const ServerGame& game, const ServerConfig& c) {
  gn::Transport sock, lan;
  bool have_lan = false;
  std::string error;
  if (!c.offline) {
    gn::Address bind;
    const std::string where = (c.ip.empty() ? "0.0.0.0" : c.ip) + ":" + std::to_string(c.port);
    if (!gn::parse_address(where, bind)) {
      std::fprintf(stderr, "%s: net_ip '%s' is not an address; next: give an IPv4 or IPv6 one\n",
                   game.binary, c.ip.c_str());
      return 1;
    }
    if (!gn::open_udp_transport(sock, bind, &error)) {
      std::fprintf(stderr,
                   "%s: cannot listen on %s: %s; next: pick another --port, or --offline to run "
                   "without a socket\n",
                   game.binary, where.c_str(), error.c_str());
      return 1;
    }
    if (game.discovery_port) {
      gn::Address disc = bind;
      disc.port = game.discovery_port;
      std::string why;
      have_lan = gn::open_udp_transport(lan, disc, &why);
      if (!have_lan)
        std::fprintf(stderr,
                     "%s: the LAN's discovery port %u is taken (%s): the LAN will not list this "
                     "server, pings still answer; next: stop the other server on this machine\n",
                     game.binary, static_cast<unsigned>(game.discovery_port), why.c_str());
    }
  }
  Server s;
  if (!server_open(s, game, c, c.offline ? nullptr : &sock, have_lan ? &lan : nullptr, error)) {
    std::fprintf(stderr, "%s: %s\n", game.binary, error.c_str());
    return 1;
  }
  char span[32] = "until stopped";
  if (c.seconds > 0) std::snprintf(span, sizeof span, "%g s", c.seconds);
  std::printf("%s, %s\n", server_banner(s).c_str(), span);
  std::fflush(stdout);

  const double step = s.frame_s;
  const auto frames = static_cast<int64_t>(core::num::round(c.seconds / step));
  const auto status_frames = static_cast<int64_t>(core::num::round(c.status_every / step));
  const auto start = std::chrono::steady_clock::now();
  for (int64_t n = 0; c.seconds == 0 || n < frames; n++) {
    if (c.realtime) {
      const auto due = start + std::chrono::duration<double>(static_cast<double>(n) * step);
      std::this_thread::sleep_until(due);
    }
    server_frame(s);
    if (status_frames > 0 && (n + 1) % status_frames == 0) {
      std::printf("%s\n", server_status(s).c_str());
      std::fflush(stdout);
    }
  }
  const double wall =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  std::printf("done: %lld frames, server time %.3f s, wall %.2f s, digest %016llx\n",
              static_cast<long long>(s.frame), server_time(s), wall,
              static_cast<unsigned long long>(server_digest(s)));
  int code = 0;
  if (!c.log.empty()) {
    std::vector<uint8_t> bytes;
    if (!game.save_log || !game.save_log(s.state, bytes, error)) {
      std::fprintf(stderr, "%s: no input log to write: %s\n", game.binary,
                   game.save_log ? error.c_str() : "this game keeps none");
      code = 1;
    } else {
      std::ofstream out(c.log, std::ios::binary);
      out.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
      if (!out) {
        std::fprintf(stderr, "%s: cannot write %s; next: check the directory\n", game.binary,
                     c.log.c_str());
        code = 1;
      } else {
        std::printf("input log: %s (%zu bytes); replay it with --replay=%s\n", c.log.c_str(),
                    bytes.size(), c.log.c_str());
      }
    }
  }
  server_close(s);
  if (have_lan) gn::close_transport(lan);
  if (!c.offline) gn::close_transport(sock);
  return code;
}

int main_body(int argc, char** argv, const ServerGame& game) {
  std::vector<std::string> args;
  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--help") == 0) {
      std::fputs(server_help(game).c_str(), stdout);
      return 0;
    }
    args.emplace_back(argv[i]);
  }
  console::Console con(true);
  con.set_sink([](std::string_view line) {
    std::printf("%.*s\n", static_cast<int>(line.size()), line.data());
  });
  ServerConfig c;
  std::string error;
  if (!server_configure(con, args, c, game, error)) {
    std::fprintf(stderr, "%s: %s\n", game.binary, error.c_str());
    return 1;
  }
  return c.replay.empty() ? run(game, c) : replay(game, c);
}

}  // namespace

bool server_configure(console::Console& con, const std::vector<std::string>& args, ServerConfig& c,
                      const ServerGame& game, std::string& error) {
  add_server_cvars(con.cvars(), c, game);
  std::vector<std::string> line;  // the command line as `+commands`, flags first
  std::vector<std::string> rest;  // the `+commands` given, after the flags
  std::string dir;
  for (size_t i = 0; i < args.size(); i++) {
    const std::string& a = args[i];
    if (a.rfind("--", 0) != 0) {
      if ((a.empty() || a[0] != '+') && rest.empty()) {
        error = "'" + a + "' is not a flag or a +command; next: " + game.binary + " --help";
        return false;
      }
      rest.push_back(a);
      continue;
    }
    const size_t eq = a.find('=');
    const std::string key = a.substr(0, eq);
    const std::string val = eq == std::string::npos ? "" : a.substr(eq + 1);
    const bool has_val = eq != std::string::npos;
    bool known = false;
    for (const CvarFlag& f : kCvarFlags) {
      if (key != f.flag) continue;
      known = true;
      if (!has_val) {
        error = "flag ";
        error.append(key).append(" needs a value (").append(key).append("=...); next: ");
        error.append(game.binary).append(" --help");
        return false;
      }
      line.insert(line.end(), {std::string("+set"), f.cvar, val});
    }
    if (known) continue;
    if (key == "--for") {
      if (!number(key, val, c.seconds, error)) return false;
    } else if (key == "--status") {
      if (!number(key, val, c.status_every, error)) return false;
    } else if (key == "--realtime") {
      c.realtime = true;
    } else if (key == "--offline") {
      c.offline = true;
    } else if (key == "--log") {
      c.log = val;
    } else if (key == "--replay") {
      c.replay = val;
    } else if (key == "--dir") {
      dir = val;
    } else {
      error = "unknown flag '" + a + "'; next: " + game.binary + " --help";
      return false;
    }
  }
  if (c.seconds < 0) {
    error = "--for must not be negative";
    return false;
  }
  if (!dir.empty()) con.set_search_path({std::filesystem::path(dir)});
  line.insert(line.end(), rest.begin(), rest.end());
  con.clear_problems();
  // Startup sets what a restart would: the latched settings take their values at once, quietly,
  // and are latched again for the server's own console.
  std::vector<console::Cvar*> latched;
  for (const char* name : {"net_ip", "net_port", "sv_maxclients", "sv_bots", "sv_seed"}) {
    console::Cvar* v = con.cvars().find(name);
    if (v && (v->flags & console::kLatch) != 0) {
      v->flags &= ~uint32_t{console::kLatch};
      latched.push_back(v);
    }
  }
  console::start_server(con, line);
  for (console::Cvar* v : latched) v->flags |= console::kLatch;
  for (const console::Problem& p : con.problems()) {
    if (p.severity != console::Problem::Severity::error) continue;
    error = (p.where.file.empty() ? std::string("the settings")
                                  : p.where.file + ":" + std::to_string(p.where.line)) +
            ": " + p.what + (p.next.empty() ? "" : "; next: " + p.next);
    return false;
  }
  return true;
}

std::string server_help(const ServerGame& game) {
  std::string out = std::string(game.binary) + ": " + game.title + "'s dedicated server.\n\n";
  out +=
      "Settings come from server.cfg (in --dir, default here), then these flags, then any\n"
      "+commands (+set sv_hostname \"Arena\"). Each flag sets the cvar beside it.\n\n"
      "  --name=TEXT        sv_hostname: the name in the browser and the lobby\n"
      "  --description=TEXT sv_description: a line under the name\n"
      "  --ip=ADDR          net_ip: the address to listen on (default every interface)\n"
      "  --port=N           net_port: the UDP port (default " +
      std::to_string(game.default_port) +
      ")\n"
      "  --maxclients=N     sv_maxclients: player slots, 1.." +
      std::to_string(game.max_clients < kMaxClients ? game.max_clients : kMaxClients) +
      "\n"
      "  --map=NAME         sv_map: the map (default " +
      std::string(game.default_map) +
      ")\n"
      "  --bots=N           sv_bots: slots the game's bots fill\n"
      "  --seed=N           sv_seed: the run's seed\n"
      "  --password=TEXT    sv_password: joining takes it\n"
      "  --visibility=V     sv_visibility: lan, invite or listed\n\n"
      "  --for=S            seconds of server time to run (default 60; 0 runs until stopped)\n"
      "  --status=S         a status line every S seconds of server time (default 5; 0 none)\n"
      "  --realtime         pace the frames to the wall clock (a server people play on)\n"
      "  --offline          no socket: the game alone\n"
      "  --log=PATH         write the run's input log here\n"
      "  --replay=PATH      replay an input log and name the first frame that leaves it\n"
      "  --dir=PATH         where server.cfg is read\n"
      "  --help             this\n";
  return out;
}

int server_main(int argc, char** argv, const ServerGame& game) {
  // What nothing else caught ends the program here, with a line that says so, rather than in
  // std::terminate's bare abort.
  try {
    return main_body(argc, argv, game);
  } catch (const std::exception& e) {
    std::fprintf(stderr,
                 "%s: stopped by an uncaught exception: %s. This is a bug: run the same command "
                 "under lldb or gdb with a breakpoint on __cxa_throw to see where it was thrown.\n",
                 game.binary, e.what());
  } catch (...) {
    std::fprintf(stderr,
                 "%s: stopped by an uncaught exception that is not a std::exception. This is a "
                 "bug: run the same command under lldb or gdb with a breakpoint on __cxa_throw to "
                 "see where it was thrown.\n",
                 game.binary);
  }
  return 1;
}

}  // namespace ge::server
