// SHOOTER'S EDGE'S WINDOW: the game played on one machine. The game folder's packs are the search
// path (files.hpp), the baseline mod is compiled from the sources built into the game and runs as
// a LISTEN SERVER in this process with the player as its client (listen.hpp), and each frame the
// window reads the keys and the raw mouse (controls.hpp), steps the server by the frames the wall
// time makes due, and draws the map from the player's eyes (engine/camera's first-person rig:
// the view height, the stairs smoothed, the landing's dip) with the game's effects (world_fx.hpp:
// rocket trails, explosions and their light, the screen's blend), the weapon in hand (held.hpp:
// switched, kicked and flashing as the mod fires it), the HUD (hud/layout.hpp) and the console.
//
//   shooters_edge [-basedir <dir>] [-game <folder>] [-progs <file>] [-models <dir>]
//                 [-benchmark <demo>] [-playdemo <demo>] [--frames=N] [--size=WxH]
//                 [+<command> ...]
//
// `-basedir` is the folder that holds the game folder (`id1` unless `-game` names another) with
// its `pak0.pak`, `pak1.pak` ...; a leading `~` is the home folder, and a path with spaces is one
// argument when quoted. `-progs` runs a mod's progs file in place of the baseline mod, and
// `-models` names the held weapons' folder (by default the tree's
// `games/shooters_edge/content/models/view`, from where the game runs or where it was built). The
// `+` words are console lines run at start: `+map dm6` loads that map
// (`map arena` is the built-in test arena); with none, the map `start` when the packs have it,
// else the arena.
//
// THE KEYS: W A S D move, SPACE jumps, the left button fires, 1..8 pick a weapon, the wheel the
// next one; the mouse looks (captured while the window has it; a click takes it back); ESCAPE
// opens the main menu (menu/game_menu.hpp: drawn in the pack's own pictures when the packs carry
// them, the rows the game cannot do yet dimmed), and ESCAPE again goes back out of it; the key
// under ESCAPE drops the console, where `map <name>` changes the map, `sv
// <line>` is a line for the server's console (`sv timelimit 10`), `save <name>` and `load <name>`
// keep and bring back a single-player game (`<game folder>/saves/<name>.save`; `<name>.sav` the
// classic text save in the game folder, which `load` also finds by the bare name), `sensitivity
// <deg a count>` and `m_invert 0|1` set the look, `r_look classic|modern` and the other picture
// cvars (app/look.hpp: `r_fx_lights`, `r_fx_glow`, `r_bloom`, `r_lava_glow`, `r_slime_glow`) the
// world's look, the view-model cvars (view/hand.hpp: `r_drawviewmodel`, `r_viewmodel_fov`,
// `cl_gunx`, `cl_guny`, `cl_gunz`, `cl_gunsway`) the weapon in hand, `vid_fullscreen 0|1` the
// window over the whole screen (`vid_display n` which one, of those `vid_displays` lists), and
// `quit` quits.
//
// THE LOBBY (client/lobby_console.hpp): a match opens each map in WARMUP, everyone free to roam
// with nothing counted, until every player is `ready` (F1, and F1 again or `break` takes it back),
// then a countdown and the map restarted for the match; F2 (`team switch`) changes team, F3 shows
// or hides the roster, `addbot` seats a bot, `start` lets the host start whoever is ready, and
// `sv_lobby 0` starts the match with the map.
//
// THE BENCHMARK: `benchmark <demo>` in the console (or `-benchmark <demo>` on the command line,
// which quits when it is done) plays a recorded game (`demo1` from the packs, or a file's path)
// one demo frame a drawn frame, at the window's settings (one in 33 ms shown, the rest drawn
// offscreen, so the display never sets the rate), and prints the average, max, min, 1% low and
// 0.1% low frame rates and the GPU's dearest passes (client/timedemo.hpp); the listen server
// waits. `playdemo <demo>` (`-playdemo <demo>`) plays one at its own pace instead, vsync on and
// nothing timed, `cl_demospeed <x>` (0..20, 0 pauses) its speed; `demo_track <slot>` picks the
// player a multiview recording's view rides (-1: the lowest playing). Every format plays, `.gz`
// included.
//
// ge:summary Shooter's Edge's window: the listen server, the keys and mouse, the world and the HUD
// ge:keywords app main window listen-server basedir game-folder plus-commands map console
// ge:keywords relative-mouse mouse-look hud first-person effects r_look quit shooters-edge
// ge:keywords benchmark timedemo demo vsync-off fps playdemo demo_track cl_demospeed
// ge:keywords main-menu escape vid_fullscreen fullscreen offscreen vid_display vid_displays
// ge:keywords lobby warmup ready break team addbot
// ge:skill platform input camera render ui shooter
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "console/cfg.hpp"
#include "console/view.hpp"
#include "core/output.hpp"
#include "gpu/device.hpp"
#include "gpu/surface.hpp"
#include "gpu/target.hpp"
#include "input/state.hpp"
#include "platform/devices.hpp"
#include "platform/window.hpp"
#include "shooters_edge/client/controls.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/held.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/lobby_console.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/hud/layout.hpp"
#include "shooters_edge/menu/game_menu.hpp"
#include "shooters_edge/menu/level_screens.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/view/weapons.hpp"
#include "ui/console.hpp"
#include "ui/console_keys.hpp"
#include "ui/draw.hpp"

