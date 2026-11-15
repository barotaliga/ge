// THE CONFIG CORPUS: players' configs, as advanced as the ones players carry, run on the console
// and held golden (M18k: "a corpus of advanced configs runs golden"; docs/shooters-edge.md
// § Configs). Each directory of tests/golden/console_configs/ is one player's files: their cfgs
// and a `play.txt` of what happens once the game has started (keys pressed, lines heard from the
// game, frames run). The console starts as a game's does (config.cfg, then autoexec.cfg), plays
// the script, and the test holds, in `golden.txt`, everything it printed and the config it saves
// at the end. A change to the language that moves a line rewrites the golden under
// GE_GOLDEN_WRITE=1, and the PR says why the new line is right.
//
// play.txt's lines, one action each:
//   press <key> / release <key> / tap <key>   a key goes down, up, or down and up (each runs a
//   frame) heard <text>                              a line from the game (a chat or a server
//   print) state <name> <value>                      the player's state a team message reads
//   ($health) frame                                     one frame of the buffer anything else typed
//   at the console and run to the end
//
// The game the configs run in: a few settings of every first-person game, its held buttons, and
// `impulse`, `say` and `say_team` printing what they would send.
//
// ge:summary Tests a corpus of players' advanced configs on the console, held golden
// ge:keywords test console config corpus golden player cfg autoexec scripts binds triggers
// ge:keywords weapon-scripts teamplay hud-config legacy-config
// ge:skill console testing
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/cfg.hpp"
#include "console/console.hpp"

using namespace ge::console;
namespace fs = std::filesystem;

namespace {

std::string read_text(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::ostringstream s;
  s << in.rdbuf();
  return s.str();
}

// The game's side of the console: settings, buttons and the commands that would reach a server.
void add_game(Console& c, std::map<std::string, std::string>& state) {
  auto& v = c.cvars();
  v.add({.name = "sensitivity", .fallback = "3", .flags = kArchive, .min = 0.1, .max = 100});
  v.add({.name = "fov", .fallback = "90", .flags = kArchive, .min = 10, .max = 140});
  v.add({.name = "crosshair", .fallback = "1", .flags = kArchive, .min = 0, .max = 7});
  v.add({.name = "volume", .fallback = "0.7", .flags = kArchive, .min = 0, .max = 1});
  v.add({.name = "m_pitch", .fallback = "0.022", .flags = kArchive, .min = -1, .max = 1});
  v.add({.name = "r_drawviewmodel", .fallback = "1", .flags = kArchive, .min = 0, .max = 1});
  v.add({.name = "cl_rollangle", .fallback = "2", .flags = kArchive, .min = 0, .max = 45});
  v.add({.name = "name",
         .fallback = "player",
         .flags = kArchive | kUserInfo,
         .kind = CvarKind::text});
  v.add({.name = "team", .fallback = "", .flags = kArchive | kUserInfo, .kind = CvarKind::text});
  for (const char* b :
       {"attack", "jump", "forward", "back", "moveleft", "moveright", "use", "mlook", "showscores"})
    c.add_button(b, "a held action");
  c.add_command("impulse", "impulse <n>: a weapon or an item",
                [](Console& con, const Args& a) { con.print("[impulse " + a[1] + "]"); });
  c.add_command("say", "say <text>",
                [](Console& con, const Args& a) { con.print("[say] " + a.rest(0)); });
  c.add_command("say_team", "say_team <text>",
                [](Console& con, const Args& a) { con.print("[say_team] " + a.rest(0)); });
  c.add_command("play", "play <sound>",
                [](Console& con, const Args& a) { con.print("[play " + a[1] + "]"); });
  for (const char* m : {"health", "armor", "ammo", "weapon", "location"})
    c.add_macro(m, [&state, m] {
      const auto it = state.find(m);
      return it == state.end() ? std::string("0") : it->second;
    });
}

// One player's files run from startup through their play script; what the console printed and
// the config it saves at the end.
std::string run_player(const fs::path& dir) {
  auto files = std::make_shared<std::map<std::string, std::string>>();
  for (const auto& e : fs::directory_iterator(dir))
    if (e.path().extension() == ".cfg")
      (*files)[e.path().filename().string()] = read_text(e.path());
  std::map<std::string, std::string> state;
  Console c(false);
  add_game(c, state);
  c.set_files(
      [files](std::string_view name, std::string& text) {
        const auto it = files->find(std::string(name));
        if (it == files->end()) return false;
        text = it->second;
        return true;
      },
      [files](std::string_view name, std::string_view text) {
        (*files)[std::string(name)] = std::string(text);
        return true;
      });
  std::vector<std::string> out;
  auto flush = [&] {
    for (const std::string& l : c.output()) out.push_back(l);
    c.clear_output();
  };
  start(c, {});
  flush();
  std::istringstream play(read_text(dir / "play.txt"));
  std::string line;
  while (std::getline(play, line)) {
    if (line.empty() || line.rfind("//", 0) == 0) continue;
    out.push_back("> " + line);
    const size_t sp = line.find(' ');
    const std::string verb = line.substr(0, sp);
    const std::string rest = sp == std::string::npos ? std::string() : line.substr(sp + 1);
    if (verb == "press" || verb == "release" || verb == "tap") {
      if (verb != "release") {
        c.key(rest, true);
        c.run();
      }
      if (verb != "press") {
        c.key(rest, false);
        c.run();
      }
    } else if (verb == "heard") {
      c.message(rest);
      c.run();
    } else if (verb == "state") {
      const size_t s2 = rest.find(' ');
      state[rest.substr(0, s2)] = rest.substr(s2 + 1);
    } else if (verb == "frame") {
      c.run();
    } else {
      c.execute(line);
    }
    flush();
  }
  c.execute("cfg_save");
  flush();
  out.push_back("--- config.cfg as saved");
  std::istringstream saved((*files)["config.cfg"]);
  while (std::getline(saved, line)) out.push_back(line);
  std::string text;
  for (const std::string& l : out) text += l + "\n";
  return text;
}

bool env_set(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool set = _dupenv_s(&v, &n, name) == 0 && v != nullptr;
  std::free(v);
  return set;
#else
  return std::getenv(name) != nullptr;
#endif
}

}  // namespace

TEST(corpus) {
  const fs::path root = ge::test::root_path("tests/golden/console_configs");
  int players = 0;
  for (const auto& e : fs::directory_iterator(root)) {
    if (!e.is_directory()) continue;
    ++players;
    const std::string got = run_player(e.path());
    const fs::path golden = e.path() / "golden.txt";
    if (env_set("GE_GOLDEN_WRITE")) {
      std::ofstream(golden, std::ios::binary) << got;
      continue;
    }
    CHECK_MSG(got == read_text(golden),
              "the config '" + e.path().filename().string() +
                  "' ran unlike its golden: rerun with GE_GOLDEN_WRITE=1 and read the diff of " +
                  (fs::path("tests/golden/console_configs") / e.path().filename() / "golden.txt")
                      .generic_string());
  }
  CHECK_MSG(players >= 5, "the corpus holds " + std::to_string(players) +
                              " configs: check tests/golden/console_configs/ is there");
}
