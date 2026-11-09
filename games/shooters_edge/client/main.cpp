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
//                 [-timedemo <demo>] [-playdemo <demo>] [--frames=N] [--size=WxH]
//                 [-demovideo <demo> <file> [--from=m:ss --to=m:ss | --highlight=N] ...]
//                 [+<command> ...]
//
// `-basedir` holds the game folder (`id1`, the multiplayer client's folder over it when present;
// `-game` names another); `~` is home. Without it, the game loaded last, however it was picked;
// LOAD GAME or `basedir <folder>` loads another in place, enhanced in the background
// (client/game_folder.hpp). `-progs` runs a mod's progs in place of the baseline mod, `-models`
// names the held weapons' folder (by default the game's `content/models/view`). The other words are
// console lines run at start (a `+` before each after the first): `map dm6` loads that map;
// with none, the title (client/title.hpp) with the menu up, `start` (else the arena) loading behind
// it. Every map loads behind the window, a spinner up (client/map_load.hpp).
//
// THE KEYS are binds (controls.hpp: W A S D, SPACE jumps, the left button fires, SHIFT walks, 1..8
// and the wheel the weapons, F12 a screenshot; OPTIONS ▸ CUSTOMIZE CONTROLS); TAB held shows the
// scores (client/scores.hpp; `scr_levelstats 0|1`); the mouse looks (taken on focus;
// `in_mouse_release` frees it); ESCAPE opens the main menu (menu/game_menu.hpp: drawn in the pack's
// own pictures when the packs carry them, the rows the game cannot do yet dimmed), and ESCAPE again
// goes back out of it; the key under ESCAPE drops the console, where `map <name>` changes the map,
// `sv <line>` is a line for the server's console (`sv timelimit 10`), `save <name>` and `load
// <name>` keep and bring back a single-player game (`<game folder>/saves/<name>.save`; `<name>.sav`
// the classic text save in the game folder, which `load` also finds by the bare name), `sensitivity
// <deg a count>` and `m_invert 0|1` set the look, `r_look classic|modern` and the other picture
// cvars (app/look.hpp: `r_fx_lights`, `r_fx_glow`, `r_bloom`, `r_lava_glow`, `r_slime_glow`) the
// world's look, the view-model cvars (view/hand.hpp: `r_drawviewmodel`, `r_viewmodel_fov`,
// `cl_gunx`, `cl_guny`, `cl_gunz`, `cl_gunsway`) the weapon in hand, `vid_fullscreen 0|1` the
// window over the whole screen (`vid_display n` which one, of those `vid_displays` lists), `gamma`
// the brightness, `topcolor` and `bottomcolor` the player's colours, and `quit` quits; the settings
// are kept in `<game folder>/settings/config.cfg` (client/settings.hpp).
//
// THE LOBBY: its own screen over the title's night until F4 joins the warmup (lobby_console).
//
// THE BENCHMARK: `timedemo <demo>` in the console (or `-timedemo <demo>` on the command line,
// which quits when it is done) plays a recorded game (`demo1` from the packs, or a file's path)
// one demo frame a drawn frame, at the window's settings (one in 33 ms shown, the rest drawn
// offscreen, so the display never sets the rate), and prints the average, max, min, 1% low and
// 0.1% low frame rates and the GPU's dearest passes (client/timedemo.hpp); the listen server
// waits. `playdemo <demo>` (`-playdemo <demo>`) plays one at its own pace instead, vsync on and
// nothing timed, `cl_demospeed <x>` (0..20, 0 pauses) its speed; `demo_track <slot>` picks the
// player a multiview recording's view rides (-1: the lowest playing). Every format plays, `.gz`
// included, and the voice formats (`.qwdv`, `.mvdv`) with their voices. A played demo is watched
// like a video (client/demo_playback.hpp); `-demovideo <demo> <file>` turns one, or a clip of it,
// into a video or a GIF with no window and quits (client/demo_video.hpp, client/video_cfg.hpp).
// ge:summary Shooter's Edge's window: the listen server, the keys and mouse, the world and the HUD
// ge:keywords app main window listen-server basedir game-folder plus-commands map console relative-
// ge:keywords mouse mouse-look hud first-person effects r_look quit shooters-edge benchmark title
// ge:keywords timedemo demo vsync-off fps playdemo demo_track cl_demospeed demovideo main-menu
// ge:keywords escape vid_fullscreen fullscreen offscreen vid_display vid_displays lobby scores-key
// ge:keywords scoreboard scr_levelstats settings config-cfg topcolor bottomcolor gamma binds f12
// ge:skill platform input camera render ui shooter
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "capture/screenshot.hpp"
#include "console/cfg.hpp"
#include "console/view.hpp"
#include "core/output.hpp"
#include "gpu/device.hpp"
#include "gpu/surface.hpp"
#include "gpu/target.hpp"
#include "hud/spinner.hpp"
#include "input/state.hpp"
#include "net/voice_codec.hpp"
#include "platform/audio.hpp"
#include "platform/devices.hpp"
#include "platform/window.hpp"
#include "render/screenshot_copy.hpp"
#include "shooters_edge/client/args.hpp"
#include "shooters_edge/client/classic_pictures.hpp"
#include "shooters_edge/client/controls.hpp"
#include "shooters_edge/client/coop_mode.hpp"
#include "shooters_edge/client/demo_check.hpp"
#include "shooters_edge/client/demo_heard.hpp"
#include "shooters_edge/client/demo_pack.hpp"
#include "shooters_edge/client/demo_playback.hpp"
#include "shooters_edge/client/demo_video.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/game_folder.hpp"
#include "shooters_edge/client/held.hpp"
#include "shooters_edge/client/listen_record.hpp"
#include "shooters_edge/client/listen_sound.hpp"
#include "shooters_edge/client/lobby_console.hpp"
#include "shooters_edge/client/map_load.hpp"
#include "shooters_edge/client/own_sounds.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/scores.hpp"
#include "shooters_edge/client/settings.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/client/title.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/window_cvars.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/hud/classic_art.hpp"
#include "shooters_edge/hud/layout.hpp"
#include "shooters_edge/hud/lobby_page.hpp"
#include "shooters_edge/menu/game_menu.hpp"
#include "shooters_edge/menu/level_screens.hpp"
#include "shooters_edge/menu/lobby_screen.hpp"
#include "shooters_edge/net/classic_browse.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/net/classic_remote.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/words.hpp"
#include "shooters_edge/view/weapons.hpp"
#include "ui/console.hpp"
#include "ui/console_keys.hpp"
#include "ui/draw.hpp"

namespace {

namespace out = ge::core::output;
namespace cl = shooters_edge::client;
namespace se = shooters_edge::hud;
namespace sn = shooters_edge::net;

// A timedemo shows one frame in this many ns (30 a second) and draws the rest offscreen.
constexpr uint64_t kBenchShowNs = 33'333'333;

void print_line(const out::LogLine& line, void*) {
  std::FILE* to = line.level == out::Level::error ? stderr : stdout;
  std::fprintf(to, "%.*s\n", static_cast<int>(line.message.size()), line.message.data());
}

}  // namespace

