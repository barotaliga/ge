// Shooter's Edge's player CLI's Spec and its own commands (spec.hpp).
//
// ge:summary Shooter's Edge's player CLI: the Spec's functions and the demos commands
// ge:keywords shooters_edge_cli locate join_words listen_words demos demos-play
// ge:skill shooter agent-interface
// ge:test tests/gamecli_cli_test.cpp
#include "shooters_edge/cli/spec.hpp"

#include <algorithm>
#include <filesystem>
#include <memory>

#include "capture/screenshot.hpp"
#include "classicenhance/cli/commands.hpp"
#include "classicenhance/profile.hpp"
#include "console/view.hpp"
#include "core/json_write.hpp"
#include "gamecli/process.hpp"
#include "shooters_edge/client/controls.hpp"
#include "shooters_edge/client/coop_mode.hpp"
#include "shooters_edge/client/game_folder.hpp"
#include "shooters_edge/client/settings.hpp"
#include "shooters_edge/hud/layout.hpp"
#include "shooters_edge/menu/game_menu.hpp"
#include "shooters_edge/net/classic_browse.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/view/hand.hpp"

namespace shooters_edge::cli {

namespace {

namespace fs = std::filesystem;
namespace gc = ge::gamecli;
namespace cl = shooters_edge::client;
using ge::core::json_arr;
using ge::core::json_num;
using ge::core::json_obj;
using ge::core::json_str;

// The window's choice of install and game folder, from the same flags and the same kept game.
cl::Args folders(const gc::Ctx& ctx) {
  cl::Args a;
  if (ctx.has("basedir")) {
    a.basedir = cl::expand_home(ctx.flag("basedir"));
    a.basedir_set = true;
  }
  if (ctx.has("game")) {
    a.game = ctx.flag("game");
    a.game_set = true;
  }
  cl::choose_folder(a, gc::prefs_dir("gameengine2", "shooters_edge"));
  std::error_code ec;
  fs::path abs = fs::absolute(a.basedir, ec).lexically_normal();
  if (!ec && abs.filename().empty()) abs = abs.parent_path();  // "dir/" read as "dir"
  if (!ec) a.basedir = abs.string();
  return a;
}

// Every player setting the libraries without a window register, as the window registers them
// (client/main.cpp): the console's (its look, the player's name), the controls, the user info, the
// co-op options, the HUD, the held weapons, the menus (the browser's sb_ settings among them) and
// the screenshots. The window's own (the picture, the window, the pacing) come from its catalog.
struct Registered {
  cl::ControlSettings controls;
  cl::UserInfo user_info;
  cl::CoopMode coop;
  shooters_edge::hud::Hud hud = shooters_edge::hud::make_hud();
  shooters_edge::view::ViewSettings view;
  shooters_edge::menu::GameMenu menu;
  ge::capture::ScreenshotSettings shots;
};

void add_cvars(ge::console::Console& con) {
  con.keep(std::make_shared<ge::console::ConsoleView>(con));  // con_ and the player's name
  auto r = std::make_shared<Registered>();
  cl::add_coop_cvars(con, r->coop);
  cl::add_cvars(con, r->controls);
  shooters_edge::view::add_view_cvars(con.cvars(), r->view);
  cl::add_user_info_cvars(con, r->user_info);
  shooters_edge::hud::add_cvars(con, r->hud);
  r->menu.add_cvars(con);
  ge::capture::add_screenshot_cvars(con.cvars(), r->shots);
  con.keep(r);
}

bool locate(const gc::Ctx& ctx, gc::Places& out, std::string& error) {
  const cl::Args a = folders(ctx);
  if (!fs::is_directory(a.basedir)) {
    error = "the install " + a.basedir + " is not a folder";
    return false;
  }
  out.install = a.basedir;
  out.settings = cl::settings_dir(a.basedir, a.game).string();
  out.masters = shooters_edge::net::classic_install_masters(a.basedir);
  return true;
}

void join_words(const gc::Ctx& ctx, const std::string& address, std::vector<std::string>& w) {
  const cl::Args a = folders(ctx);
  w.insert(w.end(), {"-basedir", a.basedir, "-game", a.game, "+connect", address});
}

void listen_words(const gc::Ctx& ctx, const gc::HostAsk& ask, std::vector<std::string>& w) {
  const cl::Args a = folders(ctx);
  // As HOST GAME's START does it: the server's lines (`sv <name> <value>`), then the map.
  w.insert(w.end(), {"-basedir", a.basedir, "-game", a.game, "+sv_port", std::to_string(ask.port),
                     "+sv", "deathmatch", "1"});
  if (ask.maxclients > 0) w.insert(w.end(), {"+sv", "maxclients", std::to_string(ask.maxclients)});
  const std::string map =
      ask.map.empty() ? std::string(ge::classicenhance::classic_profile().start_map) : ask.map;
  w.insert(w.end(), {"+map", map});
}

// The recordings in the game folder and its demos/ folder, by name.
std::vector<fs::path> demo_files(const cl::Args& a) {
  std::vector<fs::path> out;
  const fs::path game = fs::path(a.basedir) / a.game;
  for (const fs::path& dir : {game, game / "demos"}) {
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
      const std::string ext = e.path().extension().string();
      for (std::string_view known : profiles::kClassicDemos)
        if (ext == known) out.push_back(e.path());
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

int cmd_demos(gc::Ctx& ctx) {
  if (!gc::need_places(ctx)) return 1;
  const cl::Args a = folders(ctx);
  std::vector<std::string> rows;
  std::string text;
  for (const fs::path& f : demo_files(a)) {
    std::error_code ec;
    const auto bytes = fs::file_size(f, ec);
    rows.push_back(json_obj({{"name", json_str(f.stem().string())},
                             {"file", json_str(f.string())},
                             {"bytes", json_num(static_cast<long long>(bytes))}}));
    text += f.stem().string() + "  " + std::to_string(bytes / 1024) + " KiB  " + f.string() + "\n";
  }
  return gc::answer(ctx, json_obj({{"demos", json_arr(rows)}}), text,
                    std::to_string(rows.size()) + " recordings in " + a.basedir + "/" + a.game,
                    rows.empty() ? "record one in the game's console: record <name>"
                                 : "shooters_edge_cli demos play <name> watches one");
}

int cmd_demos_play(gc::Ctx& ctx) {
  if (ctx.words.size() != 1)
    return gc::refuse(ctx, 2, "demos play takes one recording's name",
                      "shooters_edge_cli demos (lists them)");
  if (!gc::need_places(ctx)) return 1;
  const cl::Args a = folders(ctx);
  fs::path file;
  for (const fs::path& f : demo_files(a))
    if (f.stem() == ctx.words[0] || f.filename() == ctx.words[0]) file = f;
  if (file.empty())
    return gc::refuse(ctx, 1, "no recording is called '" + ctx.words[0] + "'",
                      "shooters_edge_cli demos");
  const std::vector<std::string> argv = {
      gc::program_path(ctx.flag("bin", ctx.self_dir), "shooters_edge"),
      "-basedir",
      a.basedir,
      "-game",
      a.game,
      "-playdemo",
      file.string()};
  if (!fs::exists(argv[0]))
    return gc::refuse(ctx, 1, argv[0] + " is not there",
                      "build it, or pass --bin=<the folder holding shooters_edge>");
  const std::string log = (fs::path(ctx.places.logs) / "demo.log").string();
  std::error_code ec;
  fs::create_directories(ctx.places.logs, ec);
  long pid = 0;
  std::string error;
  if (!gc::spawn_detached(argv, "", log, pid, error))
    return gc::refuse(ctx, 1, error, "read " + log);
  return gc::answer(ctx, json_obj({{"file", json_str(file.string())}, {"pid", json_num(pid)}}),
                    "playing " + file.string(), "", "shooters_edge_cli logs demo says what it did");
}

gc::Spec make() {
  gc::Spec s;
  s.game = "shooters_edge";
  s.title = "Shooter's Edge";
  s.cli = "shooters_edge_cli";
  s.window = "shooters_edge";
  s.server = "shooters_edge_server";
  s.prefs_app = "shooters_edge";
  s.default_port = 27500;
  s.lan_first = shooters_edge::net::kLanFirstPort;
  s.lan_last = shooters_edge::net::kLanLastPort;
  s.master_port = 27000;
  s.query = shooters_edge::net::classic_query();
  s.server_game = &shooters_edge::net::classic_game();
  s.rcon = shooters_edge::net::classic_rcon;
  s.read_rcon = shooters_edge::net::classic_read_print;
  s.add_cvars = add_cvars;
  s.locate = locate;
  s.join_words = join_words;
  s.listen_words = listen_words;
  s.commands = ge::classicenhance::cli::commands();
  s.commands.push_back({"demos", "", "the recordings in the game folder: name, size, file",
                        "demos --json", "shooters_edge", cmd_demos});
  s.commands.push_back({"demos play", "<name>", "watch a recording in the window",
                        "demos play final", "shooters_edge", cmd_demos_play});
  s.flags = {{"basedir", "<dir>", "the classic game's install (default: the one loaded last)"},
             {"game", "<folder>", "the game folder over it (default: the multiplayer one)"}};
  s.notes =
      "Shooter's Edge plays the player's own classic packs: `--basedir` is their install (the "
      "game remembers the last one loaded, so it is rarely needed), and `content` says which game "
      "and release the packs are. Servers speak the classic protocol, so `servers` lists this "
      "game's servers and the classic ones already on the internet alike; a classic server's "
      "address is usually `host:27500`. The player's name is the setting `name`, the colours "
      "`topcolor` and `bottomcolor`; `cl_speedjump 1` asks a server for the easy jump. "
      "`demos` lists recordings and `demos play <name>` watches one.";
  return s;
}

}  // namespace

const gc::Spec& spec() {
  static const gc::Spec s = make();
  return s;
}

}  // namespace shooters_edge::cli
