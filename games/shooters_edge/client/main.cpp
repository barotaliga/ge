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
//                 [-timedemo <demo>] [-playdemo <demo>] [--frames=N] [--size=WxH] [-config <f>]
//                 [-demovideo <demo> <file> [--from=m:ss --to=m:ss | --highlight=N] ...]
//                 [-logfile <file>] [+<command> ...]
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
// are kept in `<game folder>/settings/config.cfg` (`-config <f>`: a session's own; settings.hpp).
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
#include <filesystem>
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
#include "gpu/frame_gate.hpp"
#include "gpu/surface.hpp"
#include "gpu/target.hpp"
#include "hud/frame_graph.hpp"
#include "hud/spinner.hpp"
#include "input/state.hpp"
#include "net/voice_codec.hpp"
#include "platform/audio.hpp"
#include "platform/devices.hpp"
#include "platform/pacing.hpp"
#include "platform/window.hpp"
#include "play/session_flags.hpp"
#include "play/window_output.hpp"
#include "render/screenshot_copy.hpp"
#include "shooters_edge/bots/handles.hpp"
#include "shooters_edge/client/args.hpp"
#include "shooters_edge/client/bot_view.hpp"
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
#include "shooters_edge/client/install_configs.hpp"
#include "shooters_edge/client/install_search.hpp"
#include "shooters_edge/client/installs.hpp"
#include "shooters_edge/client/listen_record.hpp"
#include "shooters_edge/client/listen_sound.hpp"
#include "shooters_edge/client/lobby_console.hpp"
#include "shooters_edge/client/map_load.hpp"
#include "shooters_edge/client/menu_sound.hpp"
#include "shooters_edge/client/own_sounds.hpp"
#include "shooters_edge/client/place_names.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/scores.hpp"
#include "shooters_edge/client/sequel_bar.hpp"
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
#include "shooters_edge/menu/classic_console.hpp"
#include "shooters_edge/menu/game_menu.hpp"
#include "shooters_edge/menu/level_screens.hpp"
#include "shooters_edge/menu/lobby_screen.hpp"
#include "shooters_edge/net/classic_admin.hpp"
#include "shooters_edge/net/classic_bot_team.hpp"
#include "shooters_edge/net/classic_browse.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/net/classic_remote.hpp"
#include "shooters_edge/net/classic_team_areas.hpp"
#include "shooters_edge/net/classic_team_view.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/sequel_server.hpp"
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

}  // namespace

