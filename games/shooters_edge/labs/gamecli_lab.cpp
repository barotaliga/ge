// LAB `gamecli`: the player's CLI an agent drives for a player (`shooters_edge_cli`,
// engine/gamecli), run through the same code the executable runs, session by session, each drawn
// as the terminal an agent or a person reads: the settings changed and refused, the servers found
// and who is on one, the remote console, hosting and joining (as dry runs, so nothing starts), a
// server's settings and their layers, the JSON answers, and the skill it prints. The servers are
// a stand-in on the loopback answering the classic status query and remote console, unless
// `--server=<host:port>` names a real one; `--basedir=<install>` adds the packs' own `content`.
//
// The terminal pictures carry pings, process ids and the stand-in's port, so they are measured
// (shown, not hashed); each command's exit code and what the scripted answers must hold are the
// digest.
//
// ge:summary Lab: shooters_edge_cli's sessions (settings, servers, rcon, host, join) as terminals
// ge:keywords lab gamecli player-cli shooters_edge_cli terminal session settings servers players
// ge:keywords rcon host join dry-run skill json
// ge:skill agent-interface labs
// ge:test tests/gamecli_cli_test.cpp
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "charset/pixel.hpp"
#include "console/catalog.hpp"
#include "console/console.hpp"
#include "gamecli/gamecli.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "net/udp_socket.hpp"
#include "shooters_edge/cli/spec.hpp"

