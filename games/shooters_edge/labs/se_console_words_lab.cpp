// LAB `se-console-words`: WHAT A PLAYER READS WHEN THE GAME SAYS NO. The window's own console
// (the lobby's, the remote console's, the administrator's and the recorder's commands, wired as
// client/main.cpp wires them) with a player typing the lines a player types: `addbot` in a
// single-player game, `ready` with no lobby, `users`, `rcon` while not connected, `record`, a
// command misspelt, `toggle` and `exec` without their words. With `--game=<folder>` (a game
// folder of packs) and `--map=<name>` the packs' own single-player program runs that map, as NEW
// GAME starts it, so the lines answer from inside a game; `--backdrop=<png>` (a se-client shot of
// that map) is the game drawn behind the console. Each answer is held to the player-text rule
// (docs/architecture.md § Player text): a game's words, never an agent's prompt ("next:",
// "skill:", backticks).
//
// ge:summary Lab: the console's refusals as a player reads them, over a real map, held to the rule
// ge:keywords lab se-console-words console refusal player-text error-message addbot coopgame
// ge:keywords agent-syntax next-hint wording
// ge:skill agent-interface shooter labs
// ge:test tests/cli_diagnostics_test.cpp
#include <algorithm>
#include <cctype>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "console/console.hpp"
#include "console/view.hpp"
#include "lab/lab.hpp"
#include "lab/png.hpp"
#include "progs/file.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/listen_record.hpp"
#include "shooters_edge/client/lobby_console.hpp"
#include "shooters_edge/client/record_cvars.hpp"
#include "shooters_edge/net/classic_admin.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/net/classic_remote.hpp"
#include "ui/console.hpp"
#include "ui/raster.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace sn = shooters_edge::net;
namespace con = ge::console;

// The lines typed, in order: each one a refusal a player meets.
constexpr const char* kTyped[] = {
    "coopgam", "toggle", "rcon secret status", "reconnect", "record match#1", "users", "speedjump",
    "ready",   "addbot",
};

struct Said {
  std::string typed;
  std::vector<std::string> answer;
};

// Whether a player's line reads like an agent's prompt.
bool agent_syntax(const std::string& line) {
  std::string low = line;
  for (char& c : low) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
  for (const char* word : {"next:", "skill:"})
    for (size_t at = low.find(word); at != std::string::npos; at = low.find(word, at + 1)) {
      const char before = at == 0 ? ' ' : low[at - 1];
      if (!(std::isalnum(static_cast<unsigned char>(before)) || before == '_')) return true;
    }
  return std::count(line.begin(), line.end(), '`') >= 2;
}

void draw(gl::Picture& p, const con::ConsoleView& v) {
  ge::ui::Image img;
  img.width = p.width, img.height = p.height;
  img.rgba = std::move(p.rgba);
  ge::ui::DrawList dl;
  dl.begin(img.width, img.height);
  ge::ui::draw_console(dl, v);
  ge::ui::render(dl, img);
  p.rgba = std::move(img.rgba);
}

// The game behind the console: `--backdrop`'s picture (its top 16:9, so a lab shot's picture
// without its caption band), scaled to w x h; else a dark wall.
gl::Picture backdrop(gl::Lab& lab, int w, int h) {
  gl::Picture p = gl::create_picture(w, h, {24, 20, 16});
  if (lab.arg("backdrop").empty()) return p;
  const std::optional<gl::Picture> from = gl::read_png(lab.arg("backdrop"));
  if (!from || from->width <= 0) {
    lab.fail("--backdrop " + lab.arg("backdrop") + " did not read as a PNG",
             "give a picture a lab wrote (previews/se-client@<tag>/enhanced-0.png); skill: labs.");
    return p;
  }
  const int fh = std::min(from->height, from->width * 9 / 16);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      const size_t src = (static_cast<size_t>(y * fh / h) * static_cast<size_t>(from->width) +
                          static_cast<size_t>(x * from->width / w)) *
                         4;
      const size_t dst =
          (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 4;
      for (int k = 0; k < 4; k++) p.rgba[dst + k] = from->rgba[src + k];
    }
  return p;
}