int run_main(int argc, char** argv) {
  cl::Args args;
  std::string log_error;
  // The flags every game takes (play/session_flags.hpp): -config and -logfile, off first.
  ge::play::SessionFlags session;
  if (std::string why; !ge::play::take_session_flags(argc, argv, session, why)) {
    ge::play::attach_window_output("", log_error);
    out::error("shooters_edge: the command line was refused: " + why +
               "; next: -config <file> and -logfile <file> each take a file");
    return 2;
  }
  if (std::string why; !cl::parse(argc, argv, args, why)) {
    ge::play::attach_window_output("", log_error);
    out::error("shooters_edge: the command line was refused: " + why +
               " Usage: shooters_edge [-basedir <dir>] [-game <folder>] "
               "[-config <file>] [-logfile <file>] [-progs <file>] [-models <dir>] [-timedemo "
               "<demo>] [-playdemo <demo>] "
               "[--frames=N] [--size=WxH] [-demovideo <demo> <file> [--fps=N] "
               "[--resolution=<WxH|720p|1080p|1440p|4k>] [--from=m:ss] [--to=m:ss] [--highlight=N] "
               "[--gif-width=N] [--codec=<c>] [--seconds=S]] "
               "[+<command> ...]; next: rerun with only those flags.");
    return 2;
  }
  // Every line on the terminal, and in `-logfile <file>` too when it is named.
  if (!ge::play::apply_session_flags(session, nullptr, log_error))
    out::error("shooters_edge: the log file was not opened, the terminal only: " + log_error +
               "; next: name a file in a folder you can write");

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
  console.add_command("map", "map <name>: play the map (\"map arena\" is built in)",
                      [&](Con& c, const CArgs& a) {
                        if (a.size() < 2)
                          c.print(listen.map ? "map: " + listen.map->name : "map: none");
                        else
                          next_map = a[1];
                      });
  console.add_command("sv", "sv <line>: a line for the server's console",
                      [&](Con& c, const CArgs& a) {
                        std::string out;  // the administrator's words first (users, speedjump)
                        if (cl::listen_admin(listen, a.rest(0), out)) {
                          for (size_t at = 0; at < out.size();) {
                            const size_t end = std::min(out.find('\n', at), out.size());
                            c.print(out.substr(at, end - at));
                            at = end + 1;
                          }
                          return;
                        }
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
  // vid_vsync, cl_maxfps, cl_maxfps_hz, vid_framelatency, scr_showfps and `framestats`.
  ge::platform::Pacing pacing;
  ge::platform::add_pacing_cvars(console, pacing);
  std::string bench_ask;  // a demo the console asked to benchmark (`timedemo`) or play
  bool play_ask = false;  // the demo asked for is played at its own pace (`playdemo`)
  cl::add_demo_commands(console, bench_ask, play_ask);
  cl::Timedemo bench;
  sn::Remote remote;  // `connect`: the window on a server elsewhere
  sn::add_remote_commands(console, remote);
  sn::add_admin_commands(console, listen, remote);  // users, status, speedjump, rcon
  // The vote on a setting (engine/server/vote.hpp): to the server joined, else the window's own.
  for (const char* word : {"vote", "yes", "no"})
    console.add_command(word,
                        "vote <setting> <value>: ask everyone to change a setting for this map; "
                        "vote yes, vote no (or yes, no): answer; vote: what is open; vote rules",
                        [&](Con&, const CArgs& a) {
                          std::string line = a[0];
                          if (const std::string rest = a.rest(0); !rest.empty()) line += " " + rest;
                          if (remote.active)
                            sn::client_command(remote.end, line);
                          else
                            cl::listen_lobby(listen, line);
                        });
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
  // The console in the loaded classic game's own letters and background (menu/classic_console.hpp).
  shooters_edge::menu::ClassicConsole con_look;
  // menu_attract: the recordings a classic game plays behind its main menu (below).
  std::string attract_demos;
  console.cvars().bind({.name = "menu_attract",
                        .fallback = "auto",
                        .flags = ge::console::kArchive,
                        .help = "The recordings a classic game plays in turn behind its main "
                                "menu.",
                        .kind = ge::console::CvarKind::text,
                        .values = "auto: the game's own (the default); \"\": none, the title's "
                                  "night sky; TEXT: these recordings, by name"},
                       &attract_demos);
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
  // Started inside an install (or placed in one, or above several: client/install_search.hpp), that
  // one.
  const std::string prefs = ge::platform::prefs_dir("gameengine2", "shooters_edge");
  std::vector<cl::FoundInstall> installs;  // the console's `games`
  cl::choose_folder(args, prefs, {cl::placed_folder(ge::platform::app_dir())}, &installs);
  // The player's settings (client/settings.hpp); the folder the game runs in is read after them.
  // The install's configs in its own order, its folders on `exec`'s path
  // (client/install_configs.hpp).
  const auto content_row = [&]() -> const ge::classicenhance::Profile& {
    const ge::classicenhance::Profile* c = ge::classicenhance::find_profile(args.profile);
    return c != nullptr ? *c : ge::classicenhance::classic_profile();
  };
  console.set_search_path(cl::config_dirs(args.basedir, args.game, content_row()));
  // A session's own config (`-config <file>`): read and saved there, the player's left alone.
  if (!session.config.empty()) console.set_config_file(session.config);
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
  // SELECT GAME: every classic game, its install kept on this computer and its edition.
  game_menu.games().set_lister([&] {
    std::vector<shooters_edge::menu::GameRow> rows;
    const auto kept = cl::remembered_installs(prefs);
    for (const ge::classicenhance::Profile& p : ge::classicenhance::kProfiles) {
      shooters_edge::menu::GameRow g;
      g.id = std::string(p.id);
      if (const auto it = kept.find(p.id); it != kept.end()) g.install = it->second;
      if (args.profile == p.id && !game_menu.game().empty()) {
        g.playing = true;
        g.install = game_menu.game();
      }
      if (std::string basedir, why; !g.install.empty()) {
        if (cl::check_game_folder(g.install, &basedir, &why, p))
          g.edition = std::string(cl::edition_words(cl::edition_of(basedir, p), p));
        else
          g.missing = true;
      }
      rows.push_back(std::move(g));
    }
    return rows;
  });
  const auto save_settings = [&console] { cl::save_settings(console); };
  if (const cl::ConfigsRun ran =
          cl::start_configs(console, args.basedir, args.game, content_row(), args.plus);
      ran.passed_over > 0)
    out::status("shooters_edge: " + std::to_string(ran.passed_over) +
                " lines of the install's configs name settings this game does not have; "
                "passed over");
  console.cvars().apply_latched();

  // The game the install is (args.profile, found by its folders: client/game_folder.hpp).
  const auto game_row = [](const std::string& id) -> const shooters_edge::profiles::GameProfile& {
    const shooters_edge::profiles::GameProfile* g = shooters_edge::profiles::find_game(id);
    return g != nullptr ? *g : shooters_edge::profiles::classic_game();
  };
  cl::SearchPath files;
  // The sequel's status bar, drawn from its rules' own program (sequel_bar.hpp).
  cl::SequelBar sequel_bar;
  sequel_bar.files = &files;
  // The HUD's type: the loaded classic game's glyph sheet writes its messages (hud/layout.hpp).
  const auto hud_look = [&] { return se::hud_style(cl::hud_art_of(files, classic_art, free_art)); };
  cl::ListenRecorder recorder;  // `record` and `stop` (listen_record.hpp)
  cl::add_record_commands(console, recorder, listen, files, playback.record);
  std::string error;
  // No packs and no `+map`: the built-in arena (the start map below falls back to it).
  const bool opened =
      cl::open_game_dir(args.basedir, args.game, files, error,
                        shooters_edge::profiles::content_of(game_row(args.profile)));
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
  remote.read_file = [&files, &args, game_row,
                      other = std::make_shared<std::pair<std::string, cl::SearchPath>>()](
                         const std::string& gamedir, const std::string& path) {
    std::vector<uint8_t> store;
    const cl::SearchPath* from = &files;
    if (!gamedir.empty() && gamedir != args.game &&
        gamedir != shooters_edge::profiles::content_of(game_row(args.profile)).base_dir) {
      if (other->first != gamedir) {
        std::string why;
        other->second = {};
        if (!cl::open_game_dir(args.basedir, gamedir, other->second, why,
                               shooters_edge::profiles::content_of(game_row(args.profile))))
          return store;
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
  // The frames the GPU may queue (vid_framelatency): waited on before each frame reads its input.
  ge::gpu::FrameGate gate;
  bool vsync_applied = pacing.settings.vsync;
  int32_t latency_applied = pacing.settings.latency;
  surface.configure(*device, static_cast<uint32_t>(w), static_cast<uint32_t>(h), vsync_applied);
  ge::platform::set_swap_images(*window, ge::platform::swap_images(pacing.settings));
  pacing.present = surface.present_mode();
  // Every frame the player watches is acquired and presented through these two, so the pacing
  // sees each one: the acquire timed (framestats' image wait; one that blocked marks a refresh
  // for vid_vsync_lag_fix), and after the present the gate told and, while the lag fix is on,
  // the frame's cost to its GPU's last work measured.
  const auto acquire_frame = [&] {
    const uint64_t before = ge::platform::now_ns();
    wgpu::TextureView view = surface.acquire(*device);
    ge::platform::acquired(pacing, before, ge::platform::now_ns());
    return view;
  };
  const auto present_frame = [&] {
    surface.present();
    gate.submitted(*device);
    if (ge::platform::late_start_on(pacing)) {
      gate.wait(*device, 1);
      ge::platform::frame_done(pacing, ge::platform::now_ns());
    }
  };
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
    shooters_edge::menu::draw_game_console(overlay, con_view, con_look, t);
    if (!caption.empty())
      ge::hud::draw_spinner(overlay, ge::hud::frame_for(w, h, se::hud_style()), se::hud_style(), t,
                            caption, t - since);
    const wgpu::TextureView target = acquire_frame();
    if (!target) return;
    cl::draw_world(*device, *title, cl::title_view(t), {}, nullptr, dt, overlay, target);
    shot_copy.frame(*device, surface.frame_target(), shots, console);
    present_frame();
  };
  title_frame(0, 0, "LOADING", 0);
  // The main menu's maps, its classic pictures when the packs carry them, and the packs classic
  // enhance offers.
  // The install's game's adapter: copied here, so LOAD GAME of another game's install swaps it in
  // place for every call that reads it.
  cl::GameAdapter adapter = cl::adapter_of(game_row(args.profile));
  shooters_edge::menu::Host menu_host{.console = &console,
                                      .maps = adapter.list_maps(adapter, files)};
  cl::add_adapter_commands(console, adapter, files, args.basedir);
  cl::read_configs_from(console, adapter, files);  // `exec` finds a cfg in the game's packs too
  // The map's place names, read as it starts and made in the game (client/place_names.hpp).
  cl::PlaceNames places;
  places.game_dir = std::filesystem::path(args.basedir) / args.game;
  cl::add_place_commands(console, places, adapter, files, [&listen](ge::shooter::Vec3f& at) {
    if (listen.server == nullptr || !listen.map || listen.server->world.entities.size() < 2)
      return false;
    at = listen.server->world.entities[1].origin;
    return true;
  });
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
    sequel_bar.pictures.clear();  // read again from the packs now open
    // Always the loaded game's own: another game's install loaded leaves none of the last one's.
    game_menu.set_art(std::move(pics.menu));
    shooters_edge::menu::classic_console(game_menu.art(), con_look);
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
  if (!cl::load_programs(adapter, files, args.progs, game_content, error, *adapter.game)) {
    out::error("shooters_edge: the mod did not load: " + error +
               "; see docs/shooters-edge.md § Running your own mod");
    return 2;
  }
  // With no map asked for, the game opens on the title with the main menu up, the first map
  // loading behind it; a measured run (a timedemo, --frames) loads its map before its first frame.
  const bool measured = !args.timedemo.empty() || !args.playdemo.empty() || args.frames != 0;
  bool on_title = next_map.empty() && !measured;
  bool menu_at_start = on_title;
  // A classic game opened on its title plays its recordings behind the main menu (`menu_attract`,
  // below) in place of loading its first map; the first map loads when they cannot play.
  bool attract_at_start = on_title && opened && !attract_demos.empty();
  if (next_map.empty() && !attract_at_start) next_map = game_content.first_map;
  cl::ListenOptions listen_opt = shooters_edge::net::listen_options();
  listen_opt.files = &files;
  if (const ge::console::Cvar* n = console.cvars().find("name"); n != nullptr && !n->value.empty())
    listen_opt.player = n->value;
  listen_opt.lobby = listen.opt.lobby;  // the lobby's settings, as the console set them
  listen_opt.lobby_settings = listen.opt.lobby_settings;
  listen_opt.port = listen.opt.port;  // others join on it (sv_port)
  listen_opt.maxclients = listen.opt.maxclients;
  listen_opt.registered = game_content.registered;
  listen_opt.episodes = game_content.episodes;
  listen_opt.item_names = game_content.item_names;
  listen_opt.arsenal = game_content.arsenal;
  listen_opt.voice = remote.voip;  // cl_voip: the window's match relays its players' voices
  // The saves (the campaign's whole game: listen.hpp's `listen_save`), `<game>/saves/<name>.save`.
  std::string load_ask;  // a save the console asked to load (loaded where a map change is made)
  cl::campaign_commands(console, listen, files, game_content, next_map, load_ask);
  // The install's enhancing, in the background unless ce_auto is off or a run is measured.
  if (std::string basedir, why;
      opened && cl::check_game_folder(args.basedir, &basedir, &why,
                                      shooters_edge::profiles::content_of(*adapter.game))) {
    game_menu.set_game(basedir);
    game_menu.servers().set_masters(shooters_edge::net::classic_install_masters(basedir));
    // The game played is the one the next start opens, however it was picked (-basedir too).
    cl::remember_game(prefs, basedir, shooters_edge::profiles::content_of(*adapter.game));
    cl::remember_install(prefs, shooters_edge::profiles::content_of(*adapter.game).id, basedir);
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
  // The hearings before the device: it is closed first, so its thread never reads a recording
  // they free on the way out.
  cl::DemoHearing hearing(synth);       // a played demo's sounds, voices and printed lines
  cl::ListenHearing heard_here(synth);  // the window's own game's sounds (listen_sound.hpp)
  cl::MenuSounds menu_sounds(synth);    // the main menu's clicks (menu_sound.hpp)
  const std::unique_ptr<ge::platform::AudioOut> speaker =
      ge::platform::open_audio({.rate = ge::audio::kRate, .pull = pull, .user = &synth});
  out::status(speaker ? std::string("shooters_edge: audio on '") + speaker->driver() + "'"
                      : std::string("shooters_edge: no audio device; a demo plays silent"));
  // The microphone, opened the first time +voip is held on a server joined (the system asks the
  // player's leave then, never at start) and read every frame after (net/classic_remote.hpp).
  std::unique_ptr<ge::platform::AudioIn> mic;
  bool mic_tried = false;
  std::vector<float> mic_samples(static_cast<size_t>(sn::kMicMostSamples));
  // Who has the mouse (input/mouse_capture.hpp): the look, whenever the window has the focus and
  // nothing in `in_mouse_release` asks for the pointer. Taken below on the first frame.
  ge::input::MouseCapture mouse;
  bool focused = ge::platform::has_focus(*window);
  bool paused_before_menu = false;  // a played demo's pause when the menu opened over it
  bool vsync = true;  // false while a timedemo runs unlocked; the player's vid_vsync otherwise
  // The surface at the window's size, its present mode the player's vid_vsync (a timedemo's
  // unlocked one aside), its swap chain's images by vid_framelatency.
  const auto configure_surface = [&] {
    vsync_applied = pacing.settings.vsync;
    latency_applied = pacing.settings.latency;
    surface.configure(*device, static_cast<uint32_t>(w), static_cast<uint32_t>(h), vsync_applied);
    ge::platform::set_swap_images(*window, ge::platform::swap_images(pacing.settings));
    pacing.present = surface.present_mode();
  };
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
  // `by_player`: the player stopped a played demo (Escape on a timedemo, or something else asked
  // for from the menu or the console over a paused one): not a failure.
  // THE RECORDINGS BEHIND THE MENU (`menu_attract`): while the title is up in a classic game, its
  // recordings play in turn behind the main menu, which leaves them playing; ESCAPE closes the
  // menu to watch one. Anything else asked for (a map, a game joined, a demo picked) stops them.
  bool attract = false;        // the demo playing is one of them
  bool attract_asked = false;  // one was asked for and has not started yet
  size_t attract_next = 0;     // the next one's place in the list
  const auto start_attract = [&] {
    const std::string list =
        attract_demos == "auto" ? std::string(adapter.game->attract) : attract_demos;
    std::vector<std::string> words;
    for (size_t at = 0; at < list.size();) {
      const size_t end = std::min(list.find(' ', at), list.size());
      if (end > at) words.push_back(list.substr(at, end - at));
      at = end + 1;
    }
    if (words.empty() || cl::archive_paths(files).empty()) return false;
    bench_ask = words[attract_next++ % words.size()];
    play_ask = true;
    attract_asked = true;
    return true;
  };
  const auto end_bench = [&](const std::string& why, double now_s, bool by_player = false) {
    attract = false;
    const std::string setup = std::to_string(w) + "x" + std::to_string(h) +
                              ", drawn offscreen, shown at 30 Hz, r_look " + picture.look + ", " +
                              device->report.name + " on " + device->report.backend;
    // A run stopped before a frame was timed has no report, only its reason; playback times none.
    if (bench.paced) {
      console.print("playdemo: " + bench.name + (why.empty() ? " ended" : " stopped: " + why));
      if (!by_player && !why.empty()) bench_failed = 1;
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
    configure_surface();
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
  // The installs found where the game was started or placed (client/install_search.hpp): `games`
  // lists them, `games <n>` loads one as `basedir` does.
  console.add_command("games",
                      "games: the classic games found where this game was started or placed; "
                      "games <number>: load one of them",
                      [&](Con& c, const CArgs& a) {
                        if (a.size() >= 2) {
                          const int n = std::atoi(a[1].c_str());
                          if (n >= 1 && n <= static_cast<int>(installs.size())) {
                            folder_ask = installs[static_cast<size_t>(n) - 1].basedir;
                            return;
                          }
                          c.print("There is no game " + a[1] + " in the list.");
                        }
                        if (installs.empty())
                          c.print(
                              "No classic games were found beside this game. Put it in your "
                              "game's folder, or in the folder above it.");
                        for (size_t i = 0; i < installs.size(); ++i)
                          c.print(cl::install_line(installs[i], static_cast<int>(i) + 1));
                      });
  const auto open_folder = [&](const std::string& basedir) {
    // Whichever game the install is: its rows and its adapter from here on.
    std::string root;
    const ge::classicenhance::Profile* content = cl::install_of(basedir, &root, &error);
    if (content == nullptr) return false;
    cl::SearchPath fresh;
    if (!cl::open_game_dir(basedir, cl::game_of(basedir, *content), fresh, error, *content))
      return false;
    cl::cancel_load(loader);  // its threads read the old packs
    if (first_pictures.valid()) first_pictures.wait();
    first_pictures = {};
    world.reset();
    world_current = false;
    cl::listen_stop(listen);
    files = std::move(fresh);
    menu_sounds.forget();  // the new game's own clicks
    args.basedir = basedir;
    args.profile = std::string(content->id);
    args.game = cl::game_of(basedir, *content);
    places.locs = {};
    places.map.clear();
    places.game_dir = std::filesystem::path(basedir) / args.game;
    adapter = cl::adapter_of(game_row(args.profile));
    game_menu.demos().set_extensions(cl::listed_extensions(adapter.game->demo_extensions));
    menu_host.table = &adapter.menu(adapter);
    game_menu.set_table(*menu_host.table);
    console.set_search_path(cl::config_dirs(args.basedir, args.game, *content));
    if (!cl::load_programs(adapter, files, args.progs, game_content, error, *adapter.game))
      return false;
    listen.opt.registered = game_content.registered;
    listen.opt.episodes = game_content.episodes;
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
    cl::remember_game(prefs, basedir, *content);
    cl::remember_install(prefs, content->id, basedir);
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
    const bool at_eye = in_hand.hand.at_eye;  // the loaded set's, not the level's
    in_hand = {};
    in_hand.hand.at_eye = at_eye;
    held_fire.shots = {};
    user_info.sent_to = nullptr;
  };
  // The window's own game heard this frame (`dt` s), at the player's volume.
  const auto hear_here = [&](double dt) {
    synth.bus_gain(0, static_cast<float>(win.volume) / cl::kDemoVolume);
    heard_here.frame(listen, files, static_cast<float>(dt));
  };
  // THE BOT CLIENT (`--bots=N --server=<host:port> --team=<name>`): a team of bots joins the
  // server (net/classic_bot_team.hpp) and the window shows the team's world as its bots believe it
  // (net/classic_team_view.hpp). The console drives them: a line no command names goes to every
  // bot, `bot<N> <line>` to one (`bot1 skill expert`, `ready`, `status`).
  if (args.bots > 0) {
    sn::BotTeamConfig bc;
    bc.server = args.server;
    bc.bots = args.bots;
    bc.team = args.team;
    bc.colour = args.colour;
    bc.basedir = args.basedir;
    bc.out = args.bots_out;
    bc.paths = args.bots_paths;
    bc.names = shooters_edge::bots::bot_handles(bc.seed + 1000u, bc.bots);  // people's handles
    if (!args.bot_features.empty()) bc.features = {args.bot_features};
    if (!args.skills.empty()) {
      bc.skills.clear();
      for (size_t at = 0; at <= args.skills.size();) {
        const size_t end = std::min(args.skills.find(',', at), args.skills.size());
        if (end > at) bc.skills.push_back(args.skills.substr(at, end - at));
        at = end + 1;
      }
    }
    sn::bot_team_locs_from(bc, "ezquake");
    if (!bc.out.empty()) {
      std::error_code ec;
      std::filesystem::create_directories(bc.out, ec);
    }
    sn::BotTeam bot_team;
    if (!sn::bot_team_start(bot_team, bc, error)) {
      out::error("shooters_edge: the bot client could not start its team (--bots=" +
                 std::to_string(args.bots) + " --server=" + args.server + "): " + error +
                 "; next: check the server answers (shooters_edge_bots --server=" + args.server +
                 " --bots=1), then try again");
      return 1;
    }
    console.set_unknown([&bot_team](const CArgs& a) {
      sn::bot_team_command(bot_team, a.raw);
      return true;
    });
    ge::platform::set_relative_mouse(*window, false);
    // The map drawn from above (client/bot_view.hpp) once the team has loaded it; the flat
    // picture (net/classic_team_view.hpp) until then, and for the recording.
    sn::TeamView bot_flat;
    sn::TeamAreas bot_areas;
    cl::BotCam bot_cam;
    cl::WorldDrawPtr bot_world;
    std::string bot_world_map;
    ge::ui::Image bot_image;
    sn::TeamViewVideo bot_video;
    if (!args.bots_view.empty() &&
        !sn::team_view_video_open(bot_video, args.bots_view, w & ~1, h & ~1, 10, error)) {
      out::error("shooters_edge: the team view's video " + args.bots_view + " did not open: " +
                 error + "; next: pass --bots-view=<a .mp4 in a folder that exists>");
      return 1;
    }
    const uint64_t bots_from = ge::platform::now_ns();
    uint64_t bots_last = bots_from;
    int64_t next_tick = 0;
    double next_picture = 0;
    bool shot_asked = false;
    while (!quit) {
      queue.clear();
      ge::platform::poll_input(*window, input, *devices, queue);
      if (input.quit) break;
      const uint64_t now = ge::platform::now_ns();
      const double dt = static_cast<double>(now - bots_last) * 1e-9;
      const double t = static_cast<double>(now - bots_from) * 1e-9;
      bots_last = now;
      if (input.resized) {
        w = input.width_px;
        h = input.height_px;
        surface.configure(*device, static_cast<uint32_t>(w), static_cast<uint32_t>(h), true);
        cl::resize_world(*device, *title, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
        if (bot_world)
          cl::resize_world(*device, *bot_world, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
      }
      ge::input::update(held, queue);
      ge::ui::feed_console(console, con_view, queue, con_keys);
      console.run();
      if (con_view.open() != text_on) {
        text_on = con_view.open();
        ge::platform::set_text_input(*window, text_on);
      }
      // The bots' ticks on the wall clock, 13 ms each; a stall drops ticks, never floods them.
      const auto now_us = static_cast<int64_t>((now - bots_from) / 1000);
      for (int k = 0; next_tick <= now_us && k < 8; k++, next_tick += 13000)
        sn::bot_team_tick(bot_team, next_tick);
      if (now_us - next_tick > 200000) next_tick = now_us;
      for (; bot_team.log_read < bot_team.log.size(); bot_team.log_read++)
        console.print(bot_team.log[bot_team.log_read]);
      sn::team_areas_update(bot_areas, bot_team);
      // The map's world, made once the team is on it (again on a new map).
      if (bot_team.loaded && bot_world_map != bot_team.map && !bot_team.seats.empty()) {
        bot_world = cl::create_world_draw(
            *device, files, *bot_team.loaded, static_cast<uint32_t>(w), static_cast<uint32_t>(h),
            surface.format(), picture, {}, error, {bot_team.seats[0]->c.models, {}});
        if (!bot_world)
          out::error("shooters_edge: the bots' map " + bot_team.map + " did not draw: " + error +
                     "; next: the flat team view stands in; check the packs under -basedir");
        bot_world_map = bot_team.map;
        bot_cam.fitted = false;
      }
      if (bot_video.open && t >= next_picture) {
        next_picture += 0.1;
        if (!sn::team_view_video_frame(bot_video, sn::team_view(bot_flat, bot_team, w & ~1, h & ~1),
                                       error))
          out::error(error);
      }
      overlay.begin(w, h);
      ge::render::FrameView view = cl::title_view(t);
      if (bot_world) {
        cl::bot_cam_input(bot_cam, bot_team, input, !con_view.open(), dt, h, px_per_unit);
        view = cl::bot_cam_view(bot_cam, bot_team, w, h, t);
        cl::draw_bot_view(overlay, view, w, h, bot_team, bot_areas);
      } else {
        ge::lab::Picture pic = sn::team_view(bot_flat, bot_team, w, h);
        bot_image.width = pic.width;
        bot_image.height = pic.height;
        bot_image.rgba = std::move(pic.rgba);
        bot_image.revision++;
        ge::ui::Box b;
        b.rect = {0, 0, static_cast<float>(w), static_cast<float>(h)};
        b.image = &bot_image;
        b.uv = {0, 0, static_cast<float>(bot_image.width), static_cast<float>(bot_image.height)};
        b.feather = 0;
        overlay.box(b);
      }
      con_view.frame(dt);
      shooters_edge::menu::draw_game_console(overlay, con_view, con_look, t);
      const wgpu::TextureView target = acquire_frame();
      if (!target) continue;
      cl::draw_world(*device, bot_world ? *bot_world : *title, view, {}, nullptr, dt, overlay,
                     target);
      shot_copy.frame(*device, surface.frame_target(), shots, console);
      present_frame();
      // `--seconds=N`: a screenshot a second before the end, then quit (a run that leaves a
      // picture behind).
      if (args.seconds > 0 && t >= args.seconds - 1 && !shot_asked) {
        shot_asked = true;
        console.execute("screenshot");
      }
      if (args.seconds > 0 && t >= args.seconds) break;
    }
    if (!sn::team_view_video_close(bot_video, error)) out::error(error);
    if (!sn::bot_team_stop(bot_team, error)) out::error(error);
    return 0;
  }
  size_t printed = 0, centred = 0;
  uint64_t last = first_ns;
  double game_t = 0;  // the game's clock, s: the sum of the frames' game times
  long presented = 0;
  double longest = 0;  // the longest frame, s: a hitch a load or a stall made
  while ((args.frames == 0 || presented < args.frames) && !quit) {
    if (std::exchange(attract_at_start, false) && !start_attract())
      next_map = game_content.first_map;
    if (cl::arrived(first_pictures, measured))
      pictures_enhanced = use_pictures(first_pictures.get(), true);
    if (!first_pictures.valid() && picture.ce.pictures != pictures_enhanced)
      pictures_enhanced = load_pictures(false);
    // The frame's pacing BEFORE its input is read, so the input is as fresh as the waits allow:
    // the GPU's queue down to vid_framelatency, then the cap (a timedemo runs unpaced).
    if (vsync) {
      if (pacing.settings.vsync != vsync_applied || pacing.settings.latency != latency_applied)
        configure_surface();
      const double gpu_s = gate.wait(*device, pacing.settings.latency);
      ge::platform::pace(*window, pacing, gpu_s);
    }
    queue.clear();
    ge::platform::poll_input(*window, input, *devices, queue);
    const uint64_t now = ge::platform::now_ns();
    const double frame_s = static_cast<double>(now - last) * 1e-9;
    // The game's time: at vsync whole refreshes (vid_steady), so the motion is even whenever the
    // frame started; a timedemo's as measured.
    const double wall = vsync ? ge::platform::game_dt(pacing, frame_s) : frame_s;
    game_t += wall;
    const double t = game_t;
    last = now;
    if (presented > 0) longest = std::max(longest, frame_s);
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
    // What the console and the menu ask for this frame, against what was asked before it: a
    // played demo paused behind the menu gives the window up to it (below).
    const std::string map_before = next_map;
    const bool joined_before = remote.active;
    ge::ui::feed_console(console, con_view, queue, con_keys);
    if (con_view.open() != text_on) {
      text_on = con_view.open();
      ge::platform::set_text_input(*window, text_on);
    }
    // The main menu: Escape opens it and goes back out of it; while it is up it has the keys and
    // the pointer. Over a played demo it pauses it, the demo's frame the backdrop, and closing it
    // plays on (as it was: a demo paused by hand stays paused).
    const bool was_open = game_menu.open();
    const bool watched = bench.active && bench.paced;
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
      const bool opening = (!con_keys.held && !was_open && (!bench.active || watched) &&
                            input.tapped(Key::escape)) ||
                           (!first_pictures.valid() && std::exchange(menu_at_start, false));
      if (opening) {  // the slots' lines read again: a save may have been made since
        menu_host.saves.assign(shooters_edge::menu::kSaveSlots, {});
        for (int i = 0; i < shooters_edge::menu::kSaveSlots; i++)
          menu_host.saves[static_cast<size_t>(i)] = cl::save_label(files, "s" + std::to_string(i));
      }
      game_menu.update(mi, opening, menu_host, wall);
      if (watched && !attract && !was_open &&
          game_menu.open()) {  // (Escape or `togglemenu`): held behind it
        paused_before_menu = playback.paused;
        playback.paused = true;
      }
      cl::keys_to_menu(game_menu, menu_host, queue, held);  // CUSTOMIZE CONTROLS' key
      // The click the presses made, from the game's own recordings, at the player's volume.
      synth.bus_gain(0, static_cast<float>(win.volume) / cl::kDemoVolume);
      menu_sounds.play(game_menu.take_cue(), adapter, files, cl::kDemoVolume);
      if (std::string demo = game_menu.take_demo(); !demo.empty()) {  // DEMOS: checked, played
        bench_ask = std::move(demo);
        play_ask = true;
      }
      if (was_open && !game_menu.open()) {
        if (watched && !attract) playback.paused = paused_before_menu;  // the demo plays on
        save_settings();   // what the menu changed is kept
        on_title = false;  // the title's menu closed: the map is played (once it has loaded)
        // A setting the world reads once (the enhanced textures and skins, the rebaked light, the
        // bloom) changed: the world made again behind the window, the old one drawn meanwhile.
        if (world && listen.server != nullptr && listen.map && cl::world_stale(*world))
          cl::load_world(loader, files, *listen.map, picture, listen.server->models, adapter, t);
      }
    }
    // Something else asked for while a played demo is up (a map, a save, a game folder, a server,
    // another demo): the demo stopped, so it is not held behind what comes next.
    if (watched && (next_map != map_before || !load_ask.empty() || !folder_ask.empty() ||
                    (remote.active && !joined_before) || !bench_ask.empty())) {
      bench.quit_after = false;  // `-playdemo`'s window stays for what was asked
      if (!end_bench("the player asked for something else", t, true)) return 1;
      game_menu.hide();
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
      if (vsync) {
        configure_surface();
      } else {
        surface.configure_unlocked(*device, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
      }
      if (world)
        cl::resize_world(*device, *world, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
      cl::resize_world(*device, *title, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    }

    // A game folder asked for: LOAD GAME's chooser answered, or the console's `basedir`.
    if (game_menu.take_load())
      ge::platform::pick_folder(*window, "Load game: the folder holding id1", args.basedir);
    // SELECT GAME: a game's install opened at once, or the folder chooser to find one.
    if (shooters_edge::menu::GameMenu::GameAsk ask = game_menu.take_game(); ask.asked) {
      if (ask.browse)
        ge::platform::pick_folder(*window, "Select game: the folder your game is installed in",
                                  args.basedir);
      else
        folder_ask = ask.install;
    }
    if (std::string picked, why; ge::platform::folder_picked(*window, &picked, &why)) {
      if (!why.empty())
        game_menu.load_refused(why + ". You can also type \"basedir <folder>\" in the console.");
      folder_ask = picked;  // "" when the player cancelled
    }
    if (!folder_ask.empty() && !bench.active) {
      std::string basedir;
      if (cl::install_of(folder_ask, &basedir, &error) == nullptr) {
        game_menu.load_refused(error);
        console.print("load game: " + error);
      } else if (open_folder(basedir)) {
        // From the menu: its root in the game's own look, the game's recordings behind it.
        if (game_menu.open() && !measured) {
          game_menu.show(menu_host, "main");
          on_title = true;
          if (start_attract()) next_map.clear();
        }
      } else {
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

    // A map started on the window's own server: its place names read (the same map keeps them).
    if (!remote.active && listen.map) cl::map_started(places, adapter, files, listen.map->name);

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
      const bool for_attract = std::exchange(attract_asked, false);
      if (!cl::timedemo_start(files, bench_ask, bench, error, adapter)) {
        console.print((for_attract ? "menu_attract: " : "timedemo: ") + error);
        // None of the game's recordings plays: the title's night, the first map loading behind.
        if (for_attract && next_map.empty() && !listen.map) next_map = game_content.first_map;
        if (quit_after) {
          out::error("shooters_edge: the timedemo did not start: " + error +
                     "; see docs/shooters-edge.md § Benchmarking a demo");
          return 2;
        }
      } else {
        bench.quit_after = quit_after;
        attract = for_attract;
        bench.paced = play_ask;
        bench.hear = play_ask;  // its sounds and lines gathered for the draw (demo_heard.hpp)
        bench.demo.track = playback.track;
        console.print((play_ask ? "playdemo: " : "timedemo: ") + bench.found);
        if (play_ask && !cl::playback_begin(playback, bench, error))
          console.print("playdemo: no highlights: " + error);
        if (!attract) con_view.set_open(false);
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
               : "voice: no microphone (there is none, or the system refused permission). Allow "
                 "the microphone for Shooter's Edge and restart the game.",
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
      remote.speedjump_view = user_info.speedjump_view;  // cl_speedjump_view, live
      cl::Moves moves = hands ? cl::button_moves(controls) : cl::Moves{};
      if (remote.active) {  // the weapon groups and the quick weapon off what the server last
                            // said (weapon_groups.hpp, quick_fire.hpp)
        const se::Status said = cl::demo_status(remote.demo.state, t);
        controls.groups.game = adapter.game->weapon_groups;
        cl::group_moves(controls.groups, said, t, moves);
        cl::quick_moves(controls.quick, said, t, moves);
      }
      sn::remote_hands(remote, moves, played, controls, wall);
      std::vector<std::string> lines;
      joined = sn::remote_step(remote, wall, lines);
      for (const std::string& line : lines) tell(line, t);
      if (!remote.active && bench_map && !back_to_listen()) return 1;
    }
    const bool drawn_remote = remote.active && (bench_map || joined == decltype(joined)::new_level);
    if (bench.active && !bench.paced && input.tapped(ge::platform::Key::escape)) {
      if (!end_bench("Escape pressed", t, true)) return 1;
      continue;
    }
    if (bench.active || drawn_remote) {
      std::vector<std::string> said;
      const bool paced = bench.paced || drawn_remote;  // every frame shown, nothing timed
      bench.lerp.strides = picture.lerp_move;          // r_lerpmove: a monster's strides glide
      const cl::TimedemoStep step =
          drawn_remote ? (joined == decltype(joined)::new_level ? cl::TimedemoStep::new_level
                                                                : cl::TimedemoStep::frame)
          : bench.paced
              // The menu or the console up: the play bar and its keys left alone.
              ? (con_view.open() || game_menu.open()
                     ? cl::playback_step(playback, bench,
                                         {{}, playback.pointer_x, playback.pointer_y, false}, w, h,
                                         wall, t, said, error)
                     : cl::playback_step(
                           playback, bench,
                           {input.pressed, input.pointer_x * px_per_unit,
                            input.pointer_y * px_per_unit, (input.pointer_buttons & 1u) != 0},
                           w, h, wall, t, said, error))
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
        const bool next = attract && step != cl::TimedemoStep::error;  // the next one, in turn
        if (!end_bench(step == cl::TimedemoStep::error ? error : "", t, attract)) return 1;
        if (next && !quit) start_attract();
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
      se::Status hud_status = cl::demo_status(ds, t);
      if (drawn_remote) cl::group_status(controls.groups, hud_status, t);  // the weapon group
      se::fill_hud(hud, hud_status, cl::hud_art_of(files, classic_art, free_art));
      const auto placed =
          hud.layout.draw(overlay, hud.layout.frame(w, h, se::hud_style()), hud_look());
      playback.bar_bottom_px = cl::hud_inset(placed, h);
      if (bench.paced && !game_menu.open()) cl::playback_draw(overlay, playback, bench, w, h, t);
      ge::hud::draw_frame_rate(overlay, ge::hud::frame_for(w, h, se::hud_style()), se::hud_style(),
                               pacing.times, pacing.refresh, pacing.show);
      if (paced) {  // a demo watched or a game joined: the menu and the console over it
        game_menu.draw(overlay, w, h, t);
        con_view.frame(wall);
        shooters_edge::menu::draw_game_console(overlay, con_view, con_look, t);
      }
      // Every frame is drawn, but only one in kBenchShowNs is shown: the rest go to an offscreen
      // target of the surface's format. A window's present waits for the display however its
      // mode is set (macOS's compositor hands an image back once a refresh), so a frame rate
      // timed through it is the display's; drawn offscreen it is the machine's.
      const uint64_t frame_ns = ge::platform::now_ns();
      wgpu::TextureView target;
      const bool show = paced || frame_ns - bench_shown_ns >= kBenchShowNs;
      if (show) target = paced ? acquire_frame() : surface.acquire(*device);
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
      // The quick weapon's preview held in place of the game's weapon (quick_fire.hpp).
      in_hand.show = drawn_remote ? cl::quick_show(controls.quick, cl::demo_status(ds, t), t) : -1;
      auto hand = cl::step_demo_held(ds, in_hand, view_settings, weapons, view, t, wall);
      if (drawn_remote) cl::quick_hand(controls.quick, hand);
      hand.lift = se::status_lift(hud, h);  // the gun over the classic status bar
      cl::draw_world(*device, *world, view, bench_fx, &hand, wall, overlay, target);
      if (!paced && bench.skip == 0) cl::record_span(*world, bench.passes);
      if (bench_shown_ns == frame_ns) {
        shot_copy.frame(*device, surface.frame_target(), shots, console);
        if (paced) {
          present_frame();
        } else {
          surface.present();  // a timedemo's shown frame: unpaced, untimed
        }
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
    cl::send_user_info(listen, console, user_info);    // the name and colours, as they change
    listen.speedjump_view = user_info.speedjump_view;  // cl_speedjump_view, live
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
    cl::Moves moves = playing ? cl::button_moves(controls) : cl::Moves{};
    // The quick weapon (quick_fire.hpp): the fire keys as the weapon to ask for and the attack.
    if (listen.server != nullptr) {  // the weapon groups first (weapon_groups.hpp)
      const se::Status said = cl::status_of(listen, t);
      controls.groups.game = adapter.game->weapon_groups;
      cl::group_moves(controls.groups, said, t, moves);
      cl::quick_moves(controls.quick, said, t, moves);
    }
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
    shown.lerp = picture.lerp_move;  // r_lerpmove: the eye between the server's frames too
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
    // See-through liquids where the server allows them (its `watervis`).
    fx.see_through = listen.server == nullptr ||
                     shooters_edge::look::watervis_allowed(listen.server->serverinfo);
    view_settings.lerp = picture.lerp_models;  // r_lerpmodels: the held weapon's frames too
    // The quick weapon's preview held in place of the game's weapon (quick_fire.hpp).
    in_hand.show =
        listen.server != nullptr ? cl::quick_show(controls.quick, cl::status_of(listen, t), t) : -1;
    shooters_edge::view::HandFrame hand =
        cl::step_held(listen, shown, in_hand, view_settings, weapons, t, wall);
    if (listen.server != nullptr) cl::quick_hand(controls.quick, hand);
    if (hand.flashing)
      fx.lights.push_back(shooters_edge::effects::muzzle_light(hand.flash.position));
    // ce_muzzles in the modern look: each shot's flame, smoke and sparks from the barrel (off, or
    // the classic look: the weapon's own flash and its light alone).
    cl::fire_held(held_fire, hand, fx_state.on.muzzles, view.time_s, fx);
    overlay.begin(w, h);
    const shooters_edge::server::SequelServer* sequel =
        listen.rules ? shooters_edge::server::sequel_of(*listen.rules) : nullptr;
    if (sequel != nullptr && listen.campaign.end == cl::LevelEnd::none) {
      // The sequel's own bar, from the program its rules send.
      cl::run_sequel_bar(sequel_bar, sequel->world, 0, sequel->frame_number, w, h);
      cl::draw_sequel_bar(overlay, sequel_bar, cl::sequel_bar_scale(h));
    } else if (listen.campaign.end == cl::LevelEnd::none) {
      se::Status hud_status = cl::status_of(listen, t);
      cl::group_status(controls.groups, hud_status, t);  // the weapon group (weapon_groups.hpp)
      se::fill_hud(hud, hud_status, cl::hud_art_of(files, classic_art, free_art));
      const ge::hud::Frame hf = hud.layout.frame(w, h, se::hud_style());
      hud.layout.draw(overlay, hf, hud_look());
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
    ge::hud::draw_frame_rate(overlay, ge::hud::frame_for(w, h, se::hud_style()), se::hud_style(),
                             pacing.times, pacing.refresh, pacing.show);
    game_menu.draw(overlay, w, h, t);
    con_view.frame(wall);
    shooters_edge::menu::draw_game_console(overlay, con_view, con_look, t);
    if (cl::busy(loader))  // the world made again behind the one drawn
      ge::hud::draw_spinner(overlay, ge::hud::frame_for(w, h, se::hud_style()), se::hud_style(), t,
                            cl::load_caption(loader), t - loader.since_s);

    wgpu::TextureView target = acquire_frame();
    if (!target) continue;
    cl::draw_world(*device, *world, view, fx, &hand, wall, overlay, target);
    shot_copy.frame(*device, surface.frame_target(), shots, console);
    present_frame();
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