int run_main(int argc, char** argv) {
  out::set_output_sink(print_line);
  cl::Args args;
  if (std::string why; !cl::parse(argc, argv, args, why)) {
    out::error("shooters_edge: the command line was refused: " + why +
               " Usage: shooters_edge [-basedir <dir>] [-game <folder>] "
               "[-progs <file>] [-models <dir>] [-timedemo <demo>] [-playdemo <demo>] "
               "[--frames=N] [--size=WxH] [-demovideo <demo> <file> [--fps=N] "
               "[--resolution=<WxH|720p|1080p|1440p|4k>] [--from=m:ss] [--to=m:ss] [--highlight=N] "
               "[--gif-width=N] [--codec=<c>] [--seconds=S]] "
               "[+<command> ...]; next: rerun with only those flags.");
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
                        if (listen.server) listen.rules->console(a.rest(0));
                      });
  cl::add_lobby_commands(console, listen);
  cl::CoopMode coop_mode;  // sv_coop and its settings (client/coop_mode.hpp)
  cl::add_coop_cvars(console, coop_mode);
  cl::add_cvars(console, controls);
  cl::default_binds(console);  // before the configs, which rebind what the player changed
  shooters_edge::look::add_cvars(console, picture);
  shooters_edge::view::ViewSettings view_settings;
  shooters_edge::view::add_view_cvars(console.cvars(), view_settings);
  console.add_command("quit", "quit the game", [&](Con&, const CArgs&) { quit = true; });
  cl::UserInfo user_info;  // SETUP's colours (and the console's `name`), sent as they change
  cl::add_user_info_cvars(console, user_info);
  cl::WindowCvars win;  // vid_fullscreen (followed each frame), vid_display, volume, ...
  cl::add_window_cvars(console, win);
  std::string bench_ask;  // a demo the console asked to benchmark (`timedemo`) or play
  bool play_ask = false;  // the demo asked for is played at its own pace (`playdemo`)
  cl::add_demo_commands(console, bench_ask, play_ask);
  cl::Timedemo bench;
  sn::Remote remote;  // `connect`: the window on a server elsewhere
  sn::add_remote_commands(console, remote);
  cl::DemoPlayback playback;  // a played demo's bar, controls and highlights (demo_playback.hpp)
  cl::add_playback_commands(console, playback, bench);
  // The window's sound: its own game's (client/listen_sound.hpp) or a played demo's or a joined
  // server's (client/demo_heard.hpp), at `volume`.
  ge::audio::Synth synth;  // outlives the device that pulls it (opened with the window)
  se::Hud hud = se::make_hud();
  const auto tell = [&](const std::string& line, double at) {  // the console and the HUD's feed
    console.print(line);
    hud.notify.push(line, se::hud_style().ink, at);
  };
  se::add_cvars(console, hud);
  shooters_edge::menu::GameMenu game_menu;
  game_menu.add_cvars(console);
  shooters_edge::menu::BoardView board_view;
  // The HUD's pictures: a classic game's own once its packs are open (below), Shooter's Edge's
  // free set only for its own content (client/classic_pictures.hpp's hud_art_of).
  se::HudArt free_art = se::free_art();
  free_art.finish();
  se::HudArt classic_art;
  // The HUD's feed shows the game's messages (hud.notify, below), so the console's own lines at
  // the top of the screen would show each a second time; a cfg or a + word can turn them back on.
  console.cvars().set("con_notifylines", "0", ge::console::Source::code);
  // The install (client/game_folder.hpp): -basedir, else the one loaded last; multiplayer over
  // `id1`.
  const std::string prefs = ge::platform::prefs_dir("gameengine2", "shooters_edge");
  cl::choose_folder(args, prefs);
  // The player's settings (client/settings.hpp); the folder the game runs in is read after them.
  console.set_search_path({cl::settings_dir(args.basedir, args.game), "."});
  // F12's screenshots (engine/capture/screenshot.hpp), in <game folder>/screenshots/ beside its
  // recordings, saves/ and settings/; made before the + lines so `+screenshot` takes the first.
  ge::capture::ScreenshotSettings shot_settings;  // scr_format, scr_quality, scr_chroma
  ge::capture::Screenshots shots(
      (cl::settings_dir(args.basedir, args.game).parent_path() / "screenshots").string(),
      "shooters_edge", &shot_settings);
  ge::capture::add_screenshot_cvars(console.cvars(), shot_settings);
  ge::capture::add_screenshot_command(console, shots, "<game folder>/screenshots/");
  game_menu.set_screenshots(shots.dir());  // OPTIONS > SCREENSHOTS looks through them
  game_menu.servers().set_query(shooters_edge::net::classic_query());  // JOIN A GAME
  const auto save_settings = [&console] { cl::save_settings(console); };
  ge::console::start(console, args.plus);
  console.cvars().apply_latched();

  cl::SearchPath files;
  cl::ListenRecorder recorder;  // `record` and `stop` (listen_record.hpp)
  cl::add_record_commands(console, recorder, listen, files, playback.record);
  std::string error;
  // No packs and no `+map`: the built-in arena (the start map below falls back to it).
  const bool opened = cl::open_game_dir(args.basedir, args.game, files, error);
  if (!opened) {
    if (!next_map.empty() && next_map != "arena") {
      out::error("shooters_edge: the game folder did not open: " + error +
                 "; see docs/shooters-edge.md § Playing it");
      return 2;
    }
    out::warn("shooters_edge: no game folder, so the built-in arena: " + error +
              "; see docs/shooters-edge.md § Playing it");
    // Its sounds: the game's own recordings under the names the mod plays (own_sounds.hpp).
#ifdef SE_CONTENT_DIR
    const std::string content = cl::find_content_dir(SE_CONTENT_DIR);
#else
    const std::string content = cl::find_content_dir("");
#endif
    cl::OwnSounds own;
    std::string why;
    if (content.empty() || !cl::mount_own_sounds(files, content, own, why)) {
      out::warn("shooters_edge: the arena is silent: " +
                (content.empty() ? std::string("no games/shooters_edge/content here") : why) +
                "; run from the tree's root");
    } else {
      out::status("shooters_edge: " + std::to_string(own.mounted) + " of " +
                  std::to_string(own.mapped) + " sounds from " + content);
    }
  } else {
    out::status("shooters_edge: " + std::to_string(cl::archive_paths(files).size()) +
                " archives on the search path of " + files.folder);
  }
  // A server's game folder for `connect`: the window's own search path when it is the same game
  // (or the base), else that folder under the base folder, mounted once.
  remote.read_file = [&files, &args,
                      other = std::make_shared<std::pair<std::string, cl::SearchPath>>()](
                         const std::string& gamedir, const std::string& path) {
    std::vector<uint8_t> store;
    const cl::SearchPath* from = &files;
    if (!gamedir.empty() && gamedir != args.game &&
        gamedir != ge::classicenhance::classic_profile().base_dir) {
      if (other->first != gamedir) {
        std::string why;
        other->second = {};
        if (!cl::open_game_dir(args.basedir, gamedir, other->second, why)) return store;
        other->first = gamedir;
      }
      from = &other->second;
    }
    const std::span<const uint8_t> got = cl::find_file(*from, path, store);
    return std::vector<uint8_t>(got.begin(), got.end());
  };
  if (!args.demovideo.empty()) return cl::demo_video_main(console, args, files, picture, playback);
  // The window first, the title's night in it at once (client/title.hpp); the rest of the start
  // (the pictures, the programs) and the first map (client/map_load.hpp) come in behind it.
  auto window = ge::platform::open_window(
      {.title = "Shooter's Edge", .width = args.width, .height = args.height});
  if (!window) return 1;
  auto device = ge::gpu::create_device();
  if (!device) return 1;
  ge::gpu::Surface surface;
  if (!surface.create(*device, window->native())) return 1;
  ge::render::ScreenshotCopy shot_copy;  // the frames `screenshot` takes, copied back
  int w = 0, h = 0;
  window->pixel_size(w, h);
  // The pointer comes in the window's logical units; the menu is laid out in pixels.
  const float px_per_unit =
      args.width > 0 ? static_cast<float>(w) / static_cast<float>(args.width) : 1;
  surface.configure(*device, static_cast<uint32_t>(w), static_cast<uint32_t>(h), true);
  cl::WorldDrawPtr title =
      cl::make_title_world(*device, static_cast<uint32_t>(w), static_cast<uint32_t>(h),
                           surface.format(), picture, error);
  if (!title) {
    out::error("shooters_edge: the title did not load: " + error + "; see docs/rendering.md");
    return 1;
  }
  ge::ui::DrawList overlay;
  const uint64_t first_ns = ge::platform::now_ns();
  // The title with the spinner in its corner: the start's first frame, and every frame a map
  // loads with no world of it yet.
  // The title's night with the menu over it; `lobby` puts the lobby's screen between them.
  const auto title_frame = [&](double t, double dt, std::string_view caption, double since,
                               bool lobby = false) {
    overlay.begin(w, h);
    if (lobby) {
      const se::LobbyView lv = cl::listen_lobby_view(listen);
      if (game_menu.art().loaded())
        shooters_edge::menu::draw_lobby_page_classic(overlay, game_menu.art(), lv, w, h);
      else
        se::draw_lobby_page(overlay, lv, w, h);
    }
    game_menu.draw(overlay, w, h, t);
    con_view.frame(dt);
    ge::ui::draw_console(overlay, con_view, {}, nullptr, t);
    if (!caption.empty())
      ge::hud::draw_spinner(overlay, ge::hud::frame_for(w, h, se::hud_style()), se::hud_style(), t,
                            caption, t - since);
    const wgpu::TextureView target = surface.acquire(*device);
    if (!target) return;
    cl::draw_world(*device, *title, cl::title_view(t), {}, nullptr, dt, overlay, target);
    shot_copy.frame(*device, surface.frame_target(), shots, console);
    surface.present();
  };
  title_frame(0, 0, "LOADING", 0);
  // The main menu's maps, its classic pictures when the packs carry them, and the packs classic
  // enhance offers.
  const cl::GameAdapter& adapter = cl::adapter_of(shooters_edge::profiles::classic_game());
  shooters_edge::menu::Host menu_host{.console = &console,
                                      .maps = adapter.list_maps(adapter, files)};
  cl::add_adapter_commands(console, adapter, files, args.basedir);
  cl::read_configs_from(console, adapter, files);  // `exec` finds a cfg in the game's packs too
  // DEMOS: the game's recordings anywhere on the machine, each checked before it plays.
  const auto demo_places = [&] {
    game_menu.demos().set_game_places(
        {{"GAME", cl::settings_dir(args.basedir, args.game).parent_path().string()}});
  };
  game_menu.demos().set_extensions(cl::listed_extensions(adapter.game->demo_extensions));
  demo_places();
  game_menu.demos().set_check(
      [&adapter](const std::string& path, std::string& facts, std::string& why) {
        cl::DemoFacts f;
        if (!cl::check_demo_file(path, adapter, f, why)) return false;
        facts = cl::facts_line(f);
        return true;
      });
  menu_host.maps.emplace_back("arena");
  menu_host.table = &adapter.menu(adapter);  // the game's own pages
  game_menu.set_table(*menu_host.table);
  // ce_pictures: the menu's and the status bar's pictures enhanced from the packs' cache; read
  // again when it flips and when an enhance run ends (client/classic_pictures.hpp). The first read
  // is on a thread, behind the title; the menu opens once they are in.
  const auto use_pictures = [&](cl::ClassicPictures pics, bool say) {
    if (!pics.menu.glyphs.empty()) game_menu.set_art(std::move(pics.menu));
    classic_art = std::move(pics.hud);
    classic_art.finish();
    if (!say) return pics.enhanced;
    if (!classic_art.bar.empty())
      out::status("shooters_edge: the HUD draws the packs' own status bar pictures" +
                  std::string(classic_art.density > 1 ? ", enhanced (ce_pictures)" : ""));
    else if (!cl::archive_paths(files).empty())
      out::status("shooters_edge: the HUD has no status bar pictures: " + pics.hud_why);
    return pics.enhanced;
  };
  const auto load_pictures = [&](bool say) {
    return use_pictures(cl::load_classic_pictures(files, picture.ce.pictures), say);
  };
  bool pictures_enhanced = picture.ce.pictures;
  std::future<cl::ClassicPictures> first_pictures =
      std::async(std::launch::async, [&files, enhance = picture.ce.pictures] {
        return cl::load_classic_pictures(files, enhance);
      });
  // The programs, the campaign and the first map the folder holds (client/game_folder.hpp).
  cl::GameContent game_content;
  if (!cl::load_programs(adapter, files, args.progs, game_content, error)) {
    out::error("shooters_edge: the mod did not load: " + error +
               "; see docs/shooters-edge.md § Running your own mod");
    return 2;
  }
  // With no map asked for, the game opens on the title with the main menu up, the first map
  // loading behind it; a measured run (a timedemo, --frames) loads its map before its first frame.
  const bool measured = !args.timedemo.empty() || !args.playdemo.empty() || args.frames != 0;
  bool on_title = next_map.empty() && !measured;
  bool menu_at_start = on_title;
  if (next_map.empty()) next_map = game_content.first_map;
  cl::ListenOptions listen_opt = shooters_edge::net::listen_options();
  listen_opt.files = &files;
  if (const ge::console::Cvar* n = console.cvars().find("name"); n != nullptr && !n->value.empty())
    listen_opt.player = n->value;
  listen_opt.lobby = listen.opt.lobby;  // the lobby's settings, as the console set them
  listen_opt.lobby_settings = listen.opt.lobby_settings;
  listen_opt.port = listen.opt.port;  // others join on it (sv_port)
  listen_opt.maxclients = listen.opt.maxclients;
  listen_opt.registered = game_content.registered;
  listen_opt.item_names = game_content.item_names;
  listen_opt.arsenal = game_content.arsenal;
  listen_opt.voice = remote.voip;  // cl_voip: the window's match relays its players' voices
  // The saves (the campaign's whole game: listen.hpp's `listen_save`), `<game>/saves/<name>.save`.
  std::string load_ask;  // a save the console asked to load (loaded where a map change is made)
  cl::campaign_commands(console, listen, files, game_content, next_map, load_ask);
  // The install's enhancing, in the background unless ce_auto is off or a run is measured.
  if (std::string basedir, why; opened && cl::check_game_folder(args.basedir, &basedir, &why)) {
    game_menu.set_game(basedir);
    game_menu.servers().set_masters(shooters_edge::net::classic_install_masters(basedir));
    // The game played is the one the next start opens, however it was picked (-basedir too).
    cl::remember_game(prefs, basedir);
    if (game_menu.auto_enhance() && !measured) {
      if (game_menu.enhance_game(false, &why))
        out::status("shooters_edge: enhancing " + basedir +
                    " in the background (ce_auto 0: only from CLASSIC ENHANCED > ENHANCE)");
      else
        out::warn("shooters_edge: the packs are not enhanced: " + why +
                  "; next: CLASSIC ENHANCED > ENHANCE in the main menu");
    }
  }
  // The first map is started where every map change is (the loop below, behind the window).
  listen.opt = listen_opt;
  // `sv_coop 1` from a cfg or the command line: the game starts as a coop game on the start map.
  cl::coop_frame(coop_mode, listen, game_content.campaign != nullptr, game_content.start_map,
                 next_map);

  // The held weapons: a classic game's own, else ours from the folder given, the tree's from here
  // or from where it was built (client/held.hpp).
  std::vector<ge::viewmodel::HeldModel> weapons;
  std::vector<std::string> model_dirs;
  if (!args.models.empty()) model_dirs.push_back(args.models);
  model_dirs.emplace_back("games/shooters_edge/content/models/view");