namespace {

namespace fs = std::filesystem;
namespace gl = ge::lab;
namespace gn = ge::net;

// A server on the loopback answering the status query and the remote console as a classic
// server does, until the lab is done.
struct StandIn {
  gn::UdpSocket sock;
  std::atomic<bool> stop{false};
  std::thread thread;
  bool open() {
    if (!gn::udp_open(sock, gn::loopback_v4(0), nullptr)) return false;
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
              "\\hostname\\Friday frags\\map\\e1m2\\maxclients\\8\\deathmatch\\1\\teamplay\\1"
              "\\fraglimit\\50\n"
              "3 27 12 38 \"Ann\" \"base\" 4 4 \"red\"\n"
              "4 19 12 52 \"Bo\" \"base\" 13 13 \"blue\"\n"
              "5 22 9 41 \"Cy\" \"base\" 4 4 \"red\"\n";
        } else if (text.rfind("rcon secret ", 0) == 0) {
          const std::string cmd(text.c_str() + 12);
          reply +=
              cmd == "status" ? "map: e1m2\nplayers: 3 active (8 max)\n" : "\"" + cmd + "\" done\n";
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

struct Line {
  std::string text;
  bool typed;
};

// A terminal: each line wrapped at `cols`, the typed ones after a prompt.
gl::Picture terminal(const std::vector<Line>& lines, int cols) {
  constexpr int kCellW = 9, kCellH = 18, kPad = 14;
  std::vector<std::pair<std::string, bool>> rows;
  for (const Line& l : lines) {
    std::string t = l.typed ? "$ " + l.text : l.text;
    do {
      rows.push_back({t.substr(0, static_cast<size_t>(cols)), l.typed});
      t = t.size() > static_cast<size_t>(cols) ? "  " + t.substr(static_cast<size_t>(cols)) : "";
    } while (!t.empty());
  }
  const int w = cols * kCellW + 2 * kPad;
  const int h = static_cast<int>(rows.size()) * kCellH + 2 * kPad + 22;
  gl::Picture p = gl::create_picture(w, h, {13, 17, 23});
  gl::fill_rect(p, 0, 0, w, 22, {33, 38, 46});
  gl::fill_disc(p, 14, 11, 5, {237, 106, 94});
  gl::fill_disc(p, 30, 11, 5, {245, 191, 79});
  gl::fill_disc(p, 46, 11, 5, {98, 197, 84});
  std::vector<uint8_t> cell(static_cast<size_t>(kCellW * kCellH));
  int y = kPad + 22;
  for (const auto& [text, typed] : rows) {
    int x = kPad;
    const bool prompt = typed && text.rfind("$ ", 0) == 0;
    for (size_t i = 0; i < text.size(); i++) {
      const auto code = static_cast<uint8_t>(text[i]);
      std::fill(cell.begin(), cell.end(), uint8_t{0});
      ge::charset::render_terminal(code, kCellW, kCellH, cell.data(), kCellW);
      const gl::Rgba ink = prompt && i < 2 ? gl::Rgba{98, 197, 84}
                           : typed         ? gl::Rgba{230, 237, 243}
                                           : gl::Rgba{160, 172, 186};
      for (int cy = 0; cy < kCellH; cy++)
        for (int cx = 0; cx < kCellW; cx++)
          if (cell[static_cast<size_t>(cy) * kCellW + static_cast<size_t>(cx)] > 127)
            gl::set_pixel(p, x + cx, y + cy, ink);
      x += kCellW;
    }
    y += kCellH;
  }
  return p;
}

std::string replace_all(std::string s, const std::string& from, const std::string& to) {
  if (from.empty()) return s;
  for (size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size()))
    s.replace(at, from.size(), to);
  return s;
}

void run(gl::Lab& lab) {
  const fs::path scratch = fs::temp_directory_path() / "ge_gamecli_lab";
  std::error_code ec;
  fs::remove_all(scratch, ec);
  const std::string given = lab.arg("basedir");
  const fs::path install = given.empty() ? scratch / "install" : fs::path(given);
  fs::create_directories(install / "id1", ec);
  // The catalog the window writes beside the player's config when it saves (console/catalog.hpp),
  // here from the settings the CLI registers itself: names and ranges are checked as in a game
  // that was started once.
  {
    ge::console::Console con(false);
    shooters_edge::cli::spec().add_cvars(con);
    fs::create_directories(scratch / "settings", ec);
    std::ofstream(scratch / "settings" / std::string(ge::console::kCatalogFile))
        << ge::console::catalog_text(con.cvars());
  }
  StandIn standin;
  std::string server = lab.arg("server");
  if (server.empty()) {
    if (!standin.open()) {
      lab.fail("the stand-in server's loopback socket did not open",
               "next: run the lab where the loopback works, or pass --server=<host:port>");
      return;
    }
    server = standin.address();
  }
  // Paths shown as a player's: the scratch folders under the home folder's names.
  const std::string state = (scratch / "state").string(), logs = (scratch / "logs").string();
  const auto shown = [&](std::string s) {
    s = replace_all(s, logs, "~/.local/share/gameengine2/shooters_edge/logs");
    s = replace_all(s, state, "~/.local/share/gameengine2/shooters_edge/cli");
    s = replace_all(s, (scratch / "settings").string(), "~/games/classic/id1/settings");
    if (given.empty()) s = replace_all(s, install.string(), "~/games/classic");
    return s;
  };
  int step = 0;
  const auto session = [&](const char* id, const char* title, const char* caption,
                           const char* look_for,
                           const std::vector<std::pair<std::vector<std::string>, int>>& calls) {
    std::vector<Line> lines;
    for (const auto& [words, want] : calls) {
      std::string typed = "shooters_edge_cli";
      for (const std::string& w : words)
        typed += " " + (w.find(' ') != std::string::npos ? "\"" + w + "\"" : w);
      lines.push_back({shown(typed), true});
      std::vector<std::string> args = words;
      args.insert(args.begin(),
                  {"--basedir=" + install.string(), "--state=" + state, "--logs=" + logs,
                   "--settings=" + (scratch / "settings").string()});
      std::string out;
      const int rc = ge::gamecli::run(shooters_edge::cli::spec(), args, out, ".");
      lab.metric("step" + std::to_string(++step) + ".exit", rc, "count");
      if (rc != want) {
        std::string what = typed;
        what += " exited " + std::to_string(rc);
        what += ", not " + std::to_string(want) + ": ";
        what += out;
        lab.fail(what, "next: run it by hand with --json and read its error");
      }
      size_t at = 0;
      int kept = 0;
      while (at < out.size() && kept < 18) {
        const size_t nl = out.find('\n', at);
        lines.push_back(
            {shown(out.substr(at, nl == std::string::npos ? std::string::npos : nl - at)), false});
        at = nl == std::string::npos ? out.size() : nl + 1;
        kept++;
      }
      if (at < out.size()) lines.push_back({"  ...", false});
    }
    lab.shot({id, title, caption, look_for, {{"server", server}}}, terminal(lines, 104),
             gl::Det::measured);
  };

  session("settings", "A PLAYER'S SETTINGS",
          "An agent finds a setting by its words, reads it, changes it in the player's config.cfg "
          "(clamped and checked by the game's own registration), is told the nearest name for a "
          "typo, and lists what is off its default.",
          "the value moves from 0.066 to 0.09, the typo names sensitivity, the list shows the two "
          "changed",
          {{{"settings", "list", "sens"}, 0},
           {{"settings", "set", "sensitivity", "0.09"}, 0},
           {{"settings", "set", "name", "Big Ben"}, 0},
           {{"settings", "set", "sensitivty", "2"}, 1},
           {{"settings", "list", "--changed"}, 0}});
  session(
      "servers", "FINDING A FRIEND'S GAME",
      "The browser's servers (here one named server), who is on it, its rules, and a command "
      "on its remote console.",
      "Friday frags on e1m2 with three players, their frags and teams; the remote console's reply",
      {{{"servers", "--lan=0", "--server=" + server}, 0},
       {{"players", server}, 0},
       {{"rcon", server, "--password=secret", "--", "status"}, 0},
       {{"rcon", server, "--password=wrong", "--", "status"}, 0}});
  session("host", "HOSTING AND JOINING",
          "A dedicated server's settings written to its server.cfg and read back with where each "
          "came from; hosting in a server process and in the window, and joining, as dry runs.",
          "sv_hostname from server.cfg; the server's flags (--for=0, --realtime); +connect for the "
          "join",
          {{{"server", "set", "sv_hostname", "Friday frags"}, 0},
           {{"server", "settings", "sv_host"}, 0},
           {{"host", "--dedicated", "--map=e1m2", "--maxclients=8", "--bots=3", "--dry-run"}, 0},
           {{"host", "--map=e1m2", "--dry-run"}, 0},
           {{"join", "192.0.2.7:27500", "--dry-run"}, 0}});
  session("json", "THE ANSWERS AN AGENT READS",
          "Every command with --json answers one object: ok, result, summary and next; a refusal "
          "carries error and next, the exact step to take.",
          "ok true with the row; ok false with exit 1 and the next command",
          {{{"settings", "get", "sensitivity", "--json"}, 0},
           {{"players", server, "--json"}, 0},
           {{"settings", "set", "sv_gravity", "100", "--json"}, 1}});
  if (!given.empty())
    session("content", "THE PLAYER'S PACKS", "Classic enhance's group: the packs in the install.",
            "the release named and verified", {{{"content"}, 0}});
  std::string skill;
  ge::gamecli::run(shooters_edge::cli::spec(), {"skill"}, skill, ".");
  std::vector<Line> lines = {
      {"shooters_edge_cli skill > ~/.claude/skills/shooters-edge/SKILL.md", true}};
  size_t at = 0;
  for (int k = 0; k < 40 && at < skill.size(); k++) {
    const size_t nl = skill.find('\n', at);
    lines.push_back({skill.substr(at, nl - at), false});
    at = nl == std::string::npos ? skill.size() : nl + 1;
  }
  lab.shot({"skill",
            "THE SKILL IT PRINTS",
            "The SKILL.md an agent installs to drive the game, generated from the same command "
            "rows, so it never falls behind them.",
            "front matter, the loop, a row a command",
            {}},
           terminal(lines, 104), gl::Det::measured);
  lab.metric("skill.bytes", static_cast<double>(skill.size()), "count", gl::Det::measured);
  fs::remove_all(scratch, ec);
}

const gl::LabSpec kSpec{
    "gamecli",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "engine/gamecli (the player's game CLI) over games/shooters_edge/cli (shooters_edge_cli)",
    "agent-interface",
    "Can an agent change a player's settings, find a server and who is on it, run its remote "
    "console, host a game and join one with shooters_edge_cli alone?",
    {
        {"server", "", "a real server to ask, host:port (default: a stand-in on the loopback)"},
        {"basedir", "", "an install holding the classic packs: adds the content session"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