namespace {

namespace out = ge::core::output;
namespace cl = shooters_edge::client;
namespace se = shooters_edge::hud;

// A benchmark shows one frame in this many ns (30 a second) and draws the rest offscreen.
constexpr uint64_t kBenchShowNs = 33'333'333;

void print_line(const out::LogLine& line, void*) {
  std::FILE* to = line.level == out::Level::error ? stderr : stdout;
  std::fprintf(to, "%.*s\n", static_cast<int>(line.message.size()), line.message.data());
}

struct Args {
  std::string basedir = ".";
  std::string game = "id1";
  std::string progs;      // a mod's progs file ("": the baseline mod)
  std::string models;     // the held weapons' folder ("": the tree's)
  std::string benchmark;  // a demo to benchmark, then quit ("": none)
  std::string playdemo;   // a demo to play at its own pace ("": none)
  long frames = 0;        // 0: until quit
  int width = 1280, height = 720;
  std::vector<std::string> plus;  // the command line from the first `+` word on
};

// `~` or `~/...` as the home folder (a shell leaves a quoted one as it is).
std::string expand_home(const std::string& p) {
  if (p.empty() || p[0] != '~' || (p.size() > 1 && p[1] != '/')) return p;
#ifdef _WIN32
  const char* home = std::getenv("USERPROFILE");  // NOLINT(concurrency-mt-unsafe): at start
#else
  const char* home = std::getenv("HOME");  // NOLINT(concurrency-mt-unsafe): at start
#endif
  return home == nullptr ? p : std::string(home) + p.substr(1);
}

bool parse(int argc, char** argv, Args& a) {
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    // The flags are read wherever they stand, a console line's words (`+r_look modern`) running
    // from a `+` to the next flag or `+`, so `+r_look modern -benchmark demo1` is a look and a
    // benchmark, not one console line of four words.
    if ((arg == "-basedir" || arg == "--basedir") && i + 1 < argc) {
      a.basedir = expand_home(argv[++i]);
    } else if ((arg == "-progs" || arg == "--progs") && i + 1 < argc) {
      a.progs = expand_home(argv[++i]);
    } else if ((arg == "-models" || arg == "--models") && i + 1 < argc) {
      a.models = expand_home(argv[++i]);
    } else if ((arg == "-benchmark" || arg == "--benchmark") && i + 1 < argc) {
      a.benchmark = expand_home(argv[++i]);
    } else if ((arg == "-playdemo" || arg == "--playdemo") && i + 1 < argc) {
      a.playdemo = expand_home(argv[++i]);
    } else if ((arg == "-game" || arg == "--game") && i + 1 < argc) {
      a.game = argv[++i];
    } else if (arg.rfind("--frames=", 0) == 0) {
      a.frames = std::strtol(argv[i] + 9, nullptr, 10);
    } else if (arg.rfind("--size=", 0) == 0) {
      const std::string wh(arg.substr(7));
      const size_t x = wh.find('x');
      if (x == std::string::npos) return false;
      a.width = std::atoi(wh.substr(0, x).c_str());
      a.height = std::atoi(wh.substr(x + 1).c_str());
    } else if (!a.plus.empty() || (!arg.empty() && arg[0] == '+')) {
      a.plus.emplace_back(arg);
    } else {
      return false;
    }
  }
  return true;
}

}  // namespace

static  // The campaign's screens over the game: the level's end, else the solo panel while
        // `scores`.
    void
    draw_campaign(ge::ui::DrawList& dl, const cl::Listen& l,
                  const shooters_edge::menu::ClassicArt& art, int w, int h, bool scores) {
  namespace mn = shooters_edge::menu;
  const cl::CampaignView& c = l.campaign;
  mn::LevelStats st;
  cl::completed_time(c, l.entered_at, st.minutes, st.seconds);
  if (c.end == cl::LevelEnd::none) {
    cl::CampaignView now = c;
    now.ended_at = l.server->sv.time;
    cl::completed_time(now, l.entered_at, st.minutes, st.seconds);
  }
  st.secrets = static_cast<int>(c.secrets);
  st.total_secrets = static_cast<int>(c.total_secrets);
  st.kills = static_cast<int>(c.monsters);
  st.total_kills = static_cast<int>(c.total_monsters);
  if (c.end == cl::LevelEnd::intermission)
    mn::draw_intermission(dl, art, w, h, st);
  else if (c.end != cl::LevelEnd::none)
    mn::draw_level_text(dl, art, w, h, c.text, cl::typed_chars(c, l.server->sv.time));
  else if (scores)
    mn::draw_solo_panel(dl, art, w, h, cl::map_title(l), st);
}

