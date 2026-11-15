// A dedicated server's process, defined: the flags and server.cfg into the settings, the sockets,
// the run loop with its status lines and digest, the input log, the replay.
//
// ge:summary The dedicated server's process: configure, sockets, the run loop, log and replay
// ge:keywords server_main server_configure flags server-cfg run-loop realtime status digest
// ge:skill net
// ge:test tests/server_core_test.cpp
#include "server/main.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>

#include "core/num.hpp"
#include "net/address.hpp"
#include "net/udp_socket.hpp"
#include "server/accounts.hpp"
#include "server/admin.hpp"
#include "server/record.hpp"

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
    {"--lobby", "sv_lobby"},
    {"--seed", "sv_seed"},
    {"--password", "sv_password"},
    {"--visibility", "sv_visibility"},
    {"--motd", "sv_motd"},
    {"--admin", "sv_admin"},
};

std::vector<std::string> split_commas(const std::string& s) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at <= s.size()) {
    const size_t comma = s.find(',', at);
    std::string part = s.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
    if (!part.empty()) out.push_back(std::move(part));
    if (comma == std::string::npos) break;
    at = comma + 1;
  }
  return out;
}

std::string joined(const std::vector<std::string>& parts) {
  std::string out;
  for (const std::string& p : parts) out.append(out.empty() ? "" : ",").append(p);
  return out;
}

// Files are named from the root (the layers' own paths: `mymod/server.cfg`); a name the root does
// not have is looked for the way the game looks: in the top mod's directory, the mods under it,
// then the base game's (a mod's server.cfg that runs `exec mymod.cfg`). `sv_mod` says which mods.
void mount(console::Console& con, const std::string& dir, const ServerCfg& cfg) {
  const std::filesystem::path root = dir.empty() ? std::string(".") : dir;
  std::vector<std::filesystem::path> search{root};
  for (auto m = cfg.mods.rbegin(); m != cfg.mods.rend(); ++m) search.push_back(root / *m);
  search.push_back(root / cfg.base);
  con.set_search_path(std::move(search));
  con.cvars().set("sv_mod", joined(cfg.mods), console::Source::code);
}

// The layers read (config.hpp) with the command line kept in `cfg`. Reading sets what a restart
// would: the latched settings take their values at once, quietly, and are latched again for the
// server's own console. With `fresh` every server value goes back to its fallback first (a mod
// switch), so a value only the old mod set does not stay.
bool read_layers(console::Console& con, ServerCfg& cfg, bool fresh, std::string& error) {
  std::vector<console::Cvar*> latched;
  for (console::Cvar* v : con.cvars().sorted()) {
    if ((v->flags & console::kServer) != 0 && (v->flags & console::kLatch) != 0) {
      v->flags &= ~uint32_t{console::kLatch};
      latched.push_back(v);
    }
  }
  if (fresh) {
    for (console::Cvar* v : con.cvars().sorted()) {
      if ((v->flags & console::kServer) == 0 || v->name == "sv_mod") continue;
      con.cvars().reset(*v, console::Source::code);
      v->explicit_set = false;
    }
  }
  const bool ok = load_server_cfg(con, cfg, cfg.line, error);
  for (console::Cvar* v : latched) v->flags |= console::kLatch;
  return ok;
}

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
    std::fprintf(stderr, "%s: cannot read %s: check the path\n", game.binary, c.replay.c_str());
    return 1;
  }
  if (!game.replay) {
    std::fprintf(stderr, "%s: this game keeps no input logs, so --replay does nothing here\n",
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
        "compare the state there with the run that wrote it\n",
        c.replay.c_str(), r.first_diverged, r.frames);
    return 2;
  }
  std::printf("replay: %s agrees on all %d frames; digest %016llx\n", c.replay.c_str(), r.frames,
              static_cast<unsigned long long>(r.digest));
  return 0;
}

int64_t wall_unix_us() {
  return static_cast<int64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count());
}

// THE COMMANDS FILE (`--commands`): the lines added since the last look, whole lines only; a file
// cut shorter is read again from its start. The lines there when the server starts are old ones.
struct CommandsFile {
  std::string path;
  std::streamoff seen = 0;
};