void run(gl::Lab& lab) {
  cl::SearchPath files;
  ge::progs::Progs progs;
  cl::Listen listen;
  const std::string folder = lab.arg("game");
  if (!folder.empty()) {
    std::string error;
    const size_t cut = folder.find_last_of('/');
    const std::string base = cut == std::string::npos ? "." : folder.substr(0, cut);
    const std::string game = cut == std::string::npos ? folder : folder.substr(cut + 1);
    std::vector<uint8_t> store;
    if (!cl::open_game_dir(base, game, files, error)) {
      lab.fail(error, "pass --game=<the folder holding the packs>; skill: shooter.");
      return;
    }
    const std::span<const uint8_t> bytes = cl::find_file(files, "progs.dat", store);
    if (!ge::progs::load_progs(bytes.data(), bytes.size(), progs, error, ge::progs::every_form())) {
      lab.fail("se-console-words: the pack's progs.dat: " + error,
               "pass a game folder whose packs hold a progs.dat; skill: progs.");
      return;
    }
    cl::ListenOptions lo = sn::listen_options();
    lo.files = &files;
    lo.console.emplace_back("deathmatch 0");  // as NEW GAME starts it
    std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, lab.arg("map"), error);
    if (!map || !cl::listen_start(listen, progs, std::move(map), lo, error)) {
      lab.fail(error, "check --map names a map of the packs; skill: shooter.");
      return;
    }
  }

  // The window's console, wired as client/main.cpp wires it.
  con::Console console;
  std::vector<std::string> printed;
  console.set_sink([&printed](std::string_view line) { printed.emplace_back(line); });
  cl::add_lobby_commands(console, listen);
  sn::Remote remote;
  sn::add_remote_commands(console, remote);
  sn::add_admin_commands(console, listen, remote);
  cl::ListenRecorder recorder;
  cl::RecordSettings record;
  cl::add_record_commands(console, recorder, listen, files, record);

  std::vector<Said> said;
  size_t heard = listen.prints.size();
  int agent = 0, lines = 0;
  for (const char* typed : kTyped) {
    printed.clear();
    console.execute(typed);
    Said s{typed, printed};
    for (; heard < listen.prints.size(); heard++) s.answer.push_back(listen.prints[heard]);
    for (const std::string& line : s.answer) {
      lines++;
      if (agent_syntax(line)) agent++;
    }
    said.push_back(std::move(s));
  }
  lab.metric("lines_answered", lines, "lines");
  lab.metric("lines_in_agent_syntax", agent, "lines");
  lab.metric("in_a_game", listen.server != nullptr ? 1 : 0, "0/1");

  gl::LabTable table{"each line typed and the game's answer", {"typed", "answer"}, {}};
  for (const Said& s : said)
    for (size_t k = 0; k < s.answer.size() || k == 0; k++)
      table.rows.push_back({k == 0 ? s.typed : "", k < s.answer.size() ? s.answer[k] : ""});
  lab.table(std::move(table));

  con::Console shown;
  con::ConsoleView v(shown);
  shown.execute("con_height 0.75");
  for (const Said& s : said) {
    v.add(con::LineKind::command, "]" + s.typed);
    for (const std::string& line : s.answer) v.add(con::LineKind::output, line);
  }
  v.set_open(true);
  for (int f = 0; f < 600 && v.drop() < 1; f++) v.frame(1.0 / 60);
  const int w = static_cast<int>(lab.arg_num("width"));
  const int h = static_cast<int>(lab.arg_num("height"));
  gl::Picture shot = backdrop(lab, w, h);
  draw(shot, v);
  lab.shot({"console",
            "THE CONSOLE SAYS NO",
            "A player types the lines a player types, and the game refuses each one in its own "
            "words: a bot asked for in a single-player game, a lobby word with no lobby, the "
            "administrator's words while not connected, a recording with a bad name, a command "
            "misspelt, commands missing their words.",
            "Every answer is a plain sentence with, where it helps, what to type instead; none "
            "carries next:, skill: or backtick marks (lines_in_agent_syntax)",
            {{"map", lab.arg("map")}}},
           shot);
}

const gl::LabSpec kSpec{
    "se-console-words",
    gl::LabKind::sim,
    "game:shooters_edge",
    "the console's refusals as a player reads them (docs/architecture.md § Player text)",
    "agent-interface",
    "Does every refusal a player meets in the console read like a game's own words, with no "
    "agent syntax?",
    {
        {"game", "", "a game folder of packs whose single-player program runs --map; empty: none"},
        {"map", "e1m1", "the map the program runs (with --game)"},
        {"backdrop", "", "a PNG of the game behind the console (a se-client shot); empty: none"},
        {"width", "1280", "the picture's width, px"},
        {"height", "720", "the picture's height, px"},
    },
    "--width=640 --height=360",
    run,
};
GE_LAB(kSpec);

}  // namespace
