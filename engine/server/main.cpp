// A dedicated server's process, defined: the flags and server.cfg into the settings, the sockets,
// the run loop with its status lines and digest, the input log, the replay.
//
// ge:summary The dedicated server's process: configure, sockets, the run loop, log and replay
// ge:keywords server_main server_configure flags server-cfg run-loop realtime status digest
// ge:skill net
// ge:test tests/server_core_test.cpp
#include "server/main.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

#if defined(_WIN32)
#include <io.h>
#else
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#endif

#include "console/clock_macros.hpp"
#include "core/num.hpp"
#include "net/address.hpp"
#include "net/net_commands.hpp"
#include "net/packet_log.hpp"
#include "net/rates.hpp"
#include "net/udp_socket.hpp"
#include "server/accounts.hpp"
#include "server/admin.hpp"
#include "server/autorecord.hpp"
#include "server/console_log.hpp"
#include "server/hand_record.hpp"
#include "server/macros.hpp"
#include "server/net_restart.hpp"
#include "server/record.hpp"
#include "server/spectators.hpp"
#include "server/terminal.hpp"

namespace ge::server {

namespace {

namespace gn = ge::net;

// THE FLAGS, as data: `server_configure` refuses a flag no row names, and `--help` and the
// reference manual list every one. A row with a cvar sets that cvar (`--name=Arena` is `+set
// sv_hostname Arena`); `group` splits `--help` into its paragraphs.
struct ServerFlag {
  const char* flag;
  const char* arg;   // "" a switch
  const char* cvar;  // the cvar it sets, or nullptr
  const char* help;
  int group;
};
constexpr ServerFlag kServerFlags[] = {
    {"--name", "TEXT", "sv_hostname", "the name in the browser and the lobby", 0},
    {"--description", "TEXT", "sv_description", "a line under the name", 0},
    {"--ip", "ADDR", "net_ip", "the address to listen on (default every interface)", 0},
    {"--port", "N", "net_port", "the UDP port", 0},
    {"--maxclients", "N", "sv_maxclients", "player slots", 0},
    {"--map", "NAME", "sv_map", "the map", 0},
    {"--bots", "N", "sv_bots", "slots the game's bots fill", 0},
    {"--lobby", "1", "sv_lobby", "warm up every map until everyone is ready", 0},
    {"--seed", "N", "sv_seed", "the run's seed", 0},
    {"--password", "TEXT", "sv_password", "joining takes it", 0},
    {"--visibility", "V", "sv_visibility", "lan, invite or listed", 0},
    {"--motd", "TEXT", "sv_motd", "the message a player sees on joining", 0},
    {"--admin", "TEXT", "sv_admin", "how to reach the server's owner", 0},
    {"--tvport", "N", "sv_tvport", "spectators watch on this port, behind a delay (default none)",
     0},
    {"--tvdelay", "S", "sv_tvdelay", "how far behind the match they see it (default 10 s)", 0},
    {"--root", "PATH", nullptr, "where the game directories are (default here)", 1},
    {"--base", "DIR", nullptr, "the base game directory, its server.cfg read first", 1},
    {"--mod", "A,B", nullptr, "mods over the base, lowest first: each one's server.cfg after it",
     1},
    {"--cfg", "NAME", nullptr, "a profile: NAME.cfg from the highest directory that has one", 1},
    {"--explain", "[=PREFIX]", nullptr,
     "print each server value, where it was set and what it overrode", 1},
    {"--for", "S", nullptr,
     "seconds of server time to run, as fast as it can unless --realtime (none: paced to the "
     "wall clock until stopped; 0 runs until stopped)",
     2},
    {"--status", "S", nullptr, "a status line every S seconds of server time (default 5; 0 none)",
     2},
    {"--realtime", "", nullptr, "pace the frames to the wall clock (a server people play on)", 2},
    {"--offline", "", nullptr, "no socket: the game alone", 2},
    {"--log", "PATH", nullptr, "write the run's input log here", 2},
    {"--record", "PATH", nullptr, "record the match, every player's part, as it runs (a .ged file)",
     2},
    {"--replay", "PATH", nullptr, "replay an input log and name the first frame that leaves it", 2},
    {"--commands", "PATH", nullptr,
     "run each console line added to this file (mod NAME switches mods)", 2},
    {"--manual", "PATH", nullptr,
     "write every setting, command and flag for the reference manual, then quit", 2},
    {"--help", "", nullptr, "this", 2},
};

// The game's own default a flag's help adds ("" none).
std::string flag_default(const ServerFlag& f, const ServerGame& game) {
  const std::string_view name = f.flag;
  if (name == "--port") return "default " + std::to_string(game.default_port);
  if (name == "--maxclients")
    return "1.." + std::to_string(game.max_clients < kMaxClients ? game.max_clients : kMaxClients);
  if (name == "--map") return "default " + std::string(game.default_map);
  if (name == "--base") return "default " + std::string(game.base_dir);
  return "";
}

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
// The settings `set` made that nothing registered, for the game's rules (a mod's own).
void take_created(console::Console& con, ServerConfig& c) {
  c.created.clear();
  for (const console::Cvar* v : con.cvars().sorted())
    if ((v->flags & console::kCreated) != 0) c.created[v->name] = v->value;
}

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

// THE TERMINAL: lines typed on the server's own terminal, read on a thread of their own and run as
// the operator's console lines with the commands file's. One reader for the process, started by
// the first networked server whose input is a terminal it has in the foreground (a server started
// in the background, or with its input a pipe or a file, reads none); it ends at the input's end.
// Where the terminal can be edited (not Windows, `TERM` set and not `dumb`) it is put in raw mode
// and the reader hands over every key as it comes, for the editor (server/terminal.hpp).
struct TerminalLines {
  std::mutex m;
  std::string lines;  // whole lines, or the raw keys when `raw`
  bool raw = false;
};

// The editor while the terminal is raw (null otherwise): the sink and the game's lines put their
// line above the one being typed.
TermEdit* g_edit = nullptr;
// The console log (console_log.hpp) every printed line is copied to, and its prefix setting.
ConsoleLog g_log;
gn::PacketLog g_netlog;  // the packet log (net/packet_log.hpp) the server's sockets write to
const std::string* g_log_prefix = nullptr;

#if !defined(_WIN32)
termios g_cooked{};
void terminal_restore() { tcsetattr(STDIN_FILENO, TCSANOW, &g_cooked); }
extern "C" void terminal_signal(int sig) {
  terminal_restore();
  signal(sig, SIG_DFL);
  raise(sig);
}
#endif

bool terminal_ours() {
#if defined(_WIN32)
  return _isatty(_fileno(stdin)) != 0;
#else
  return isatty(STDIN_FILENO) != 0 && tcgetpgrp(STDIN_FILENO) == getpgrp();
#endif
}

// Raw mode (no line buffering, no echo; signals kept), undone at exit or on a signal.
bool terminal_make_raw() {
#if defined(_WIN32)
  return false;
#else
  const char* term = std::getenv("TERM");
  if (term == nullptr || *term == 0 || std::strcmp(term, "dumb") == 0) return false;
  if (isatty(STDOUT_FILENO) == 0 || tcgetattr(STDIN_FILENO, &g_cooked) != 0) return false;
  termios raw = g_cooked;
  raw.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) return false;
  std::atexit(terminal_restore);
  for (const int sig : {SIGINT, SIGTERM, SIGHUP, SIGQUIT}) signal(sig, terminal_signal);
  return true;
#endif
}

TerminalLines& terminal_lines() {
  static TerminalLines* t = [] {
    auto* made = new TerminalLines;  // never freed: the reader may outlive every server
    made->raw = terminal_make_raw();
    std::thread([made] {
#if !defined(_WIN32)
      if (made->raw) {
        char c = 0;
        while (read(STDIN_FILENO, &c, 1) == 1) {
          const std::lock_guard<std::mutex> hold(made->m);
          made->lines.push_back(c);
        }
        return;
      }
#endif
      for (std::string line; std::getline(std::cin, line);) {
        const std::lock_guard<std::mutex> hold(made->m);
        made->lines += line + "\n";
      }
    }).detach();
    return made;
  }();
  return *t;
}

// A line printed on the terminal, above the line being typed when there is one.
void terminal_print(const std::string& head, std::string_view line) {
  if (g_log.file != nullptr) {
    const std::time_t t = std::time(nullptr);
    std::tm now{};
#if defined(_WIN32)
    localtime_s(&now, &t);
#else
    localtime_r(&t, &now);
#endif
    console_log_line(g_log, g_log_prefix != nullptr ? *g_log_prefix : "", line, now);
  }
  if (g_edit == nullptr) {
    std::printf("%s%.*s\n", head.c_str(), static_cast<int>(line.size()), line.data());
    return;
  }
  std::printf("%s%s%.*s\n%s", term_hide().c_str(), head.c_str(), static_cast<int>(line.size()),
              line.data(), term_redraw(*g_edit).c_str());
}

std::string terminal_typed(TerminalLines& t) {
  const std::lock_guard<std::mutex> hold(t.m);
  return std::exchange(t.lines, std::string());
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
    std::string said6;
    open_ipv6(sock, c.ipv6, c.ip6, c.port, said6);
    if (!said6.empty()) std::fprintf(stderr, "%s: %s", game.binary, said6.c_str());
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
  take_created(con, c);
  c.admin_info = [&con] {
    std::string out;
    const std::string all = con.cvars().info_string(console::kServerInfo);
    size_t at = 0;
    while (at < all.size()) {  // each pair kept when `sets` made it
      const size_t k = all.find('\\', at + 1), v = all.find('\\', k + 1);
      const std::string key = all.substr(at + 1, k - at - 1);
      const console::Cvar* cv = con.cvars().find(key);
      if (cv != nullptr && (cv->flags & console::kCreated) != 0)
        out += all.substr(at, (v == std::string::npos ? all.size() : v) - at);
      at = v == std::string::npos ? all.size() : v;
    }
    return out;
  };
  if (!server_open(s, game, c, c.offline ? nullptr : &sock, have_lan ? &lan : nullptr, error)) {
    std::fprintf(stderr, "%s: %s\n", game.binary, error.c_str());
    // The manual needs the console, not a match: a machine with no game's files still writes it,
    // less what the game registers only once its map is up.
    if (c.manual.empty()) {
      close_all();
      return 1;
    }
  }
  // A console line the server does not know goes to the game (its rules' own `map`, a mod's
  // words), while there is one; the words it declares are registered (add_game_words).
  if (game.console != nullptr) {
    con.set_unknown([&s, &game](const console::Args& a) {
      return s.state != nullptr && game.console(s.state, a.raw);
    });
    add_game_words(con, game, [&s, &game](const std::string& line) {
      return s.state == nullptr            ? GameWordRun::no_game
             : game.console(s.state, line) ? GameWordRun::taken
                                           : GameWordRun::refused;
    });
  }
  // THE RECORDING: the stream's bytes written as each block closes, so a server stopped hard
  // keeps every whole second before it; the same bytes feed the server's own TV (spectators.hpp).
  Recording rec;
  std::ofstream rec_out;
  gn::Transport tv_sock;
  ServerTv tv;
  const bool want_tv = c.tv_port != 0 && !c.offline;
  const auto rec_write = [&] {
    const std::span<const uint8_t> fresh = record_fresh(rec);
    if (rec_out.is_open())
      rec_out.write(reinterpret_cast<const char*>(fresh.data()),
                    static_cast<std::streamsize>(fresh.size()));
    if (tv.on && !server_tv_frame(tv, fresh, s.now_us) && !tv.error.empty()) {
      std::fprintf(stderr, "%s: the TV stopped: %s\n", game.binary, tv.error.c_str());
      tv.error = "said";  // once
    }
  };
  if (!c.record.empty()) {
    rec_out.open(c.record, std::ios::binary);
    if (!rec_out) {
      std::fprintf(stderr,
                   "%s: cannot record to %s: the file cannot be made; check the directory\n",
                   game.binary, c.record.c_str());
      server_close(s);
      close_all();
      return 1;
    }
  }
  if (want_tv) {
    gn::Address at;
    const std::string where = (c.ip.empty() ? "0.0.0.0" : c.ip) + ":" + std::to_string(c.tv_port);
    if (!gn::parse_address(where, at) || !gn::open_udp_transport(tv_sock, at, &error)) {
      std::fprintf(stderr,
                   "%s: the TV cannot listen on %s: %s. Pick another sv_tvport, or 0 for no TV.\n",
                   game.binary, where.c_str(), error.c_str());
      server_close(s);
      close_all();
      return 1;
    }
  }
  const auto tv_off = [&] {
    if (!want_tv) return;
    server_tv_close(tv, s.now_us);
    gn::close_transport(tv_sock);
  };
  if (!c.record.empty() || want_tv) {
    if (!record_begin(rec, s, error)) {
      std::fprintf(stderr, "%s: cannot %s: %s\n", game.binary,
                   c.record.empty() ? "serve the TV" : ("record to " + c.record).c_str(),
                   error.c_str());
      tv_off();
      server_close(s);
      close_all();
      return 1;
    }
    if (want_tv) {
      server_tv_open(tv, s, tv_sock);
      std::printf("tv: spectators on %s, %g s behind the match\n",
                  gn::to_string(tv_sock.local).c_str(), c.tv_delay_s);
    }
    rec_write();
  }
  // EVERY MATCH KEPT (autorecord.hpp): with sv_autorecord 1, each match to its own file under the
  // game folder's demos/, written as it goes.
  MatchRecorder matches;
  std::ofstream match_out;
  std::string match_path;
  const auto match_write = [&] {
    const std::span<const uint8_t> fresh = record_fresh(matches.rec);
    match_out.write(reinterpret_cast<const char*>(fresh.data()),
                    static_cast<std::streamsize>(fresh.size()));
  };
  const auto match_done = [&] {
    match_write();
    match_out.close();
    if (!match_out)
      std::fprintf(stderr, "%s: the match's recording %s was cut short: check the disk\n",
                   game.binary, match_path.c_str());
    else
      std::printf("autorecord: %s (%zu bytes, %.0f s)\n", match_path.c_str(),
                  matches.rec.bytes.size(),
                  static_cast<double>(s.now_us - matches.started_us) / 1e6);
  };
  const auto match_frame = [&] {
    if (c.autorecord == 0) return;
    std::string why;
    switch (match_record_frame(matches, s, why)) {
      case MatchRecord::started: {
        const std::filesystem::path dir = std::filesystem::path(c.root) / game.base_dir / "demos";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        match_path = (dir / match_record_name(s.config.map, wall_unix_us() / 1000000)).string();
        match_out.open(match_path, std::ios::binary);
        if (!match_out) {
          std::fprintf(stderr, "%s: cannot record the match to %s: check the directory\n",
                       game.binary, match_path.c_str());
          match_record_stop(matches, s);
          return;
        }
        match_write();
        std::printf("autorecord: the match on %s, recording to %s\n", s.config.map.c_str(),
                    match_path.c_str());
        return;
      }
      case MatchRecord::recording:
        match_write();
        return;
      case MatchRecord::finished:
        match_done();
        return;
      case MatchRecord::failed:
        std::fprintf(stderr, "%s: sv_autorecord: %s\n", game.binary, why.c_str());
        return;
      case MatchRecord::none:
        return;
    }
  };
  const auto match_stop = [&] {
    if (match_record_stop(matches, s) && match_out.is_open()) match_done();
  };
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
  // `serverrecord <name>` and `serverstop` (hand_record.hpp): the operator's own recording.
  HandRecording hand;
  const std::filesystem::path demos = std::filesystem::path(c.root) / game.base_dir / "demos";
  con.add_command("serverrecord",
                  "serverrecord NAME: record the match from now, every player's part, to "
                  "demos/NAME.ged in the game's folder; serverstop ends it",
                  [&](console::Console& cn, const console::Args& a) {
                    std::string said;
                    if (a.size() < 2)
                      said = "serverrecord: name the file (serverrecord NAME)";
                    else
                      hand_record_start(hand, s, demos, a[1], said);
                    cn.print(said);
                  });
  con.add_command("serverstop", "serverstop: end the recording serverrecord began",
                  [&](console::Console& cn, const console::Args&) {
                    std::string said;
                    hand_record_stop(hand, s, said);
                    cn.print(said);
                  });
  // `version`: the game's own build version.
  con.add_command(
      "version", "version: the server's version",
      [&game](console::Console& cn, const console::Args&) {
        cn.print(std::string("Version ") + (*game.version_text ? game.version_text : "unknown"));
      });
  // `quit [reason]`: the server ends after this frame, as at the end of its time: every player
  // told by the game as it closes (a classic server's "server shutdown"), the masters too.
  // `recycle [reason]` ends it the same way but tells the players to come back: whatever keeps
  // the process running (a script, a service) starts it again. Both exit with success.
  bool quit_asked = false;
  bool quit_restart = false;
  std::string quit_reason;  // the words after the command, joined by single spaces
  const auto ask_quit = [&](const console::Args& a, bool restart) {
    quit_asked = true;
    quit_restart = restart;
    quit_reason.clear();
    for (size_t i = 1; i < a.size(); i++) quit_reason += (i > 1 ? " " : "") + a.words[i];
  };
  con.add_command("quit", "quit [reason]: end the server; every player is told and dropped",
                  [&](console::Console&, const console::Args& a) { ask_quit(a, false); });
  // `dns` and `net_stats` over the server's socket; `$net_uprate` and `$net_dnrate`, the bytes a
  // second it sent and received over the last three seconds.
  gn::add_net_commands(con, [&]() -> const gn::Transport* { return c.offline ? nullptr : &sock; });
  gn::Rates rates;
  con.add_macro("net_uprate",
                [&] { return std::to_string(static_cast<int64_t>(rates.sent_bytes)); });
  con.add_macro("net_dnrate",
                [&] { return std::to_string(static_cast<int64_t>(rates.received_bytes)); });
  // `$com_maplist`: the maps the game finds, by name ignoring case, each once, by single spaces.
  con.add_macro("com_maplist", [&] {
    std::vector<std::string> maps;
    if (game.maps != nullptr && s.state != nullptr) game.maps(s.state, maps);
    return server_map_list(maps);
  });
  // `net_restart`: the server's socket closed and opened again on net_ip and net_port, a value
  // waiting on either (latched) taken now; when that cannot be had, the last port that opened.
  std::string good_ip = c.ip;
  int good_port = c.port;
  con.add_command("net_restart",
                  "net_restart: open the server's socket again on net_ip and net_port",
                  [&](console::Console& cn, const console::Args&) {
                    if (c.offline) return;
                    for (const char* name : {"net_ip", "net_port", "net_ip6", "net_enable_ipv6"}) {
                      console::Cvar* v = con.cvars().find(name);
                      if (v == nullptr || !v->has_latched) continue;
                      const std::string to = v->latched;
                      con.cvars().set(*v, to, console::Source::code);
                      v->has_latched = false;
                      v->latched.clear();
                    }
                    int port = c.port;
                    std::string said;
                    const bool open = net_restart(sock, c.ip, port, good_ip, good_port, said);
                    if (open) open_ipv6(sock, c.ipv6, c.ip6, port, said);
                    if (port != c.port)
                      if (console::Cvar* v = con.cvars().find("net_port"))
                        con.cvars().set(*v, std::to_string(port), console::Source::code);
                    c.port = port;
                    if (!said.empty()) cn.print(said.substr(0, said.size() - 1));
                    if (!open) return;
                    good_ip = c.ip;
                    good_port = c.port;
                    if (game.reopened != nullptr && s.state != nullptr)
                      game.reopened(s.state, sock);
                  });
  // `showip`: this machine's name and addresses, then the sockets the server listens on.
  con.add_command(
      "showip", "showip: this machine's addresses and the sockets the server listens on",
      [&](console::Console& cn, const console::Args&) {
        std::string name, canonical, why;
        std::vector<gn::HostAddress> addresses;
        if (!gn::host_addresses(name, canonical, addresses, true, &why)) {
          cn.print(why);
          return;
        }
        std::vector<std::pair<std::string, gn::Address>> sockets;
        const auto udp = [](const gn::Address& a) {
          return std::string(a.family == gn::Family::ipv6 ? "Server UDP6" : "Server UDP");
        };
        if (!c.offline) sockets.emplace_back(udp(sock.local), sock.local);
        if (!c.offline && sock.udp6.handle != -1)
          sockets.emplace_back("Server UDP6", sock.udp6.local);
        if (tv_sock.kind != gn::TransportKind::none) sockets.emplace_back("TV UDP", tv_sock.local);
        if (game.sockets != nullptr && s.state != nullptr) game.sockets(s.state, sockets);
        std::string text = gn::showip_text(canonical, addresses, sockets);
        if (!text.empty() && text.back() == '\n') text.pop_back();
        cn.print(text);
      });
  con.add_command("recycle",
                  "recycle [reason]: end the server and tell every player to reconnect; whatever "
                  "keeps it running starts it again",
                  [&](console::Console&, const console::Args& a) { ask_quit(a, true); });
  const auto hand_stop = [&] {
    std::string said;
    if (hand_record_stop(hand, s, said)) std::printf("%s\n", said.c_str());
  };
  if (!c.manual.empty()) {  // every command and setting registered: the manual's source
    const bool wrote = console::write_manual(c.manual, console::manual_of(con), error);
    if (wrote)
      std::printf("the reference manual's source written to %s\n", c.manual.c_str());
    else
      std::fprintf(stderr, "%s: %s Name a file in a folder you can write.\n", game.binary,
                   error.c_str());
    server_close(s);
    close_all();
    return wrote ? 0 : 1;
  }
  CommandsFile commands{c.commands, c.commands.empty() ? 0 : file_size(c.commands)};
  TerminalLines* terminal = c.offline || !terminal_ours() ? nullptr : &terminal_lines();
  // The editor over a raw terminal, its history read from the base game folder.
  const std::string history_file =
      (std::filesystem::path(c.root.empty() ? "." : c.root) / game.base_dir / ".syshistory")
          .string();
  TermEdit edit;
  const bool edit_on = terminal != nullptr && terminal->raw;
  if (edit_on) {
    term_history_load(edit, history_file, c.con_history);
    g_edit = &edit;
    std::fputs(term_redraw(edit).c_str(), stdout);
    std::fflush(stdout);
  }
  const auto edit_off = [&] {
    console_log_close(g_log);
    gn::packet_log_close(g_netlog);
    if (!edit_on || g_edit == nullptr) return;
    g_edit = nullptr;
    std::fputs(term_hide().c_str(), stdout);
    term_history_save(edit, history_file, c.con_history);
  };
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
      if (rec_out.is_open()) {
        rec_out.close();
        std::printf("recording: %s ended at the mod switch\n", c.record.c_str());
      }
      if (tv.on) {
        server_tv_end(tv);
        std::printf(
            "tv: the broadcast ended at the mod switch; spectators join again to watch the "
            "next match\n");
      }
    }
    match_stop();
    hand_stop();
    matches = MatchRecorder{};  // the next match is looked for from the new map's start
    server_close(s);
    take_created(con, c);
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
    if (want_tv) {  // a new match on a new broadcast, the TV's port kept
      server_tv_close(tv, s.now_us);
      rec = Recording{};
      if (record_begin(rec, s, why)) {
        server_tv_open(tv, s, tv_sock);
        rec_write();
      } else {
        std::fprintf(stderr, "%s: the TV is off for this match: %s\n", game.binary, why.c_str());
      }
    }
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
  std::vector<std::string> printed;  // the game's console lines of one frame
  std::string map_seen;              // the map sv_mapcmd last ran after
  // The game's console lines since the last call, printed and logged.
  const auto print_lines = [&] {
    if (game.console_lines == nullptr || s.state == nullptr) return;
    printed.clear();
    game.console_lines(s.state, printed);
    for (std::string& line : printed) {
      while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
      terminal_print("", line);
      features_log(features, line);
    }
    if (!printed.empty()) std::fflush(stdout);
  };
  // The console log brought to its settings each frame (a change closes and opens it again).
  g_log_prefix = &c.con_logfile_prefix;
  const auto log_check = [&] {
    std::string said;
    const std::string folder =
        con.search_path().empty()
            ? (std::filesystem::path(c.root.empty() ? "." : c.root) / game.base_dir).string()
            : con.search_path().front().string();
    console_log_update(g_log, c.con_logfile, folder, c.con_logfile_name, c.con_logfile_flush, said);
    if (!gn::packet_log_update(g_netlog, c.net_log, folder, c.net_log_name, c.net_log_flush,
                               said)) {
      con.cvars().set("net_log", "0", console::Source::code);
      g_netlog.mode = 0;
    }
    sock.log = g_netlog.file != nullptr ? &g_netlog : nullptr;
    lan.log = sock.log;
    if (said.empty()) return;
    said.pop_back();  // each line ends in a newline; print adds the last
    con.print(said);
  };
  for (int64_t n = 0; c.seconds == 0 || n < frames; n++) {
    log_check();
    if (c.realtime) {
      const auto due = start + std::chrono::duration<double>(static_cast<double>(n) * step);
      std::this_thread::sleep_until(due);
    }
    server_frame(s);
    print_lines();
    // A recoverable error the game raised: its players dropped, or the process ended with it.
    if (game.drop_error != nullptr && s.state != nullptr) {
      const std::string e = game.drop_error(s.state);
      if (!e.empty() && c.fatal_errors != 0) {
        if (game.crash != nullptr) game.crash(s.state, "Server fatal crashed: " + e + "\n");
        if (g_log.file != nullptr) {
          std::fputs(("FATAL: " + e + "\n").c_str(), g_log.file);
        }
        edit_off();
        const char* mark = c.console_prefix.starts_with("<?>") ? "<3>" : "";
        std::fprintf(stderr, "%s********************\n%sFATAL: %s\n%s********************\n", mark,
                     mark, e.c_str(), mark);
        tv_off();
        server_close(s);
        close_all();
        return 1;
      }
      if (!e.empty()) {
        con.print("********************\nERROR: " + e + "\n********************");
        if (game.crash != nullptr) game.crash(s.state, "Server crashed: " + e + "\n");
        std::fflush(stdout);
      }
    }
    // sv_mapcmd once after each map loads, the change seen within a quarter of a second.
    if (n % poll_frames == 0 && server_map_loaded(s, map_seen) && !c.mapcmd.empty()) {
      con.execute(c.mapcmd);
      std::fflush(stdout);
    }
    if (edit_on) {  // every key as it comes, each finished line run
      const std::string keys = terminal_typed(*terminal);
      std::string shown;
      std::vector<std::string> done;
      edit.mode = c.con_tab_mode;
      edit.threshold = std::max(c.con_tab_threshold, 1);
      for (const char k : keys) term_feed(edit, con, k, shown, done);
      if (!shown.empty()) {
        std::fputs(shown.c_str(), stdout);
        std::fflush(stdout);
      }
      for (const std::string& line : done) {
        con.execute(line);
        std::fflush(stdout);
      }
    }
    if ((!commands.path.empty() || (terminal != nullptr && !edit_on)) && n % poll_frames == 0) {
      std::string lines = commands.path.empty() ? std::string() : commands_added(commands);
      if (terminal != nullptr && !edit_on) lines += terminal_typed(*terminal);
      if (!lines.empty()) {
        con.execute(lines);
        std::fflush(stdout);
      }
      if (switch_asked && !switch_mods()) {
        edit_off();
        tv_off();
        server_close(s);
        close_all();
        return 1;
      }
    }
    if (rec.open) {
      record_frame(rec, s);
      rec_write();
    }
    hand_record_frame(hand, s);
    if (!c.offline) gn::rates_step(rates, static_cast<double>(s.now_us) / 1e6, sock, 0, 3);
    match_frame();
    if (quit_asked) {
      if (game.quit != nullptr && s.state != nullptr) game.quit(s.state, quit_reason, quit_restart);
      print_lines();  // what the game said as it ended
      break;
    }
    server_accounts_update(accounts, wall_unix_us());
    server_accounts_reports(accounts, s, wall_unix_us());
    if (status_frames > 0 && (n + 1) % status_frames == 0) {
      const std::string watching = server_tv_status(tv);
      std::printf("%s%s%s\n", server_status(s).c_str(), watching.empty() ? "" : " ",
                  watching.c_str());
      std::fflush(stdout);
    }
  }
  const double wall =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  std::printf("done: %lld frames, server time %.3f s, wall %.2f s, digest %016llx\n",
              static_cast<long long>(s.frame), server_time(s), wall,
              static_cast<unsigned long long>(server_digest(s)));
  int code = 0;
  match_stop();
  hand_stop();
  if (rec.open) record_finish(rec, s);
  rec_write();
  if (tv.on) {
    // The spectators are sent the end of the match: the TV runs on past the last frame until
    // every one has it, or the delay and 2 s more have gone.
    server_tv_end(tv);
    const int64_t until =
        s.now_us + static_cast<int64_t>(core::num::round((c.tv_delay_s + 2) * 1e6));
    const auto drained = [&] {
      for (const tv::Viewer& v : tv.host.tv.viewers)
        if (v.greeted && !v.files_only && !v.ended) return false;
      return true;
    };
    const auto step_us = static_cast<int64_t>(core::num::round(step * 1e6));
    int64_t at = s.now_us;
    const auto from = std::chrono::steady_clock::now();
    while (at < until && !drained()) {
      at += step_us;
      if (c.realtime)
        std::this_thread::sleep_until(from + std::chrono::microseconds(at - s.now_us));
      server_tv_frame(tv, {}, at);
    }
    std::printf("tv: the match's end sent to %lld spectators\n",
                static_cast<long long>(server_tv_watching(tv)));
    server_tv_close(tv, at);
  }
  tv_off();
  if (rec_out.is_open()) {
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
  edit_off();
  server_close(s);
  close_all();
  return code;
}

int main_body(int argc, char** argv, const ServerGame& game, const console::ManualInfo& manual) {
  std::vector<std::string> args;
  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--help") == 0) {
      std::fputs(server_help(game).c_str(), stdout);
      return 0;
    }
    args.emplace_back(argv[i]);
  }
  console::Console con(true);
  // THE MANUAL (`--manual`): the program, its game and every flag, its own main's first.
  console::ManualInfo info = manual;
  if (info.program.id.empty())
    info.program = {game.binary, std::string(game.title) + " server", {}};
  for (console::FlagDoc& f : server_flag_docs(game)) info.flags.push_back(std::move(f));
  con.set_manual(std::move(info));
  con.set_sink([](std::string_view line) {
    std::printf("%.*s\n", static_cast<int>(line.size()), line.data());
  });
  // The clock's macros ($com_date, $com_time, $com_uptime, $com_uptime_long, $random) for an
  // operator's scripts and messages, as a player's console has them.
  const auto born = std::chrono::steady_clock::now();
  console::add_clock_macros(
      con,
      {.local =
           [] {
             const std::time_t t = std::time(nullptr);
             std::tm out{};
#if defined(_WIN32)
             localtime_s(&out, &t);
#else
             localtime_r(&t, &out);
#endif
             return out;
           },
       .uptime_s =
           [born] {
             return std::chrono::duration<double>(std::chrono::steady_clock::now() - born).count();
           },
       .seed = static_cast<uint32_t>(wall_unix_us())});
  // `macrolist [filter]`: each `$` macro (its name in 16 columns, then what it gives now), the
  // filter a wildcard on the name; then "<shown> of <all> macros".
  con.add_command("macrolist", "macrolist [filter]: every $ macro and what it gives now",
                  [](console::Console& cn, const console::Args& a) {
                    cn.print(server_macro_list(cn, a.size() > 1 ? a[1] : ""));
                  });
  ServerConfig c;
  // Each line the terminal prints, `console_prefix` before it (read at every line).
  con.set_sink([&c](std::string_view line) {
    const std::time_t t = std::time(nullptr);
    std::tm now{};
#if defined(_WIN32)
    localtime_s(&now, &t);
#else
    localtime_r(&t, &now);
#endif
    terminal_print(terminal_prefix(c.console_prefix, LineLevel::normal, now), line);
  });
  ServerCfg cfg;
  AdminFeatures admin;
  Features features;
  if (game.serve == nullptr)
    features.list = admin_features(admin);
  else
    features.list.push_back(admin_log_feature(admin));  // the game's own: its lines, sv_logfile
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
  bool timed = false;  // --for given
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
    const ServerFlag* flag = nullptr;
    for (const ServerFlag& f : kServerFlags)
      if (key == f.flag) flag = &f;
    if (flag == nullptr) {
      error = "unknown flag '" + a + "'; see " + game.binary + " --help";
      return false;
    }
    for (const ServerFlag& f : kServerFlags) {
      if (key != f.flag || f.cvar == nullptr) continue;
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
      timed = true;
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
    } else if (key == "--manual") {
      c.manual = val;
    } else {
      error = "unknown flag '" + a + "'; see " + game.binary + " --help";
      return false;
    }
  }
  // Started with no run of its own asked for (no --for, --offline or --replay): a server people
  // play on, paced to the wall clock until it is stopped.
  if (!timed && !c.offline && c.replay.empty()) {
    c.seconds = 0;
    c.realtime = true;
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
  if (config.fs_autoexec == 0) return true;  // the folders switched, no config run again
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
      "(map dm2 +set sv_hostname \"Arena\"). Each flag sets the cvar beside it.\n";
  int group = -1;
  for (const ServerFlag& f : kServerFlags) {
    if (f.group != group) out += '\n';
    group = f.group;
    std::string lead = std::string("  ") + f.flag;
    if (*f.arg != '\0') lead += (f.arg[0] == '[' ? "" : "=") + std::string(f.arg);
    if (lead.size() < 21) lead.resize(21, ' ');
    out += lead;
    if (f.cvar != nullptr) out += std::string(f.cvar) + ": ";
    out += f.help;
    if (const std::string d = flag_default(f, game); !d.empty()) out += " (" + d + ")";
    out += '\n';
  }
  return out;
}