#ifdef SE_MODELS_DIR
  model_dirs.emplace_back(SE_MODELS_DIR);
#endif
  const bool weapons_at_eye = cl::load_held_set(adapter, files, model_dirs, weapons);
  // Each weapon's muzzle fire (world_fx.hpp), at the size its gun is drawn: a classic game's own
  // weapons fire the classic table's.
  const auto held_names = [&](bool at_eye) {
    return at_eye && adapter.game != nullptr ? adapter.game->held_weapons
                                             : std::span<const std::string_view>{};
  };
  cl::HeldFire held_fire;
  if (std::string why; !cl::load_held_fire(held_fire, &why, held_names(weapons_at_eye))) {
    out::error("shooters_edge: " + why +
               "; next: fix the weapon's fire in games/shooters_edge/view/weapons.cpp");
    return 1;
  }

  cl::WorldDrawPtr world;      // the map's, made behind the window (client/map_load.hpp)
  bool world_current = false;  // the world is the server's map's (not the last map's)
  cl::MapLoad loader;
  ge::platform::DevicesPtr devices = ge::platform::open_devices();
  if (!devices) return 1;
  const auto pull = [](float* out, int frames, void* user) {
    static_cast<ge::audio::Synth*>(user)->render(out, frames);
  };
  const std::unique_ptr<ge::platform::AudioOut> speaker =
      ge::platform::open_audio({.rate = ge::audio::kRate, .pull = pull, .user = &synth});
  out::status(speaker ? std::string("shooters_edge: audio on '") + speaker->driver() + "'"
                      : std::string("shooters_edge: no audio device; a demo plays silent"));
  cl::DemoHearing hearing(synth);       // a played demo's sounds, voices and printed lines
  cl::ListenHearing heard_here(synth);  // the window's own game's sounds (listen_sound.hpp)
  // The microphone, opened the first time +voip is held on a server joined (the system asks the
  // player's leave then, never at start) and read every frame after (net/classic_remote.hpp).
  std::unique_ptr<ge::platform::AudioIn> mic;
  bool mic_tried = false;
  std::vector<float> mic_samples(static_cast<size_t>(sn::kMicMostSamples));
  // Who has the mouse (input/mouse_capture.hpp): the look, whenever the window has the focus and
  // nothing in `in_mouse_release` asks for the pointer. Taken below on the first frame.
  ge::input::MouseCapture mouse;
  bool focused = ge::platform::has_focus(*window);
  bool vsync = true;
  bool full_applied = false;  // what the window was last set to; vid_fullscreen is the ask
  std::unique_ptr<cl::LoadedMap> bench_map;  // the benchmark's level (the world draws it)
  cl::DemoFolder bench_folder;               // the demo's game folder (timedemo.hpp's demo_folder)
  size_t bench_models = 0;         // the model list's length the demo's world was made with
  cl::FxFrame bench_fx;            // the demo's entities, players and effects this frame
  cl::FxState bench_fx_state;      // the demo's effects as they live (world_fx.hpp)
  cl::DemoFx bench_demo_fx;        // its temporary entities this level (demo_fx.hpp)
  ge::gpu::Target bench_off;       // the benchmark's frames that are not shown
  uint64_t bench_shown_ns = 0;     // when the benchmark last showed a frame
  wgpu::Future bench_in_flight{};  // the frame before this one, waited before the next
  if (!args.timedemo.empty()) bench_ask = args.timedemo;
  if (!args.playdemo.empty()) {
    bench_ask = args.playdemo;
    play_ask = true;
  }
  // The world drawn again for `map`, with the models on `load` standing in it, through `from`
  // (the demo's game folder while one plays, else the window's).
  const auto rebuild_world = [&](const cl::LoadedMap& map, const cl::ModelLoad& load,
                                 const cl::SearchPath& from) {
    world.reset();
    world = cl::create_world_draw(*device, from, map, static_cast<uint32_t>(w),
                                  static_cast<uint32_t>(h), surface.format(), picture, weapons,
                                  error, load);
    if (!world)
      out::error("shooters_edge: the world draw did not load: " + error +
                 "; see docs/rendering.md");
    return world != nullptr;
  };
  // The benchmark over: its report, vsync back, the listen server's world again.
  int bench_failed = 0;
  std::function<bool()> back_to_listen;  // below
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
           cl::bench_report(bench.name, setup, cl::bench_stats(bench.frame_s)))
        tell(line, now_s);
    if (!bench.paced && (why.empty() || !bench.frame_s.empty()))
      console.print(cl::span_report(bench.passes));
    if (!why.empty() && !bench.paced) {
      console.print("timedemo: stopped early: " + why);
      bench_failed = 1;
    }
    if (bench_in_flight.id != 0) ge::gpu::wait(*device, bench_in_flight);
    bench_in_flight = {};
    bench_off = {};
    bench.active = false;
    if (bench.quit_after) quit = true;
    return back_to_listen();
  };
  // A demo or a server left: the listen server's world again, vsync back.
  back_to_listen = [&] {
    bench_map.reset();
    bench_folder = {};
    bench_models = 0;
    hearing.stop();
    hud.centre.clear();
    vsync = true;
    surface.configure(*device, static_cast<uint32_t>(w), static_cast<uint32_t>(h), vsync);
    if (!listen.map || listen.server == nullptr) {  // run from the title: back to it
      world.reset();
      return true;
    }
    return rebuild_world(
        *listen.map,
        listen.server != nullptr ? cl::ModelLoad{listen.server->models, {}} : cl::ModelLoad{},
        files);
  };

  ge::input::Queue queue;
  ge::input::InputState held;
  const ge::input::InputState idle;
  ge::ui::ConsoleKeys con_keys;
  bool text_on = false;
  ge::platform::InputSnapshot input;
  cl::ViewState shown;
  ge::ui::Vec2 last_pointer;
  cl::FxState fx_state;
  cl::FxFrame fx;
  // The pack's palette for the classic effects' specks, read again when the game folder changes.
  // and the effects the game's files define (particles/*.fx, world_fx.hpp), read with it.
  std::vector<uint8_t> fx_palette;
  ge::particles::Book fx_book;
  bool fx_palette_read = false;
  const auto read_fx_files = [&] {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> pal = cl::find_file(files, "gfx/palette.lmp", store);
    fx_palette.assign(pal.begin(), pal.end());
    fx_book = {};
    std::vector<std::string> faults;
    cl::load_effects(files, fx_book, faults);
    for (const std::string& f : faults) console.print("particles: " + f);
    fx_palette_read = true;
  };
  cl::HeldState in_hand;
  in_hand.hand.at_eye = weapons_at_eye;
  // LOAD GAME's folder and the console's `basedir` (client/game_folder.hpp): the install opened in
  // place of the one playing (the old world and servers let go first: they view its packs), its
  // first map started where the map changes, its packs enhanced in the background, kept for the
  // next start. False, with nothing changed, when its folder does not open.
  std::string folder_ask;
  console.add_command("basedir",
                      "basedir <folder>: your game's install (the folder holding id1) loaded in "
                      "place of this one, its packs enhanced in the background",
                      [&](Con& c, const CArgs& a) {
                        if (a.size() < 2)
                          c.print("basedir: " + args.basedir + " (basedir <folder> loads another)");
                        else
                          folder_ask = cl::expand_home(a.size() == 2 ? a[1] : a.rest(0));
                      });
  const auto open_folder = [&](const std::string& basedir) {
    cl::SearchPath fresh;
    if (!cl::open_game_dir(basedir, cl::game_of(basedir), fresh, error)) return false;
    cl::cancel_load(loader);  // its threads read the old packs
    if (first_pictures.valid()) first_pictures.wait();
    first_pictures = {};
    world.reset();
    world_current = false;
    cl::listen_stop(listen);
    files = std::move(fresh);
    args.basedir = basedir;
    args.game = cl::game_of(basedir);
    console.set_search_path({cl::settings_dir(args.basedir, args.game), "."});
    if (!cl::load_programs(adapter, files, args.progs, game_content, error)) return false;
    listen.opt.registered = game_content.registered;
    listen.opt.item_names = game_content.item_names;
    listen.opt.arsenal = game_content.arsenal;
    cl::campaign_commands(console, listen, files, game_content, next_map, load_ask);
    cl::add_adapter_commands(console, adapter, files, basedir);
    menu_host.maps = adapter.list_maps(adapter, files);
    menu_host.maps.emplace_back("arena");
    pictures_enhanced = load_pictures(true);
    weapons.clear();
    in_hand = {};
    in_hand.hand.at_eye = cl::load_held_set(adapter, files, model_dirs, weapons);
    if (std::string why; !cl::load_held_fire(held_fire, &why, held_names(in_hand.hand.at_eye)))
      out::error("shooters_edge: " + why + "; next: fix the weapon's fire in its table");
    fx_palette_read = false;
    next_map = game_content.first_map;
    game_menu.set_game(basedir);
    game_menu.servers().set_masters(shooters_edge::net::classic_install_masters(basedir));
    demo_places();
    cl::remember_game(prefs, basedir);
    save_settings();  // the player's settings go on in the new folder
    out::status("shooters_edge: loaded " + basedir + " (" + files.folder + ")");
    if (std::string why; game_menu.auto_enhance() && !game_menu.enhance_game(true, &why))
      console.print("classic enhance: " + why);
    return true;
  };
  // A new map's or a new server's: what was seen of the last one let go, the user info sent again.
  const auto forget_seen = [&] {
    shown = {};
    fx_state = {};
    in_hand = {};
    held_fire.shots = {};
    user_info.sent_to = nullptr;
  };
  // The window's own game heard this frame (`dt` s), at the player's volume.
  const auto hear_here = [&](double dt) {
    synth.bus_gain(0, static_cast<float>(win.volume) / cl::kDemoVolume);
    heard_here.frame(listen, files, static_cast<float>(dt));
  };
  size_t printed = 0, centred = 0;
  uint64_t last = first_ns;
  long presented = 0;
  double longest = 0;  // the longest frame, s: a hitch a load or a stall made
  while ((args.frames == 0 || presented < args.frames) && !quit) {
    if (cl::arrived(first_pictures, measured))
      pictures_enhanced = use_pictures(first_pictures.get(), true);
    if (!first_pictures.valid() && picture.ce.pictures != pictures_enhanced)
      pictures_enhanced = load_pictures(false);
    queue.clear();
    ge::platform::poll_input(*window, input, *devices, queue);
    const uint64_t now = ge::platform::now_ns();
    const double wall = static_cast<double>(now - last) * 1e-9;
    const double t = static_cast<double>(now - first_ns) * 1e-9;
    last = now;
    if (presented > 0) longest = std::max(longest, wall);
    if (input.quit) {
      save_settings();
      break;
    }
    if (win.fullscreen != full_applied) {  // the size follows through `input.resized`
      if (!ge::platform::set_fullscreen(*window, win.fullscreen, win.display))
        win.fullscreen = full_applied;
      full_applied = win.fullscreen;
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
        mi.del = input.tapped(Key::del) || input.tapped(Key::backspace);  // a Mac's delete key
        mi.refresh = input.tapped(Key::r) || input.tapped(Key::f5);
        if (mouse.pointer()) {  // a held pointer is frozen: the menu then reads only the keys
          mi.pointer_x = input.pointer_x * px_per_unit;
          mi.pointer_y = input.pointer_y * px_per_unit;
          mi.pointer_moved = mi.pointer_x != last_pointer.x || mi.pointer_y != last_pointer.y;
          last_pointer = {mi.pointer_x, mi.pointer_y};
          mi.click = held.mouse.buttons.went_up(0);
        }
      }
      menu_host.server = listen.server ? &listen.server->cvars : nullptr;
      // A benchmark's Escape stops the run (below) and opens nothing.
      const bool opening =
          (!con_keys.held && !was_open && !bench.active && input.tapped(Key::escape)) ||
          (!first_pictures.valid() && std::exchange(menu_at_start, false));
      if (opening) {  // the slots' lines read again: a save may have been made since
        menu_host.saves.assign(shooters_edge::menu::kSaveSlots, {});
        for (int i = 0; i < shooters_edge::menu::kSaveSlots; i++)
          menu_host.saves[static_cast<size_t>(i)] = cl::save_label(files, "s" + std::to_string(i));
      }
      game_menu.update(mi, opening, menu_host, wall);
      cl::keys_to_menu(game_menu, menu_host, queue, held);            // CUSTOMIZE CONTROLS' key
      if (std::string demo = game_menu.take_demo(); !demo.empty()) {  // DEMOS: checked, played
        bench_ask = std::move(demo);
        play_ask = true;
      }
      if (was_open && !game_menu.open()) {
        save_settings();   // what the menu changed is kept
        on_title = false;  // the title's menu closed: the map is played (once it has loaded)
        // A setting the world reads once (the enhanced textures and skins, the rebaked light, the
        // bloom) changed: the world made again behind the window, the old one drawn meanwhile.
        if (world && listen.server != nullptr && listen.map && cl::world_stale(*world))
          cl::load_world(loader, files, *listen.map, picture, listen.server->models, adapter, t);
      }
    }
    // The mouse is the look's whenever the window has the focus: a click on the window or on the
    // app's icon in the Dock takes it, no click in the window needed. Only what `in_mouse_release`
    // names lets it go: the console by default, and a demo watched at its pace (its play bar); a
    // menu keeps it and is driven by the keys unless the menu's bit is set.
    if (input.focus_lost) focused = false;
    if (input.focus_gained) focused = true;
    const ge::input::CaptureFacts facts{.focused = focused,
                                        .console = con_view.open(),
                                        .menu = game_menu.open(),
                                        .watching = bench.active && bench.paced};
    switch (ge::input::follow_capture(mouse, facts, controls.mouse_release)) {
      case ge::input::CaptureStep::take:
        ge::platform::set_relative_mouse(*window, true);
        break;
      case ge::input::CaptureStep::let_go:
        ge::platform::set_relative_mouse(*window, false);
        break;
      case ge::input::CaptureStep::none:
        break;
    }
    const bool captured = mouse.held;
    const bool playing = !(con_keys.held || !captured || was_open);
    const ge::input::InputState& played = playing ? held : idle;
    // The mouse's buttons and wheel to the game's binds (the keyboard's went with the console's
    // keys above); the moves let go while the menu or the console has the keys.
    if (playing && !game_menu.open()) {
      cl::mouse_to_binds(console, held);
    } else {
      cl::release_buttons(controls);
    }
    if (input.resized) {
      w = input.width_px;
      h = input.height_px;
      surface.configure(*device, static_cast<uint32_t>(w), static_cast<uint32_t>(h), vsync);
      if (world)
        cl::resize_world(*device, *world, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
      cl::resize_world(*device, *title, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    }

    // A game folder asked for: LOAD GAME's chooser answered, or the console's `basedir`.
    if (game_menu.take_load())
      ge::platform::pick_folder(*window, "Load game: the folder holding id1", args.basedir);
    if (std::string picked, why; ge::platform::folder_picked(*window, &picked, &why)) {
      if (!why.empty()) game_menu.load_refused(why + "; next: basedir <folder> in the console");
      folder_ask = picked;  // "" when the player cancelled
    }
    if (!folder_ask.empty() && !bench.active) {
      std::string basedir;
      if (!cl::check_game_folder(folder_ask, &basedir, &error)) {
        game_menu.load_refused(error);
        console.print("load game: " + error);
      } else if (!open_folder(basedir)) {
        if (!listen.map) {  // let go of the old game, then the new one did not load
          error.insert(0, "shooters_edge: " + basedir + " did not load: ");
          out::error(error + "; see docs/shooters-edge.md § Playing it");
          return 2;
        }
        game_menu.load_refused(error);
        console.print("load game: " + error);
      }
      folder_ask.clear();
    }
    // A run of the enhancing ended: the world drawn again from what it wrote.
    // A run that changed nothing (every stage up to date) leaves the world as it is.
    if (game_menu.take_ended()) {
      const ge::classicenhance::BoardView b = game_menu.job()->view();
      console.print(b.finished ? "classic enhance: " + game_menu.game() + " enhanced"
                               : "classic enhance: stopped: " + b.error);
      const ge::classicenhance::RunReport report = game_menu.job()->report();
      if (std::any_of(report.stages.begin(), report.stages.end(),
                      [](const ge::classicenhance::StageResult& r) { return r.ran; })) {
        pictures_enhanced = load_pictures(false);
        if (world && listen.server != nullptr && listen.map && !bench.active)
          cl::load_world(loader, files, *listen.map, picture, listen.server->models, adapter, t);
      }
    }

    // Co-op as the window's mode: `sv_coop` turned on, the friendly-fire settings, the episodes.
    cl::coop_frame(coop_mode, listen, game_content.campaign != nullptr, game_content.start_map,
                   next_map);

    // A map asked for (the console's `map`, or the mod's level change at its limits), read behind
    // the window (client/map_load.hpp): started on the server once read, its world made behind
    // the window, drawn once its pipelines are compiled. A measured run waits for each step.
    if (next_map.empty() && !listen.pending_map.empty() && !cl::busy(loader))
      next_map = listen.pending_map;
    const bool elsewhere = bench.active || remote.active || bench_map;  // the world is theirs
    if (elsewhere) heard_here.stop();  // a demo or a server joined is heard instead
    if (!next_map.empty() && !loader.map.valid() && !elsewhere) {
      cl::load_map(loader, files, next_map, t);
      next_map.clear();
    }
    if (!elsewhere) {
      const cl::LoadWindow at{device.get(),
                              static_cast<uint32_t>(w),
                              static_cast<uint32_t>(h),
                              surface.format(),
                              &picture,
                              weapons,
                              &files,
                              &adapter,
                              &game_content,
                              [&] {  // the new map's first view, from where the player stands
                                cl::ViewState first;
                                return cl::frame_view(listen, first, 0, t, 0);
                              }};
      bool fresh = false;
      if (const int code =
              cl::take_load(loader, at, listen, console, measured, t, world, world_current, fresh))
        return code;
      if (fresh) forget_seen();
    }

    // A save asked for: the game it holds now playing (its world made behind the window, below).
    if (!load_ask.empty()) {
      const std::string name = std::exchange(load_ask, std::string());
      console.print(cl::load_asked_save(listen, game_content, files, name, error)
                        ? "loaded " + name
                        : "load: " + error);
    }

    // A benchmark asked for: vsync off, the listen server waits until it is done. A demo played
    // at its pace keeps vsync.
    if (!bench_ask.empty()) {
      const bool quit_after = bench_ask == args.timedemo || bench_ask == args.playdemo;
      if (!cl::timedemo_start(files, bench_ask, bench, error, adapter)) {
        console.print("timedemo: " + error);
        if (quit_after) {
          out::error("shooters_edge: the timedemo did not start: " + error +
                     "; see docs/shooters-edge.md § Benchmarking a demo");
          return 2;
        }
      } else {
        bench.quit_after = quit_after;
        bench.paced = play_ask;
        bench.hear = play_ask;  // its sounds and lines gathered for the draw (demo_heard.hpp)
        bench.demo.track = playback.track;
        console.print((play_ask ? "playdemo: " : "timedemo: ") + bench.found);
        if (play_ask && !cl::playback_begin(playback, bench, error))
          console.print("playdemo: no highlights: " + error);
        con_view.set_open(false);
        if (!play_ask) {
          vsync = false;  // and unlocked: immediate present, where the platform has it
          surface.configure_unlocked(*device, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
        }
      }
      args.timedemo.clear();
      args.playdemo.clear();
      bench_ask.clear();
      play_ask = false;
      sn::remote_disconnect(remote);  // a demo is not played over a game joined
    }
    // Joined to a server (`connect`): its game drawn as a played demo's is (classic_remote.hpp),
    // from the frame its first level is known to the frame it drops, the listen server held.
    // The microphone: opened the first time +voip is held where voice is carried (a server
    // joined, or the window's own match), then read every frame into the game that carries it.
    const bool talk_key = remote.voip_key != nullptr && remote.voip_key->down();
    const bool voice_here = !remote.active && cl::listen_voice(listen);
    if (!mic_tried && (sn::remote_talking(remote) || (voice_here && talk_key))) {
      mic_tried = true;
      mic = ge::platform::open_audio_in(ge::net::kVoiceRate);
      tell(mic ? "voice: the microphone is on while +voip is held"
               : "voice: no microphone opened (none, or the system's permission refused); next: "
                 "allow the microphone for shooters_edge and restart",
           t);
    }
    if (mic) {
      const int n = ge::platform::read_audio_in(*mic, mic_samples.data(),
                                                static_cast<int>(mic_samples.size()));
      const std::span<const float> heard_mic{mic_samples.data(), static_cast<size_t>(n)};
      if (remote.active)
        sn::remote_mic(remote, heard_mic);
      else if (voice_here)
        cl::listen_mic(listen, heard_mic, talk_key);
    }
    auto joined = sn::RemoteStep::waiting;
    if (remote.active || (bench_map && !bench.active)) {
      const bool hands = playing && !game_menu.open();
      sn::remote_hands(remote, hands ? cl::button_moves(controls) : cl::Moves{}, played, controls,
                       wall);
      std::vector<std::string> lines;
      joined = sn::remote_step(remote, wall, lines);
      for (const std::string& line : lines) tell(line, t);
      if (!remote.active && bench_map && !back_to_listen()) return 1;
    }
    const bool drawn_remote = remote.active && (bench_map || joined == decltype(joined)::new_level);
    if (bench.active && input.tapped(ge::platform::Key::escape)) {
      if (!end_bench("Escape pressed", t)) return 1;
      continue;
    }
    if (bench.active || drawn_remote) {
      std::vector<std::string> said;
      const bool paced = bench.paced || drawn_remote;  // every frame shown, nothing timed
      const cl::TimedemoStep step =
          drawn_remote ? (joined == decltype(joined)::new_level ? cl::TimedemoStep::new_level
                                                                : cl::TimedemoStep::frame)
          : bench.paced
              ? cl::playback_step(playback, bench,
                                  {con_view.open() ? std::span<const bool>{} : input.pressed,
                                   input.pointer_x * px_per_unit, input.pointer_y * px_per_unit,
                                   (input.pointer_buttons & 1u) != 0},
                                  w, h, wall, t, said, error)
              : cl::timedemo_step(bench, error);
      for (const std::string& line : said) console.print(line);
      // A paced demo is drawn between its frames (demo_lerp.hpp), so it moves smoothly.
      const cl::DemoState& ds = drawn_remote ? remote.demo.state : cl::demo_drawn(bench);
      if (step == cl::TimedemoStep::new_level) {
        // The game folder the recording's server ran, mounted when the window is not on it.
        std::vector<std::string> lines;
        const cl::SearchPath& from =
            cl::demo_folder(bench_folder, args.basedir, args.game, ds, files, lines);
        for (const std::string& line : lines) console.print("playdemo: " + line);
        bench_map = cl::load_named_map(from, ds.map, error);
        if (!bench_map) {
          if (!end_bench("the demo's map " + ds.map + ": " + error, t)) return 1;
          continue;
        }
        if (!rebuild_world(*bench_map, {ds.models, {}}, from)) return 1;
        bench_models = ds.models.size();
        bench_fx_state = {};  // a new level: what burned on the last one is gone
        bench_demo_fx = {};
        if (drawn_remote) sn::remote_world(remote, &bench_map->spec.models);
        if (paced) hearing.level(ds, from, &bench_map->bsp.map);
      } else if (step != cl::TimedemoStep::frame) {
        if (!end_bench(step == cl::TimedemoStep::error ? error : "", t)) return 1;
        continue;
      }
      // A client recording's model list may grow after its level started: the world made again.
      if (bench_map && ds.models.size() != bench_models) {
        const cl::SearchPath& from = bench_folder.path ? *bench_folder.path : files;
        if (!rebuild_world(*bench_map, {ds.models, {}}, from)) return 1;
        bench_models = ds.models.size();
      }
      if (paced) {  // what the frames read carried: heard, and its lines shown
        synth.bus_gain(0, static_cast<float>(win.volume) / cl::kDemoVolume);
        std::vector<std::string> lines;
        cl::DemoHeard joined_heard;  // a game joined: what its frames read carried (remote_step)
        if (drawn_remote) joined_heard = {ds.started, ds.stopped, ds.voice, ds.prints, ds.centre};
        if (hearing.frame(ds, drawn_remote ? joined_heard : bench.heard, static_cast<float>(wall),
                          lines, hud.centre))
          hud.centre_until = t + 2;
        for (const std::string& line : lines) tell(line, t);
      }
      if (!world) continue;  // no level of the demo's yet
      ge::render::FrameView view = cl::demo_view(ds, ge::camera::FirstPersonRow{}.fov_y, t);
      view.frame = static_cast<uint64_t>(presented);
      overlay.begin(w, h);
      se::fill_hud(hud, cl::demo_status(ds, t), cl::hud_art_of(files, classic_art, free_art));
      const auto placed =
          hud.layout.draw(overlay, hud.layout.frame(w, h, se::hud_style()), se::hud_style());
      playback.bar_bottom_px = cl::hud_inset(placed, h);
      if (bench.paced) cl::playback_draw(overlay, playback, bench, w, h, t);
      if (drawn_remote) {  // a game joined: its menu and console over it
        game_menu.draw(overlay, w, h, t);
        con_view.frame(wall);
        ge::ui::draw_console(overlay, con_view, {}, nullptr, t);
      }
      // Every frame is drawn, but only one in kBenchShowNs is shown: the rest go to an offscreen
      // target of the surface's format. A window's present waits for the display however its
      // mode is set (macOS's compositor hands an image back once a refresh), so a frame rate
      // timed through it is the display's; drawn offscreen it is the machine's.
      const uint64_t frame_ns = ge::platform::now_ns();
      wgpu::TextureView target;
      const bool show = paced || frame_ns - bench_shown_ns >= kBenchShowNs;
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
      // Its entities, other players and effects (its explosions, trails, hits, beams).
      if (!fx_palette_read) {
        read_fx_files();
        bench_fx_state.palette.clear();
      }
      if (bench_fx_state.palette.empty()) bench_fx_state.palette = fx_palette;
      bench_fx_state.book = &fx_book;
      bench_fx_state.models = &cl::world_models(*world);
      bench_fx_state.on = cl::fx_switches(picture);
      cl::demo_fx_step(ds, bench_demo_fx, bench_map.get(), view, bench_fx_state, t, wall, bench_fx);
      auto hand = cl::step_demo_held(ds, in_hand, view_settings, weapons, view, t, wall);
      hand.lift = se::status_lift(hud, h);  // the gun over the classic status bar
      cl::draw_world(*device, *world, view, bench_fx, &hand, wall, overlay, target);
      if (!paced && bench.skip == 0) cl::record_span(*world, bench.passes);
      if (bench_shown_ns == frame_ns) {
        shot_copy.frame(*device, surface.frame_target(), shots, console);
        surface.present();
      }
      if (paced) {  // playback: every frame shown, nothing timed
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

    // The title (no world of the server's map yet, or the menu still up over the title): the
    // menu and the console over the night, the spinner while a map loads, the server waiting. A
    // save loaded from it is its world made behind the window.
    if (!world || !world_current || listen.server == nullptr || (on_title && game_menu.open())) {
      if (listen.map_changed && listen.map && listen.server != nullptr) {
        listen.map_changed = false;
        world_current = false;
        cl::load_world(loader, files, *listen.map, picture, listen.server->models, adapter, t);
      }
      title_frame(t, wall, cl::load_caption(loader), loader.since_s);
      ++presented;
      continue;
    }
    cl::send_user_info(listen, console, user_info);  // the name and colours, as they change
    // The lobby's keys (F1 ready or break, F2 the other team, F4 the warmup or the lobby's
    // screen; on the screen F5 and F6 a bot more or less, ENTER the start), then the player's
    // hands, then the server's frames.
    {
      using ge::platform::Key;
      const bool keys = !con_keys.held && !con_view.open() && !game_menu.open();
      const auto tap = [&](Key k) { return keys && input.tapped(k); };
      cl::lobby_keys(listen, {.f1 = tap(Key::f1),
                              .f2 = tap(Key::f2),
                              .f4 = tap(Key::f4),
                              .f5 = tap(Key::f5),
                              .f6 = tap(Key::f6),
                              .enter = tap(Key::enter)});
    }
    // The lobby's screen (hud/lobby_page.hpp, or the classic look's): the player is not on the
    // map, so it is drawn over the main menu's backdrop while the server runs on (the bots and the
    // warmup fight on without him), until he joins the warmup or the match starts.
    if (cl::on_lobby_screen(listen)) {
      const int frames = cl::listen_advance(listen, wall, ge::shooter::MoveCmd{});
      cl::record_tick(recorder, listen, frames, console);
      hear_here(wall);
      if (std::exchange(listen.restarted, false)) forget_seen();
      for (; printed < listen.prints.size(); printed++) tell(listen.prints[printed], t);
      title_frame(t, wall, {}, 0, true);
      ++presented;
      continue;
    }
    const cl::Moves moves = playing ? cl::button_moves(controls) : cl::Moves{};
    cl::turn_view(played, moves, controls, wall, listen.view);
    const ge::shooter::MoveCmd cmd = cl::read_controls(moves, played, controls);
    if (listen.server != nullptr) {
      const cl::PlayerState me = cl::player_state(listen);
      cl::spring_view(played, controls, me.on_ground, cmd, me.ideal_pitch, wall, listen.view);
    }
    if (cmd.impulse != 0) listen.impulse = cmd.impulse;
    // A single-player game waits while the console or the menu is up.
    const bool paused = listen.campaign_host && (con_view.open() || game_menu.open());
    const int frames = paused ? 0 : cl::listen_advance(listen, wall, cmd);
    cl::record_tick(recorder, listen, frames, console);
    hear_here(wall);
    if (!listen.failed.empty()) {
      out::error("shooters_edge: the campaign's next map did not start: " + listen.failed +
                 "; see docs/shooters-edge.md § Playing it");
      return 2;
    }
    if (listen.map_changed) {  // the campaign went on to its next map: its world made behind
      listen.map_changed = false;
      world_current = false;
      cl::load_world(loader, files, *listen.map, picture, listen.server->models, adapter, t);
      continue;  // the title from the next frame, not the last map from the new one's eyes
    }
    if (std::exchange(listen.restarted, false)) forget_seen();  // the lobby's match restarted
    if (listen.campaign.sellscreen) {  // the shareware episode's end: the help pages
      listen.campaign.sellscreen = false;
      game_menu.show(menu_host, "help");
    }
    for (; printed < listen.prints.size(); printed++) tell(listen.prints[printed], t);
    if (centred > listen.centres.size()) centred = 0;     // a new listen
    for (; centred < listen.centres.size(); centred++) {  // a centre print: the last one shows
      hud.centre = listen.centres[centred];
      hud.centre_until = hud.centre.empty() ? 0 : t + 2;
    }
    // The eyes on the player and what the world throws, then the overlay: the HUD, the console.
    ge::render::FrameView view = cl::frame_view(listen, shown, frames, t, wall);
    view.frame = static_cast<uint64_t>(presented);
    fx_state.models = world ? &cl::world_models(*world) : nullptr;
    if (!fx_palette_read) {
      read_fx_files();
      fx_state.palette.clear();
    }
    if (fx_state.palette.empty()) fx_state.palette = fx_palette;
    fx_state.book = &fx_book;
    fx_state.on = cl::fx_switches(picture);  // the enhanced weapon effects, by their ce_ switches
    cl::step_fx(listen, shown, fx_state, t, wall, fx);
    view_settings.lerp = picture.lerp_models;  // r_lerpmodels: the held weapon's frames too
    shooters_edge::view::HandFrame hand =
        cl::step_held(listen, shown, in_hand, view_settings, weapons, t, wall);
    if (hand.flashing)
      fx.lights.push_back(shooters_edge::effects::muzzle_light(hand.flash.position));
    // ce_muzzles in the modern look: each shot's flame, smoke and sparks from the barrel (off, or
    // the classic look: the weapon's own flash and its light alone).
    cl::fire_held(held_fire, hand, fx_state.on.muzzles, view.time_s, fx);
    overlay.begin(w, h);
    if (listen.campaign.end == cl::LevelEnd::none) {
      se::fill_hud(hud, cl::status_of(listen, t), cl::hud_art_of(files, classic_art, free_art));
      const ge::hud::Frame hf = hud.layout.frame(w, h, se::hud_style());
      hud.layout.draw(overlay, hf, se::hud_style());
      hand.lift = se::status_lift(hud, h);  // the gun over the classic status bar
    }
    if (listen.room && listen.server != nullptr) {
      // The warmup's banner in the packs' own menu pictures when they carry them, else modern.
      se::LobbyView lv = cl::listen_lobby_view(listen);
      // A classic game counts down in its own centre print (server/announce.hpp).
      lv.count = shooters_edge::server::words_of(listen.server->localinfo, "count_head").empty();
      if (game_menu.art().loaded())
        shooters_edge::menu::draw_lobby_classic(overlay, game_menu.art(), lv, w, h);
      else
        se::draw_lobby(overlay, lv, w, h);
    }
    cl::draw_scores(overlay, listen, game_menu.art(), w, h,
                    !con_keys.held && !was_open && input.down(ge::platform::Key::tab),
                    win.level_stats, board_view);
    game_menu.draw(overlay, w, h, t);
    con_view.frame(wall);
    ge::ui::draw_console(overlay, con_view, {}, nullptr, t);
    if (cl::busy(loader))  // the world made again behind the one drawn
      ge::hud::draw_spinner(overlay, ge::hud::frame_for(w, h, se::hud_style()), se::hud_style(), t,
                            cl::load_caption(loader), t - loader.since_s);

    wgpu::TextureView target = surface.acquire(*device);
    if (!target) continue;
    cl::draw_world(*device, *world, view, fx, &hand, wall, overlay, target);
    shot_copy.frame(*device, surface.frame_target(), shots, console);
    surface.present();
    ++presented;
  }
  if (quit) save_settings();
  const double seconds = static_cast<double>(ge::platform::now_ns() - first_ns) * 1e-9;
  char line[200];
  std::snprintf(line, sizeof line,
                "shooters_edge: %ld frames in %.2f s (%.1f ms a frame, the longest %.0f ms) at "
                "%dx%d, %u GPU errors",
                presented, seconds,
                presented ? seconds * 1e3 / static_cast<double>(presented) : 0.0, longest * 1e3, w,
                h, device->errors.load());
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