std::streamoff file_size(const std::string& path) {
  std::error_code ec;
  const auto n = std::filesystem::file_size(path, ec);
  return ec ? 0 : static_cast<std::streamoff>(n);
}

std::string commands_added(CommandsFile& f) {
  const std::streamoff size = file_size(f.path);
  if (size < f.seen) f.seen = 0;
  if (size == f.seen) return {};
  std::ifstream in(f.path, std::ios::binary);
  in.seekg(f.seen);
  std::string text(static_cast<size_t>(size - f.seen), '\0');
  in.read(text.data(), static_cast<std::streamsize>(text.size()));
  text.resize(static_cast<size_t>(in.gcount()));
  const size_t end = text.rfind('\n');
  if (end == std::string::npos) return {};
  text.resize(end + 1);
  f.seen += static_cast<std::streamoff>(text.size());
  return text;
}

int run(const ServerGame& game, console::Console& con, ServerConfig& c, ServerCfg& cfg,
        Features& features) {
  gn::Transport sock, lan;
  bool have_lan = false;
  std::string error;
  if (!c.offline) {
    gn::Address bind;
    const std::string where = (c.ip.empty() ? "0.0.0.0" : c.ip) + ":" + std::to_string(c.port);
    if (!gn::parse_address(where, bind)) {
      std::fprintf(stderr, "%s: net_ip '%s' is not an IPv4 or IPv6 address\n", game.binary,
                   c.ip.c_str());
      return 1;
    }
    if (!gn::open_udp_transport(sock, bind, &error)) {
      std::fprintf(stderr,
                   "%s: cannot listen on %s: %s. Pick another --port, or use --offline to run "
                   "without a socket.\n",
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
                     "server (pings still answer); is another server running on this machine?\n",
                     game.binary, static_cast<unsigned>(game.discovery_port), why.c_str());
    }
  }
  ServerAccounts accounts;
  const auto close_all = [&] {
    server_accounts_close(accounts);
    if (have_lan) gn::close_transport(lan);
    if (!c.offline) gn::close_transport(sock);
  };
  if (!c.offline) {
    std::string said;
    if (!server_accounts_open(accounts, c, c.root + "/" + game.base_dir, wall_unix_us, said,
                              error)) {
      std::fprintf(stderr, "%s: %s\n", game.binary, error.c_str());
      close_all();
      return 1;
    }
    if (!said.empty()) std::printf("%s: %s\n", game.binary, said.c_str());
  }
  Server s;
  s.features = &features;
  if (!server_open(s, game, c, c.offline ? nullptr : &sock, have_lan ? &lan : nullptr, error)) {
    std::fprintf(stderr, "%s: %s\n", game.binary, error.c_str());
    close_all();
    return 1;
  }
  // THE RECORDING: the stream's bytes written as each block closes, so a server stopped hard
  // keeps every whole second before it.
  Recording rec;
  std::ofstream rec_out;
  const auto rec_write = [&] {
    const std::span<const uint8_t> fresh = record_fresh(rec);
    rec_out.write(reinterpret_cast<const char*>(fresh.data()),
                  static_cast<std::streamsize>(fresh.size()));
  };
  if (!c.record.empty()) {
    rec_out.open(c.record, std::ios::binary);
    if (!rec_out || !record_begin(rec, s, error)) {
      std::fprintf(stderr, "%s: cannot record to %s: %s\n", game.binary, c.record.c_str(),
                   rec_out ? error.c_str() : "the file cannot be made; check the directory");
      server_close(s);
      close_all();
      return 1;
    }
    rec_write();
  }
  // THE CONSOLE AT RUN TIME: `mod` asks for a switch, made between frames (the layers are read
  // again outside the console's own run).
  bool switch_asked = false;
  std::vector<std::string> asked;
  con.add_command(
      "mod",
      "mod NAME[,NAME]: switch the mods, lowest first (mod - for the base game alone); the match "
      "restarts under their settings",
      [&](console::Console& cn, const console::Args& a) {
        if (a.size() < 2) {
          cn.print(cfg.mods.empty() ? "mods: none, the base game alone"
                                    : "mods: " + joined(cfg.mods));
          return;
        }
        std::vector<std::string> want;
        for (size_t i = 1; i < a.size(); i++)
          if (a[i] != "-")
            for (std::string& m : split_commas(a[i])) want.push_back(std::move(m));
        for (const std::string& m : want) {
          if (m.find("..") != std::string::npos || m.find_first_of("/\\:") != std::string::npos) {
            cn.print("mod: '" + m + "' is not a mod's folder name; give the folder's name alone");
            return;
          }
        }
        asked = std::move(want);
        switch_asked = true;
      });
  CommandsFile commands{c.commands, c.commands.empty() ? 0 : file_size(c.commands)};
  const auto switch_mods = [&]() -> bool {
    switch_asked = false;
    std::string why;
    if (!server_switch_mods(con, cfg, c, asked, why)) {
      std::printf("mod: not switched: %s\n", why.c_str());
      return true;
    }
    if (rec.open) {
      record_finish(rec, s);
      rec_write();
      rec_out.close();
      std::printf("recording: %s ended at the mod switch\n", c.record.c_str());
    }
    server_close(s);
    if (!server_open(s, game, c, c.offline ? nullptr : &sock, have_lan ? &lan : nullptr, why)) {
      std::fprintf(stderr, "%s: the match did not restart under the new mods: %s\n", game.binary,
                   why.c_str());
      return false;
    }
    std::string read;
    for (const std::string& f : cfg.read) read.append(read.empty() ? "" : " ").append(f);
    std::printf("mod: %s (read %s); the match restarts\n%s\n",
                cfg.mods.empty() ? "none, the base game alone" : joined(cfg.mods).c_str(),
                read.empty() ? "no cfg file" : read.c_str(), server_banner(s).c_str());
    std::fflush(stdout);
    return true;
  };
  const auto poll_frames =
      std::max<int64_t>(1, static_cast<int64_t>(core::num::round(0.25 / s.frame_s)));

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
    if (!commands.path.empty() && n % poll_frames == 0) {
      const std::string lines = commands_added(commands);
      if (!lines.empty()) {
        con.execute(lines);
        std::fflush(stdout);
      }
      if (switch_asked && !switch_mods()) {
        server_close(s);
        close_all();
        return 1;
      }
    }
    if (rec.open) {
      record_frame(rec, s);
      rec_write();
    }
    server_accounts_update(accounts, wall_unix_us());
    server_accounts_reports(accounts, s, wall_unix_us());
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
  if (rec.open) {
    record_finish(rec, s);
    rec_write();
    rec_out.close();
    if (!rec_out) {
      std::fprintf(stderr, "%s: the recording %s was cut short: check the disk\n", game.binary,
                   c.record.c_str());
      code = 1;
    } else {
      std::printf("recording: %s (%zu bytes, %lld frames)\n", c.record.c_str(), rec.bytes.size(),
                  static_cast<long long>(s.frame));
    }
  }
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
        std::fprintf(stderr, "%s: cannot write %s: check the directory\n", game.binary,
                     c.log.c_str());
        code = 1;
      } else {
        std::printf("input log: %s (%zu bytes); replay it with --replay=%s\n", c.log.c_str(),
                    bytes.size(), c.log.c_str());
      }
    }
  }
  server_close(s);
  close_all();
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
  ServerCfg cfg;
  AdminFeatures admin;
  Features features;
  if (game.serve == nullptr) features.list = admin_features(admin);
  std::string error;
  if (!server_configure(con, args, c, cfg, game, error, &features)) {
    std::fprintf(stderr, "%s: %s\n", game.binary, error.c_str());
    return 1;
  }
  if (c.explain) {
    std::printf("read:");
    for (const std::string& f : cfg.read) std::printf(" %s", f.c_str());
    std::printf("%s\n", cfg.read.empty() ? " (no cfg file)" : "");
    std::fputs(explain(con, cfg, c.explain_prefix).c_str(), stdout);
    return 0;
  }
  return c.replay.empty() ? run(game, con, c, cfg, features) : replay(game, c);
}

}  // namespace