std::vector<console::FlagDoc> server_flag_docs(const ServerGame& game) {
  std::vector<console::FlagDoc> out;
  for (const ServerFlag& f : kServerFlags) {
    std::string help = f.help;
    if (!help.empty())
      help[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(help[0])));
    if (const std::string d = flag_default(f, game); !d.empty()) help += " (" + d + ")";
    help += '.';
    if (f.cvar != nullptr) help += " Sets " + std::string(f.cvar) + ".";
    std::string arg = f.arg;
    out.push_back({std::string(f.flag) + (arg.empty() || arg[0] == '[' ? "" : "="), arg, help});
  }
  return out;
}

int server_main(int argc, char** argv, const ServerGame& game, const console::ManualInfo& manual) {
  // A line at a time even into a file or a pipe (a server started detached writes its log there),
  // so the log reads as the server runs and a crash loses nothing printed before it.
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  // What nothing else caught ends the program here, with a line that says so, rather than in
  // std::terminate's bare abort.
  try {
    return main_body(argc, argv, game, manual);
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

void add_game_words(console::Console& con, const ServerGame& game,
                    std::function<GameWordRun(const std::string& line)> run) {
  const std::string& pass = con.manual().pass;
  const console::CvarLayer layer(con.cvars(),
                                 pass.empty() ? "game" : "game:" + pass.substr(0, pass.find(',')));
  const auto shared =
      std::make_shared<std::function<GameWordRun(const std::string&)>>(std::move(run));
  for (int i = 0; i < game.word_count; i++) {
    const GameWord& w = game.words[i];
    if (con.commands().count(w.name) != 0) continue;
    con.add_command(w.name, w.help, [shared](console::Console& c, const console::Args& a) {
      switch ((*shared)(a.raw)) {
        case GameWordRun::taken:
          break;
        case GameWordRun::no_game:
          c.print(a[0] + ": no map is running");
          break;
        case GameWordRun::refused:
          c.print(a[0] + ": the game did not take that line");
          break;
      }
    });
    if (w.also != nullptr && con.commands().count(w.also) == 0)
      con.add_command_synonym(w.also, w.name);
  }
}

}  // namespace ge::server