int run_main(int argc, char** argv) {
  out::set_output_sink(print_line);
  Args args;
  if (!parse(argc, argv, args)) {
    out::error(
        "shooters_edge: unknown flag. Usage: shooters_edge [-basedir <dir>] [-game <folder>] "
        "[-progs <file>] [-models <dir>] [-benchmark <demo>] [-playdemo <demo>] [--frames=N] "
        "[--size=WxH] "
        "[+<command> ...]; next: rerun with those flags only.");
    return 2;
  }

  // The console: its lines on the terminal and in its scrollback; the client's commands.
  ge::console::Console console;
  ge::console::ConsoleView con_view(console);
  console.set_sink([&con_view](std::string_view line) {
    con_view.add(ge::console::LineKind::output, line);
    out::status("console: " + std::string(line));
  });
  std::string next_map;
  bool quit = false;
  cl::ControlSettings controls;
  shooters_edge::look::Picture picture;
  cl::Listen listen;
  using CArgs = ge::console::Args;
  using Con = ge::console::Console;
  console.add_command("map", "map <name>: play the map (`map arena` is built in)",
                      [&](Con& c, const CArgs& a) {
                        if (a.size() < 2)
                          c.print(listen.map ? "map: " + listen.map->name : "map: none");
                        else
                          next_map = a[1];
                      });
  console.add_command("sv", "sv <line>: a line for the server's console",
                      [&](Con&, const CArgs& a) {
                        listen.opt.console.push_back(a.rest(0));
                        if (listen.server)
                          shooters_edge::server::server_command(*listen.server, a.rest(0));
                      });
  cl::add_lobby_commands(console, listen);
  cl::add_cvars(console, controls);
  shooters_edge::look::add_cvars(console, picture);
  shooters_edge::view::ViewSettings view_settings;
  shooters_edge::view::add_view_cvars(console.cvars(), view_settings);
  console.add_command("quit", "quit the game", [&](Con&, const CArgs&) { quit = true; });
  bool fullscreen = false;  // vid_fullscreen; the window follows it each frame
  console.cvars().bind({.name = "vid_fullscreen",
                        .fallback = "0",
                        .help = "1: the window over the whole screen (the desktop's own mode), "
                                "0: a window",
                        .kind = ge::console::CvarKind::boolean},
                       &fullscreen);
  int display = 0;  // vid_display; read when the window goes fullscreen
  console.cvars().bind({.name = "vid_display",
                        .fallback = "0",
                        .help = "the display vid_fullscreen covers: 0 the one the window is on, n "
                                "the n-th that vid_displays lists",
                        .kind = ge::console::CvarKind::integer},
                       &display);
  console.add_command("vid_displays", "list the displays vid_display numbers",
                      [&](Con& c, const CArgs&) {
                        for (const ge::platform::Display& d : ge::platform::displays())
                          c.print(std::to_string(d.number) + ": " + d.name + ", " +
                                  std::to_string(d.width_px) + "x" + std::to_string(d.height_px));
                      });
  std::string bench_ask;  // a demo the console asked to benchmark
  console.add_command("benchmark", "benchmark <demo>: play a demo flat out and report its fps",
                      [&](Con& c, const CArgs& a) {
                        if (a.size() < 2)
                          c.print("benchmark: name a demo (benchmark demo1)");
                        else
                          bench_ask = expand_home(a[1]);
                      });
  bool play_ask = false;  // the demo asked for is played at its own pace, not benchmarked
  console.add_command("playdemo", "playdemo <demo>: play a demo at its own pace",
                      [&](Con& c, const CArgs& a) {
                        if (a.size() < 2) {
                          c.print("playdemo: name a demo (playdemo demo1, playdemo match.mvd)");
                        } else {
                          bench_ask = expand_home(a[1]);
                          play_ask = true;
                        }
                      });
  double demo_speed = 1;
  console.add_command("cl_demospeed", "cl_demospeed <x>: a played demo's speed, 0..20 (0 pauses)",
                      [&](Con& c, const CArgs& a) {
                        if (a.size() > 1)
                          demo_speed = std::clamp(std::atof(a[1].c_str()), 0.0, 20.0);
                        c.print("cl_demospeed " + std::to_string(demo_speed));
                      });
  int demo_track = -1;
  cl::Timedemo bench;
  console.add_command("demo_track", "demo_track <slot>: the player a multiview demo's view rides",
                      [&](Con& c, const CArgs& a) {
                        if (a.size() > 1) demo_track = std::atoi(a[1].c_str());
                        bench.demo.track = demo_track;
                        c.print("demo_track " + std::to_string(demo_track) +
                                (demo_track < 0 ? " (the lowest slot playing)" : ""));
                      });
  se::Hud hud = se::make_hud();
  se::add_cvars(console, hud);
  shooters_edge::menu::GameMenu game_menu;
  game_menu.add_cvars(console);
  se::HudArt art = se::free_art();
  art.finish();
  // The HUD's feed shows the game's messages (hud.notify, below), so the console's own lines at
  // the top of the screen would show each a second time; a cfg or a + word can turn them back on.
  console.cvars().set("con_notifylines", "0", ge::console::Source::code);
  ge::console::start(console, args.plus);
  console.cvars().apply_latched();

  cl::SearchPath files;
  std::string error;
  if (!cl::open_game_dir(args.basedir, args.game, files, error)) {
    out::error("shooters_edge: the game folder did not open: " + error +
               "; see docs/shooters-edge.md § Playing it");
    if (next_map != "arena") return 2;
  } else {
    out::status("shooters_edge: " + std::to_string(files.archives.size()) + " packs in " +
                files.folder);
  }
  // The main menu's maps, its classic pictures when the packs carry them, and the packs classic
  // enhance offers.
  shooters_edge::menu::Host menu_host{.console = &console, .maps = cl::map_names(files)};
  menu_host.maps.emplace_back("arena");
  {
    shooters_edge::menu::ClassicArt menu_art;
    const auto find = [&files](std::string_view p, std::vector<uint8_t>& store) {
      return cl::find_file(files, p, store);
    };
    if (shooters_edge::menu::load_classic_art(find, &menu_art, &error))
      game_menu.set_art(std::move(menu_art));
    std::vector<std::string> packs;
    packs.reserve(files.archives.size());
    for (const cl::SearchPath::Archive& a : files.archives) packs.push_back(a.path);
    game_menu.set_packs(std::move(packs));
  }
  if (next_map.empty()) {
    std::vector<uint8_t> store;
    next_map = cl::find_file(files, "maps/start.bsp", store).empty() ? "arena" : "start";
  }
  ge::progs::Progs progs;
  std::string source;
  if (!(args.progs.empty() ? cl::baseline_progs(progs, error)
                           : shooters_edge::server::load_mod(args.progs, progs, source, error))) {
    out::error("shooters_edge: the mod did not load: " + error +
               "; see docs/shooters-edge.md § Running your own mod");
    return 2;
  }
  // The campaign: a single-player program given with -progs, else the packs' own progs.dat when
  // it is one. With it, SINGLE PLAYER ▸ NEW GAME (`newgame`) is lit.
  ge::progs::Progs pack_progs;
  const ge::progs::Progs* campaign_progs = cl::single_player(progs) ? &progs : nullptr;
  if (campaign_progs == nullptr) {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> bytes = cl::find_file(files, "progs.dat", store);
    std::string why;
    if (!bytes.empty() &&
        ge::progs::load_progs(bytes.data(), bytes.size(), pack_progs, why,
                              ge::progs::every_form()) &&
        cl::single_player(pack_progs))
      campaign_progs = &pack_progs;
  }
  cl::ListenOptions listen_opt;
  listen_opt.files = &files;
  listen_opt.lobby = listen.opt.lobby;  // the lobby's settings, as the console set them
  listen_opt.lobby_settings = listen.opt.lobby_settings;
  {
    std::vector<uint8_t> store;
    listen_opt.registered = !cl::find_file(files, "gfx/pop.lmp", store).empty();
  }
  if (campaign_progs != nullptr)
    console.add_command("newgame", "newgame: a new single-player game from the map start",
                        [&](Con&, const CArgs&) {
                          listen.opt.console.emplace_back("deathmatch 0");
                          listen.opt.console.emplace_back("coop 0");
                          next_map = "start";
                        });
  // The saves (the campaign's whole game: listen.hpp's `listen_save`), `<game>/saves/<name>.save`.
  std::string load_ask;  // a save the console asked to load (loaded where a map change is made)
  if (campaign_progs != nullptr) {
    console.add_command(
        "save",
        "save <name>: the single-player game saved as saves/<name>.save (<name>.sav: the "
        "classic text save in the game folder)",
        [&](Con& c, const CArgs& a) {
          if (a.size() < 2) {
            c.print("save: save <name>");
            return;
          }
          std::string why, name = a[1];
          const bool classic = cl::classic_save_name(name);
          std::vector<uint8_t> bytes;
          std::string text;
          const bool made = classic ? cl::listen_save_text(listen, text, why)
                                    : cl::listen_save(listen, bytes, why,
                                                      static_cast<int64_t>(std::time(nullptr)));
          if (classic) bytes.assign(text.begin(), text.end());
          if (!made || !cl::write_save(files, name, bytes, why, classic))
            c.print(why);
          else
            c.print("saved " + a[1]);
        });
    console.add_command("load", "load <name>: the game saved as saves/<name>.save, else <name>.sav",
                        [&](Con& c, const CArgs& a) {
                          if (a.size() < 2)
                            c.print("load: load <name>");
                          else
                            load_ask = a[1];
                        });
  }
  std::unique_ptr<cl::LoadedMap> first = cl::load_named_map(files, next_map, error);
  if (!first || !cl::listen_start(listen, progs, std::move(first), listen_opt, error)) {
    out::error("shooters_edge: the first map did not start: " + error +
               "; see docs/shooters-edge.md § Playing it");
    return 2;
  }
  next_map.clear();

  // The held weapons: the folder given, else the tree's from here, else from where it was built.
  std::vector<ge::viewmodel::HeldModel> weapons;
  {
    std::vector<std::string> dirs;
    if (!args.models.empty()) dirs.push_back(args.models);
    dirs.emplace_back("games/shooters_edge/content/models/view");
#ifdef SE_MODELS_DIR
    dirs.emplace_back(SE_MODELS_DIR);
#endif
    std::string why;
    for (const std::string& dir : dirs)
      if (shooters_edge::view::load_weapon_models(dir, weapons, &why)) break;
    if (weapons.empty())
      out::status("shooters_edge: no weapon is drawn: " + why +
                  "; next: pass -models <the tree's games/shooters_edge/content/models/view>");
  }

  auto window = ge::platform::open_window(
      {.title = "Shooter's Edge", .width = args.width, .height = args.height});
  if (!window) return 1;
  auto device = ge::gpu::create_device();
  if (!device) return 1;
  ge::gpu::Surface surface;
  if (!surface.create(*device, window->native())) return 1;
  int w = 0, h = 0;
  window->pixel_size(w, h);
  // The pointer comes in the window's logical units; the menu is laid out in pixels.
  const float px_per_unit =
      args.width > 0 ? static_cast<float>(w) / static_cast<float>(args.width) : 1;
  surface.configure(*device, static_cast<uint32_t>(w), static_cast<uint32_t>(h), true);
  cl::WorldDrawPtr world = cl::create_world_draw(
      *device, files, *listen.map, static_cast<uint32_t>(w), static_cast<uint32_t>(h),
      surface.format(), picture, weapons, error, {listen.server->models, {}});
  if (!world) {
    out::error("shooters_edge: the world draw did not load: " + error + "; see docs/rendering.md");
    return 1;
  }
  ge::platform::DevicesPtr devices = ge::platform::open_devices();
  if (!devices) return 1;
  ge::platform::set_relative_mouse(*window, true);
  bool captured = true;
  bool vsync = true;
  bool full_applied = false;  // what the window was last set to; vid_fullscreen is the ask
  std::unique_ptr<cl::LoadedMap> bench_map;  // the benchmark's level (the world draws it)
  ge::gpu::Target bench_off;                 // the benchmark's frames that are not shown
  uint64_t bench_shown_ns = 0;               // when the benchmark last showed a frame
  wgpu::Future bench_in_flight{};            // the frame before this one, waited before the next
  if (!args.benchmark.empty()) bench_ask = args.benchmark;
  if (!args.playdemo.empty()) {
    bench_ask = args.playdemo;
    play_ask = true;
  }
  // The world's frame for `map`, or false with the error said.
  const auto rebuild_world = [&](const cl::LoadedMap& map) {
    world.reset();
    world =
        cl::create_world_draw(*device, files, map, static_cast<uint32_t>(w),
                              static_cast<uint32_t>(h), surface.format(), picture, weapons, error);
    if (!world)
      out::error("shooters_edge: the world draw did not load: " + error +
                 "; see docs/rendering.md");
    return world != nullptr;
  };
  // The benchmark over: its report, vsync back, the listen server's world again.
  int bench_failed = 0;
  const auto end_bench = [&](const std::string& why, double now_s) {
    const std::string setup = std::to_string(w) + "x" + std::to_string(h) +
                              ", drawn offscreen, shown at 30 Hz, r_look " + picture.look + ", " +
                              device->report.name + " on " + device->report.backend;
    // A run stopped before a frame was timed has no report, only its reason; playback times none.
    if (bench.paced) {
      console.print("playdemo: " + bench.name + (why.empty() ? " ended" : " stopped: " + why));
      if (why.find("Escape") == std::string::npos && !why.empty()) bench_failed = 1;
    } else if (why.empty() || !bench.frame_s.empty())
      for (const std::string& line :
           cl::bench_report(bench.name, setup, cl::bench_stats(bench.frame_s))) {
        console.print(line);
        hud.notify.push(line, se::hud_style().ink, now_s);
      }
    if (!bench.paced && (why.empty() || !bench.frame_s.empty()))
      console.print(cl::span_report(bench.passes));
    if (!why.empty() && !bench.paced) {
      console.print("benchmark: stopped early: " + why);
      bench_failed = 1;
    }
    if (bench_in_flight.id != 0) ge::gpu::wait(*device, bench_in_flight);
    bench_in_flight = {};
    bench_off = {};
    bench.active = false;
    bench_map.reset();
    vsync = true;
    surface.configure(*device, static_cast<uint32_t>(w), static_cast<uint32_t>(h), vsync);
    if (bench.quit_after) quit = true;
    return rebuild_world(*listen.map);
  };

  ge::input::Queue queue;
  ge::input::InputState held;
  const ge::input::InputState idle;
  ge::ui::ConsoleKeys con_keys;
  bool lobby_roster = true;  // the lobby panel's roster (F3)
  bool text_on = false;
  ge::ui::DrawList overlay;
  ge::platform::InputSnapshot input;
  cl::ViewState shown;
  ge::ui::Vec2 last_pointer;
  cl::FxState fx_state;
  cl::FxFrame fx;
  cl::HeldState in_hand;
  size_t printed = 0;
  const uint64_t first_ns = ge::platform::now_ns();
  uint64_t last = first_ns;
  long presented = 0;
  while ((args.frames == 0 || presented < args.frames) && !quit) {
    queue.clear();
    ge::platform::poll_input(*window, input, *devices, queue);
    const uint64_t now = ge::platform::now_ns();
    const double wall = static_cast<double>(now - last) * 1e-9;
    const double t = static_cast<double>(now - first_ns) * 1e-9;
    last = now;
    if (input.quit) break;
    if (fullscreen != full_applied) {  // the size follows through `input.resized`
      if (!ge::platform::set_fullscreen(*window, fullscreen, display)) fullscreen = full_applied;
      full_applied = fullscreen;
    }
    ge::input::update(held, queue);
    ge::ui::feed_console(console, con_view, queue, con_keys);
    if (con_view.open() != text_on) {
      text_on = con_view.open();
      ge::platform::set_text_input(*window, text_on);
    }
    // The main menu: Escape opens it and goes back out of it; while it is up it has the keys and
    // the pointer.
    const bool was_open = game_menu.open();
    {
      using ge::platform::Key;
      ge::ui::MenuInput mi;
      if (!con_keys.held && was_open) {
        mi.up = input.tapped(Key::up) || input.tapped(Key::w);
        mi.down = input.tapped(Key::down) || input.tapped(Key::s);
        mi.left = input.tapped(Key::left) || input.tapped(Key::a);
        mi.right = input.tapped(Key::right) || input.tapped(Key::d);
        mi.accept = input.tapped(Key::enter) || input.tapped(Key::space);
        mi.back = input.tapped(Key::escape);
        mi.pointer_x = input.pointer_x * px_per_unit;
        mi.pointer_y = input.pointer_y * px_per_unit;
        mi.pointer_moved = mi.pointer_x != last_pointer.x || mi.pointer_y != last_pointer.y;
        last_pointer = {mi.pointer_x, mi.pointer_y};
        mi.click = held.mouse.buttons.went_up(0);
      }
      menu_host.server = listen.server ? &listen.server->cvars : nullptr;
      // A benchmark's Escape stops the run (below) and opens nothing.
      const bool opening =
          !con_keys.held && !was_open && !bench.active && input.tapped(Key::escape);
      if (opening) {  // the slots' lines read again: a save may have been made since
        menu_host.saves.assign(shooters_edge::menu::kSaveSlots, {});
        for (int i = 0; i < shooters_edge::menu::kSaveSlots; i++)
          menu_host.saves[static_cast<size_t>(i)] = cl::save_label(files, "s" + std::to_string(i));
      }
      game_menu.update(mi, opening, menu_host, wall);
    }
    // The mouse is the look's while captured: the console, the menu or the focus lost lets it go;
    // the menu closing or a click in the window takes it again.
    const bool release = con_view.open() || input.focus_lost || game_menu.open();
    if (captured && release) {
      captured = false;
      ge::platform::set_relative_mouse(*window, false);
    } else if (!captured && !con_view.open() && !game_menu.open() &&
               (was_open || held.mouse.buttons.went_down(0))) {
      captured = true;
      ge::platform::set_relative_mouse(*window, true);
    }
    const ge::input::InputState& played = con_keys.held || !captured || was_open ? idle : held;
    if (input.resized) {
      w = input.width_px;
      h = input.height_px;
      surface.configure(*device, static_cast<uint32_t>(w), static_cast<uint32_t>(h), vsync);
      cl::resize_world(*device, *world, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    }

    // A map asked for (the console's `map`, or the mod's level change at its limits).
    if (next_map.empty() && !listen.pending_map.empty()) next_map = listen.pending_map;
    if (!next_map.empty()) {
      std::unique_ptr<cl::LoadedMap> m = cl::load_named_map(files, next_map, error);
      next_map.clear();
      if (!m) {
        console.print("map: " + error);
        listen.pending_map.clear();
      } else if (!(campaign_progs != nullptr && cl::wants_campaign(listen.opt)
                       ? (listen.campaign_host
                              ? cl::listen_change(listen, std::move(m), error)
                              : cl::listen_campaign(listen, *campaign_progs, std::move(m),
                                                    listen.opt, error))
                   : listen.campaign_host || listen.progs != &progs
                       ? cl::listen_start(listen, progs, std::move(m), listen.opt, error)
                       : cl::listen_change(listen, std::move(m), error))) {
        out::error("shooters_edge: the map change failed: " + error +
                   "; see docs/shooters-edge.md § Playing it");
        return 2;
      } else {
        world.reset();
        world = cl::create_world_draw(*device, files, *listen.map, static_cast<uint32_t>(w),
                                      static_cast<uint32_t>(h), surface.format(), picture, weapons,
                                      error, {listen.server->models, {}});
        if (!world) {
          out::error("shooters_edge: the new world draw did not load: " + error +
                     "; see docs/rendering.md");
          return 1;
        }
        shown = {};
        fx_state = {};
        in_hand = {};
      }
    }

    // A save asked for: the game it holds now playing (the world drawn anew below).
    if (!load_ask.empty()) {
      const std::string name = load_ask;
      load_ask.clear();
      std::vector<uint8_t> bytes;
      cl::ListenOptions lo = listen.opt;
      lo.console.emplace_back("deathmatch 0");
      // Ours by that name, else the classic text save (`<name>.sav` names it alone).
      std::string base = name;
      bool classic = cl::classic_save_name(base);
      if (!classic && !cl::read_save(files, base, bytes, error))
        classic = cl::read_save(files, base, bytes, error, true);
      else if (classic && !cl::read_save(files, base, bytes, error, true))
        bytes.clear();
      const bool loaded =
          !bytes.empty() &&
          (classic
               ? cl::listen_load_text(
                     listen, *campaign_progs, lo,
                     std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()),
                     name, error)
               : cl::listen_load(listen, *campaign_progs, lo, bytes, name, error));
      if (!loaded)
        console.print("load: " + error);
      else
        console.print("loaded " + name);
    }

    // A benchmark asked for: vsync off, the listen server waits until it is done. A demo played
    // at its pace keeps vsync.
    if (!bench_ask.empty()) {
      const bool quit_after = bench_ask == args.benchmark || bench_ask == args.playdemo;
      if (!cl::timedemo_start(files, bench_ask, bench, error)) {
        console.print("benchmark: " + error);
        if (quit_after) {
          out::error("shooters_edge: the benchmark did not start: " + error +
                     "; see docs/shooters-edge.md § Benchmarking a demo");
          return 2;
        }
      } else {
        bench.quit_after = quit_after;
        bench.paced = play_ask;
        bench.demo.track = demo_track;
        console.print((play_ask ? "playdemo: " : "benchmark: ") + bench.found);
        con_view.set_open(false);
        if (!play_ask) {
          vsync = false;  // and unlocked: immediate present, where the platform has it
          surface.configure_unlocked(*device, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
        }
      }
      args.benchmark.clear();
      args.playdemo.clear();
      bench_ask.clear();
      play_ask = false;
    }
    if (bench.active && input.tapped(ge::platform::Key::escape)) {
      if (!end_bench("Escape pressed", t)) return 1;
      continue;
    }
    if (bench.active) {
      const cl::TimedemoStep step = bench.paced ? cl::playdemo_step(bench, wall, demo_speed, error)
                                                : cl::timedemo_step(bench, error);
      if (step == cl::TimedemoStep::new_level) {
        bench_map = cl::load_named_map(files, bench.demo.state.map, error);
        if (!bench_map) {
          if (!end_bench("the demo's map " + bench.demo.state.map + ": " + error, t)) return 1;
          continue;
        }
        if (!rebuild_world(*bench_map)) return 1;
      } else if (step != cl::TimedemoStep::frame) {
        if (!end_bench(step == cl::TimedemoStep::error ? error : "", t)) return 1;
        continue;
      }
      ge::render::FrameView view =
          cl::demo_view(bench.demo.state, ge::camera::FirstPersonRow{}.fov_y, t);
      view.frame = static_cast<uint64_t>(presented);
      overlay.begin(w, h);
      se::fill_hud(hud, cl::demo_status(bench.demo.state, t), art);
      hud.layout.draw(overlay, hud.layout.frame(w, h, se::hud_style()), se::hud_style());
      // Every frame is drawn, but only one in kBenchShowNs is shown: the rest go to an offscreen
      // target of the surface's format. A window's present waits for the display however its
      // mode is set (macOS's compositor hands an image back once a refresh), so a frame rate
      // timed through it is the display's; drawn offscreen it is the machine's.
      const uint64_t frame_ns = ge::platform::now_ns();
      wgpu::TextureView target;
      const bool show = bench.paced || frame_ns - bench_shown_ns >= kBenchShowNs;
      if (show) target = surface.acquire(*device);
      if (target) {
        bench_shown_ns = frame_ns;
      } else {
        if (bench_off.width != static_cast<uint32_t>(w) ||
            bench_off.height != static_cast<uint32_t>(h))
          bench_off = ge::gpu::create_target(*device, "benchmark frame", static_cast<uint32_t>(w),
                                             static_cast<uint32_t>(h), surface.format());
        target = bench_off.view;
      }
      // The demo's effects are not read yet: the world's frame with none of its own.
      cl::draw_world(*device, *world, view, cl::FxFrame{}, nullptr, wall, overlay, target);
      if (!bench.paced && bench.skip == 0) cl::record_span(*world, bench.passes);
      if (bench_shown_ns == frame_ns) surface.present();
      if (bench.paced) {  // playback: every frame shown, nothing timed
        ++presented;
        continue;
      }
      // At most one frame in flight, as a swap chain of two images would allow: the GPU's time
      // is in the frame rate, not queued behind it.
      const wgpu::Future done = device->queue.OnSubmittedWorkDone(
          wgpu::CallbackMode::WaitAnyOnly, [](wgpu::QueueWorkDoneStatus, wgpu::StringView) {});
      if (bench_in_flight.id != 0) ge::gpu::wait(*device, bench_in_flight);
      bench_in_flight = done;
      cl::timedemo_mark(bench, ge::platform::now_ns());
      ++presented;
      continue;
    }

    // The lobby's keys (F1 ready or break, F2 the other team, F3 the roster), then the player's
    // hands, then the server's frames.
    {
      using ge::platform::Key;
      const bool keys = !con_keys.held && !con_view.open() && !game_menu.open();
      cl::lobby_keys(listen, keys && input.tapped(Key::f1), keys && input.tapped(Key::f2),
                     keys && input.tapped(Key::f3), lobby_roster);
    }
    cl::turn_view(played, controls, listen.view);
    const ge::shooter::MoveCmd cmd = cl::read_controls(played, controls);
    if (listen.server != nullptr) {
      const cl::PlayerState me = cl::player_state(listen);
      cl::spring_view(played, controls, me.on_ground, cmd, me.ideal_pitch, wall, listen.view);
    }
    if (cmd.impulse != 0) listen.impulse = cmd.impulse;
    // A single-player game waits while the console or the menu is up.
    const bool paused = listen.campaign_host && (con_view.open() || game_menu.open());
    const int frames = paused ? 0 : cl::listen_advance(listen, wall, cmd);
    if (!listen.failed.empty()) {
      out::error("shooters_edge: the campaign's next map did not start: " + listen.failed +
                 "; see docs/shooters-edge.md § Playing it");
      return 2;
    }
    if (listen.map_changed) {  // the campaign went on to its next map
      listen.map_changed = false;
      if (!rebuild_world(*listen.map)) return 1;
      shown = {};
      fx_state = {};
      in_hand = {};
    }
    if (listen.restarted) {  // the lobby's countdown ran out: the match on a new server
      listen.restarted = false;
      shown = {};
      fx_state = {};
      in_hand = {};
    }
    if (listen.campaign.sellscreen) {  // the shareware episode's end: the help pages
      listen.campaign.sellscreen = false;
      game_menu.show(menu_host, "help");
    }
    for (; printed < listen.prints.size(); printed++) {
      const std::string& line = listen.prints[printed];
      console.print(line);
      hud.notify.push(line, se::hud_style().ink, t);
    }
    // The eyes on the player and what the world throws, then the overlay: the HUD, the console.
    ge::render::FrameView view = cl::frame_view(listen, shown, frames, t, wall);
    view.frame = static_cast<uint64_t>(presented);
    cl::step_fx(listen, shown, fx_state, t, wall, fx);
    const shooters_edge::view::HandFrame hand =
        cl::step_held(listen, shown, in_hand, view_settings, weapons, t, wall);
    if (hand.flashing)
      fx.lights.push_back(shooters_edge::effects::muzzle_light(hand.flash.position));
    overlay.begin(w, h);
    if (listen.campaign.end == cl::LevelEnd::none) {
      se::fill_hud(hud, cl::status_of(listen, t), art);
      const ge::hud::Frame hf = hud.layout.frame(w, h, se::hud_style());
      hud.layout.draw(overlay, hf, se::hud_style());
    }
    if (listen.room && listen.server != nullptr)
      se::draw_lobby(overlay, cl::lobby_view(*listen.room, listen.server->sv.time, 0, lobby_roster),
                     w, h);
    draw_campaign(
        overlay, listen, game_menu.art(), w, h,
        listen.campaign_host && !con_keys.held && !was_open && input.down(ge::platform::Key::tab));
    game_menu.draw(overlay, w, h, t);
    con_view.frame(wall);
    ge::ui::draw_console(overlay, con_view, {}, nullptr, t);

    wgpu::TextureView target = surface.acquire(*device);
    if (!target) continue;
    cl::draw_world(*device, *world, view, fx, &hand, wall, overlay, target);
    surface.present();
    ++presented;
  }
  const double seconds = static_cast<double>(ge::platform::now_ns() - first_ns) * 1e-9;
  char line[160];
  std::snprintf(line, sizeof line,
                "shooters_edge: %ld frames in %.2f s (%.1f ms a frame) at %dx%d, %u GPU errors",
                presented, seconds,
                presented ? seconds * 1e3 / static_cast<double>(presented) : 0.0, w, h,
                device->errors.load());
  out::status(line);
  return device->errors.load() == 0 && !device->lost.load() && bench_failed == 0 ? 0 : 1;
}

// What nothing else caught ends the program here, with a line that says so, rather than in
// std::terminate's bare abort.
int main(int argc, char** argv) {
  try {
    return run_main(argc, argv);
  } catch (const std::exception& e) {
    std::fprintf(stderr,
                 "shooters_edge: stopped by an uncaught exception: %s. This is a bug: run the same "
                 "command under lldb or gdb with a breakpoint on __cxa_throw to see where it "
                 "was thrown.\n",
                 e.what());
  } catch (...) {
    std::fprintf(
        stderr,
        "shooters_edge: stopped by an uncaught exception that is not a std::exception. This "
        "is a bug: run the same command under lldb or gdb with a breakpoint on "
        "__cxa_throw to see where it was thrown.\n");
  }
  return 1;
}