bool server_configure(console::Console& con, const std::vector<std::string>& args, ServerConfig& c,
                      ServerCfg& cfg, const ServerGame& game, std::string& error,
                      Features* features) {
  add_server_cvars(con.cvars(), c, game);
  if (features) features_attach(*features, con);
  cfg.base = game.base_dir;
  std::vector<std::string> line;  // the command line as `+commands`, flags first
  std::vector<std::string> rest;  // the `+commands` given, after the flags
  std::string dir;
  for (size_t i = 0; i < args.size(); i++) {
    const std::string& a = args[i];
    if (a.rfind("--", 0) != 0) {
      // The first command needs no `+` (`map dm2`); it gets one here, since the flags' `+set`
      // lines go before it.
      rest.push_back(rest.empty() && (a.empty() || a[0] != '+') ? "+" + a : a);
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
        error.append(key).append(" needs a value (").append(key).append("=...); see ");
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
    } else if (key == "--record") {
      c.record = val;
    } else if (key == "--replay") {
      c.replay = val;
    } else if (key == "--commands") {
      c.commands = val;
    } else if (key == "--root") {
      dir = val;
      c.root = val;
    } else if (key == "--base") {
      cfg.base = val;
    } else if (key == "--mod") {
      cfg.mods = split_commas(val);
    } else if (key == "--cfg") {
      cfg.profile = val;
    } else if (key == "--explain") {
      c.explain = true;
      c.explain_prefix = val;
    } else {
      error = "unknown flag '" + a + "'; see " + game.binary + " --help";
      return false;
    }
  }
  if (c.seconds < 0) {
    error = "--for must not be negative";
    return false;
  }
  if (cfg.base.empty()) {
    error = "--base needs a directory; see " + std::string(game.binary) + " --help";
    return false;
  }
  line.insert(line.end(), rest.begin(), rest.end());
  cfg.line = std::move(line);
  mount(con, dir, cfg);
  return read_layers(con, cfg, false, error);
}

bool server_switch_mods(console::Console& con, ServerCfg& cfg, const ServerConfig& config,
                        const std::vector<std::string>& mods, std::string& error) {
  const std::vector<std::string> was = cfg.mods;
  cfg.mods = mods;
  mount(con, config.root, cfg);
  if (read_layers(con, cfg, true, error)) return true;
  std::string ignored;
  cfg.mods = was;
  mount(con, config.root, cfg);
  read_layers(con, cfg, true, ignored);
  error += "; the mods stay " + (was.empty() ? std::string("none") : joined(was));
  return false;
}

std::string server_help(const ServerGame& game) {
  std::string out = std::string(game.binary) + ": " + game.title + "'s dedicated server.\n\n";
  out +=
      "Settings come from the base game directory's server.cfg, then each mod's, then a\n"
      "profile, then these flags, then any commands, a + before each after the first\n"
      "(map dm2 +set sv_hostname \"Arena\"). Each flag sets the cvar beside it.\n\n"
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
      "  --lobby=1          sv_lobby: warm up every map until everyone is ready\n"
      "  --seed=N           sv_seed: the run's seed\n"
      "  --password=TEXT    sv_password: joining takes it\n"
      "  --visibility=V     sv_visibility: lan, invite or listed\n"
      "  --motd=TEXT        sv_motd: the message a player sees on joining\n"
      "  --admin=TEXT       sv_admin: how to reach the server's owner\n\n"
      "  --root=PATH        where the game directories are (default here)\n"
      "  --base=DIR         the base game directory (default " +
      std::string(game.base_dir) +
      "), its server.cfg read first\n"
      "  --mod=A,B          mods over the base, lowest first: each one's server.cfg after it\n"
      "  --cfg=NAME         a profile: NAME.cfg from the highest directory that has one\n"
      "  --explain[=PREFIX] print each server value, where it was set and what it overrode\n\n"
      "  --for=S            seconds of server time to run (default 60; 0 runs until stopped)\n"
      "  --status=S         a status line every S seconds of server time (default 5; 0 none)\n"
      "  --realtime         pace the frames to the wall clock (a server people play on)\n"
      "  --offline          no socket: the game alone\n"
      "  --log=PATH         write the run's input log here\n"
      "  --record=PATH      record the match, every player's part, as it runs (a .ged file)\n"
      "  --replay=PATH      replay an input log and name the first frame that leaves it\n"
      "  --commands=PATH    run each console line added to this file (mod NAME switches mods)\n"
      "  --help             this\n";
  return out;
}

int server_main(int argc, char** argv, const ServerGame& game) {
  // A line at a time even into a file or a pipe (a server started detached writes its log there),
  // so the log reads as the server runs and a crash loses nothing printed before it.
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
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
