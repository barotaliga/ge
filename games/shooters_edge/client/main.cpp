// SHOOTER'S EDGE'S WINDOW: the game played on one machine. The game folder's packs are the search
// path (files.hpp), the baseline mod is compiled from the sources built into the game and runs as
// a LISTEN SERVER in this process with the player as its client (listen.hpp), and each frame the
// window reads the keys and the raw mouse (controls.hpp), steps the server by the frames the wall
// time makes due, and draws the map from the player's eyes (engine/camera's first-person rig:
// the view height, the stairs smoothed, the landing's dip) with the game's effects (world_fx.hpp:
// rocket trails, explosions and their light, the screen's blend), the weapon in hand (held.hpp:
// switched, kicked and flashing as the mod fires it), the HUD (hud/layout.hpp) and the console.
//
//   se [-game <dir>] [-progs <file>] [-models <dir>] [-config f] [+set fs_basegame q1|q2]
//                 [-timedemo <demo>] [-playdemo <demo>] [-tv <node>] [--frames=N] [--size=WxH]
//                 [-hidden] (the window never shown: a run that draws and takes screenshots)
//                 [-demovideo <demo> <file> [--from=m:ss --to=m:ss | --highlight=N] ...]
//                 [-logfile <file>] [+<command> ...]
//
// `fs_basegame` names the game played and `fs_basedir_<id>` its install (client/installs.hpp),
// else the window asks for it (client/install_ask.hpp); `-game` a mod's folder over it. SELECT
// GAME, LOAD GAME, `import <folder>` or the settings load another in place, enhanced in the
// background (client/game_folder.hpp). `-progs` runs a mod's progs in place of the baseline mod,
// `-models` names the held weapons' folder (by default the game's `content/models/view`). The other
// words are console lines run at start (a `+` before each after the first): `map dm6` loads that
// map; with none, the title (client/title.hpp) with the menu up, `start` (else the arena) loading
// behind it. Every map loads behind the window, a spinner up (client/map_load.hpp).
//
// THE KEYS are binds (controls.hpp: W A S D, SPACE jumps, the left button fires, SHIFT walks, 1..8
// and the wheel the weapons, F12 a screenshot; OPTIONS ▸ CUSTOMIZE CONTROLS); TAB held shows the
// scores (client/scores.hpp; `scr_levelstats 0|1`); the mouse looks (taken on focus;
// `in_mouse_release` frees it); ESCAPE opens the main menu (menu/game_menu.hpp: drawn in the pack's
// own pictures when the packs carry them, the rows the game cannot do yet dimmed), and ESCAPE again
// goes back out of it; the key under ESCAPE drops the console, where `map <name>` changes the map,
// `sv <line>` is a line for the server's console (`sv timelimit 10`) and `save <name>` and `load
// <name>` keep and bring back a single-player game, in the player's own folder
// (client/installs.hpp's `own_files`) as the settings are in theirs (client/settings.hpp). Quake
// II's unit movie and the game's last picture play over the window (cinematic.hpp).
//
// THE LOBBY: its own screen over the title's night until F4 joins the warmup (lobby_console).
//
// THE BENCHMARK: `timedemo <demo>` (`-timedemo`, which quits when done) plays a recorded game one
// demo frame a drawn frame, most drawn offscreen, and prints the frame rates and the GPU's dearest
// passes (client/timedemo.hpp). `playdemo <demo>` plays one at its own pace (`cl_demospeed`,
// `demo_track <slot>` for a multiview's view; every format, `.gz` and the voice formats too),
// watched like a video (client/demo_playback.hpp); `-demovideo <demo> <file>` turns one into a
// video or a GIF with no window and quits (client/demo_video.hpp, client/video_cfg.hpp).
// ge:summary Shooter's Edge's window: the listen server, the keys and mouse, the world and the HUD
// ge:keywords app main window listen-server basedir game-folder plus-commands map console relative-
// ge:keywords mouse mouse-look hud first-person effects r_look quit shooters-edge benchmark title
// ge:keywords timedemo demo vsync-off fps playdemo demo_track cl_demospeed demovideo main-menu
// ge:keywords escape vid_fullscreen offscreen vid_display vid_displays lobby scores-key cinematic
// ge:keywords scoreboard scr_levelstats settings config-cfg topcolor bottomcolor gamma binds f12
// ge:skill platform input camera render ui shooter
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "capture/build_info.hpp"
#include "capture/screenshot.hpp"
#include "classic/content/downloads.hpp"
#include "console/cfg.hpp"
#include "console/clock_macros.hpp"
#include "console/lex.hpp"
#include "console/view.hpp"
#include "core/output.hpp"
#include "gpu/device.hpp"
#include "gpu/frame_gate.hpp"
#include "gpu/surface.hpp"
#include "gpu/target.hpp"
#include "hud/spinner.hpp"
#include "input/names.hpp"
#include "input/state.hpp"
#include "net/voice_codec.hpp"
#include "platform/audio.hpp"
#include "platform/devices.hpp"
#include "platform/menu_keys.hpp"
#include "platform/pacing.hpp"
#include "platform/reveal.hpp"
#include "platform/window.hpp"
#include "play/session_flags.hpp"
#include "play/window_output.hpp"
#include "render/gpu_debug.hpp"
#include "shooters_edge/client/args.hpp"
#include "shooters_edge/client/bot_client.hpp"
#include "shooters_edge/client/cinematic.hpp"
#include "shooters_edge/client/classic_pictures.hpp"
#include "shooters_edge/client/controls.hpp"
#include "shooters_edge/client/coop_mode.hpp"
#include "shooters_edge/client/corners.hpp"
#include "shooters_edge/client/crash_note.hpp"
#include "shooters_edge/client/cvar_layers.hpp"
#include "shooters_edge/client/demo_check.hpp"
#include "shooters_edge/client/demo_eyes.hpp"
#include "shooters_edge/client/demo_free_look.hpp"
#include "shooters_edge/client/demo_heard.hpp"
#include "shooters_edge/client/demo_pack.hpp"
#include "shooters_edge/client/demo_playback.hpp"
#include "shooters_edge/client/demo_video.hpp"
#include "shooters_edge/client/entity_debug.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/game_folder.hpp"
#include "shooters_edge/client/game_start.hpp"
#include "shooters_edge/client/gpu_debug.hpp"
#include "shooters_edge/client/held.hpp"
#include "shooters_edge/client/host_dedicated.hpp"
#include "shooters_edge/client/install_ask.hpp"
#include "shooters_edge/client/install_configs.hpp"
#include "shooters_edge/client/install_search.hpp"
#include "shooters_edge/client/installs.hpp"
#include "shooters_edge/client/listen_record.hpp"
#include "shooters_edge/client/listen_sound.hpp"
#include "shooters_edge/client/lobby_console.hpp"
#include "shooters_edge/client/manual.hpp"
#include "shooters_edge/client/map_load.hpp"
#include "shooters_edge/client/menu_sound.hpp"
#include "shooters_edge/client/own_sounds.hpp"
#include "shooters_edge/client/place_names.hpp"
#include "shooters_edge/client/place_seen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/q2_bar.hpp"
#include "shooters_edge/client/remix.hpp"
#include "shooters_edge/client/remix_bar.hpp"
#include "shooters_edge/client/remix_pictures.hpp"
#include "shooters_edge/client/render_words.hpp"
#include "shooters_edge/client/scores.hpp"
#include "shooters_edge/client/settings.hpp"
#include "shooters_edge/client/take_in.hpp"
#include "shooters_edge/client/team_chat.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/client/title.hpp"
#include "shooters_edge/client/trouble_icons.hpp"
#include "shooters_edge/client/tv_live.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/view_from.hpp"
#include "shooters_edge/client/window_cvars.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/hud/classic_art.hpp"
#include "shooters_edge/hud/layout.hpp"
#include "shooters_edge/hud/lobby_page.hpp"
#include "shooters_edge/livemap/watch.hpp"
#include "shooters_edge/menu/classic_console.hpp"
#include "shooters_edge/menu/game_menu.hpp"
#include "shooters_edge/menu/level_screens.hpp"
#include "shooters_edge/menu/lobby_screen.hpp"
#include "shooters_edge/net/game_commands.hpp"
#include "shooters_edge/net/qw_browse.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "shooters_edge/net/qw_remote.hpp"
#include "shooters_edge/net/window_words.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/q2_server.hpp"
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
namespace slm = shooters_edge::livemap;
namespace sp = shooters_edge::profiles;
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
    out::error("shooters_edge: the command line was refused: " + why + " Usage: " + cl::usage() +
               "; next: rerun with only those flags.");
    return 2;
  }
  if (args.help) return cl::print_help();  // the usage and every flag, nothing started
  // Every line on the terminal, and in `-logfile <file>` too when it is named.
  if (!ge::play::apply_session_flags(session, nullptr, log_error))
    out::error("shooters_edge: the log file was not opened, the terminal only: " + log_error +
               "; next: name a file in a folder you can write");
  // The console: its lines on the terminal and in its scrollback; the client's commands.
  ge::console::Console console;
  ge::console::ConsoleView con_view(console);
  console.set_sink([&con_view](std::string_view line) {
    con_view.add(ge::console::LineKind::output, line);
    if (!line.empty() && line.back() == '\n') line.remove_suffix(1);  // the print's own newline
    out::status("console: " + std::string(line));
  });
  std::string next_map;
  bool quit = false;
  cl::ControlSettings controls;
  shooters_edge::look::Picture picture;
  cl::Listen listen;
  using CArgs = ge::console::Args;
  using Con = ge::console::Console;
  cl::CoopMode coop_mode;  // sv_coop and its settings (client/coop_mode.hpp)
  // Each layer's cvars in its own (client/cvar_layers.hpp): no name merged unsaid (test held).
  cl::add_coop_cvars(console, coop_mode);
  cl::add_cvars(console, controls);  // the shooter's controls in their layer, then the shell's
  cl::default_binds(console);        // before the configs, which rebind what the player changed
  console.cvars().set_layer(cl::kLayerClassic);
  shooters_edge::look::add_cvars(console, picture);
  cl::ViewFrom view_from;  // r_viewfrom and viewfrom: a picture of one spot (client/view_from.hpp)
  ge::render::GpuDebug gpu_debug;  // gpustats and gpudebug (render/gpu_debug.hpp)
  cl::add_view_from(console, view_from);
  cl::RenderWords render_words;  // `sky` and `timerefresh` (client/render_words.hpp)
  cl::add_render_words(console, render_words, picture.sky_typed, [&] {
    return listen.server != nullptr && listen.map ? listen.map->name : std::string();
  });
  console.cvars().set_layer(cl::kLayerEngine);
  shooters_edge::view::ViewSettings view_settings;
  shooters_edge::view::add_view_cvars(console.cvars(), view_settings);
  ge::camera::CameraCvars camera_cvars;  // cl_bob, cl_rollangle, v_kick... at the classic look's
  cl::add_view_camera_cvars(console.cvars(), camera_cvars);
  controls.lens_fov = &camera_cvars.fov;  // m_autosens slows the mouse by the lens
  // `quit [reason]` and `recycle [reason]`: a match hosted here tells its players first (quit:
  // the server is gone; recycle: reconnect, as one that comes back), then the window closes.
  const auto farewell = [&](const CArgs& a, bool restart) {
    std::string reason;
    for (size_t i = 1; i < a.size(); i++) reason += (i > 1 ? " " : "") + a[i];
    cl::listen_farewell(listen, reason, restart);
    quit = true;
  };
  console.add_command("quit",
                      "quit [reason]: the game closed; a match hosted here tells its "
                      "players why",
                      [farewell](Con&, const CArgs& a) { farewell(a, false); });
  console.add_command("recycle",
                      "recycle [reason]: the game closed; a match hosted here tells "
                      "its players to reconnect",
                      [farewell](Con&, const CArgs& a) { farewell(a, true); });
  console.add_command("net_restart", "net_restart: a match hosted here opened again on sv_port",
                      [&](Con& c, const CArgs&) {
                        std::string said;
                        cl::listen_net_restart(listen, listen.opt.port, said);
                        if (!said.empty()) c.print(said.substr(0, said.size() - 1));
                      });
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
  const auto to_server = [&remote](const std::string& w) { return sn::remote_lobby(remote, w); };
  cl::add_lobby_commands(console, listen, to_server);  // a server joined takes the lobby's words
  // Chat and voice (console/talk.hpp): said to the server joined, else to the window's own
  // match's server; T, Y and U open the chat line, V, B and N talk.
  ge::console::Talk talk;
  ge::console::add_talk(console, con_view, talk);
  remote.talk = &talk;
  sn::add_server_commands(console, listen, remote);    // users, status, rcon, cmd, score, inven
  std::function<bool(const std::string&)> word_first;  // `pause` over a played demo (below)
  sn::add_window_words(console, con_view, listen, remote, next_map,
                       &word_first);  // map, sv, chat ...
  cl::DemoPlayback playback;  // a played demo's bar, controls and highlights (demo_playback.hpp)
  word_first = [&](const std::string& l) { return cl::playback_pause_word(playback, bench, l); };
  cl::add_playback_commands(console, playback, bench);
  slm::Watch live_map;  // a match from above on TV and in demos (demo_map, livemap/watch.hpp)
  slm::add_live_map_cvars(console.cvars(), live_map.settings);
  // The window's sound (client/listen_sound.hpp, client/demo_heard.hpp), at `volume`.
  ge::audio::MusicConsole music;  // the kept soundtrack (`cd`, `bgmvolume`): outlives the synth
  ge::audio::add_music_commands(console, music);  // before the game's (Quake II's ogg_volume)
  ge::audio::Synth synth;  // outlives the device that pulls it (opened with the window)
  se::Hud hud = se::make_hud();
  const auto tell = [&](const std::string& line, double at, bool chat = false) {  // and the HUD
    console.print(line);
    const ge::console::ConsoleView::NotifySettings ns = con_view.notify_settings();
    if (!line.starts_with('\n'))  // not a banner: the feeds (cl_chat_notify, scr_chathud)
      cl::tell_feeds(remote.ignore.chat, hud.notify, line, chat, se::hud_style().ink, at, &ns);
  };
  hud.chat = &remote.ignore.chat.feed;  // the chat lines on screen (scr_chathud)
  se::add_cvars(console, hud);
  shooters_edge::menu::GameMenu game_menu;
  console.cvars().set_layer(cl::kLayerShell);  // the menus and the attract loop (menu_attract)
  game_menu.add_cvars(console, args.recipes);  // -recipes: a recipe's commands too
  // The console in the loaded classic game's own letters and background (menu/classic_console.hpp).
  shooters_edge::menu::ClassicConsole con_look;
  console.cvars().set_layer(cl::kLayerEngine);  // the engine's again (screenshots, the console)
  shooters_edge::menu::BoardView board_view;
  // The HUD's pictures: a classic game's own once its packs are open (below), Shooter's Edge's
  // free set only for its own content (client/classic_pictures.hpp's hud_art_of).
  se::HudArt free_art = se::free_art();
  free_art.finish();
  se::HudArt classic_art;
  // The HUD's feed is the one notify area: the console's own lines would show each twice.
  con_view.set_host_notify(true);
  // The install (client/game_folder.hpp): fs_basegame's fs_basedir_<id>; multiplayer over `id1`.
  // Installs around the game (client/install_search.hpp) are listed for `games`, never played.
  const std::string prefs = ge::platform::prefs_dir("gameengine2", "shooters_edge");
  // The game CLI's account file (`account signin`): our client logs itself in by it on a server
  // whose master is the same (net/qw_remote.hpp § Logging in).
  if (!prefs.empty())
    sn::remote_login_from(
        remote, (std::filesystem::path(prefs) / "cli" / "account.txt").string(), [] {
          return static_cast<int64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                          std::chrono::system_clock::now().time_since_epoch())
                                          .count());
        });
  // A game pointed at is copied into the data folder (`+set homedir`) and played from there.
  const std::string placed = cl::placed_folder(ge::platform::app_dir());  // relative paths' start
  const std::string data_root = cl::data_root(placed, prefs, cl::take_home_dir(&args.plus));
  // The series config, the program's own over the player's (client/settings.hpp); in it the game
  const auto series = cl::series_settings_files(data_root, placed);  // played and its install
  cl::BaseGame base = cl::read_base_game(series, args.plus);
  base.from = placed;
  const std::string copy_first =  // copied once the window is up, with its bar (take_in.hpp)
      cl::take_in_basedir(&base, data_root, cl::played_live(args));
  const auto installs = cl::installs_near({placed});  // `games`
  cl::choose_folder(args, copy_first.empty() ? base : cl::BaseGame{},
                    cl::own_game_folder(data_root));  // Shooter's Edge's own: <data folder>/se
  const cl::BaseGame chosen = cl::bind_base_game(console.cvars(), &base);  // set again below
  // The player's settings (client/settings.hpp), then the install's configs in its own order, its
  // folders on `exec`'s path (install_configs.hpp).
  const auto content_row = [&]() -> const ge::classicenhance::Profile& {
    const ge::classicenhance::Profile* c = ge::classicenhance::find_profile(args.profile);
    return c != nullptr ? *c : ge::classicenhance::q1_profile();
  };
  console.set_search_path(cl::config_dirs(args.basedir, args.game, content_row()));
  // A session's own config (`-config <file>`): read and saved there, the player's left alone.
  if (!session.config.empty()) console.set_config_file(session.config);
  // The player's saves, recordings and F12's screenshots (engine/capture/screenshot.hpp): under the
  // game folder, or its install (installs.hpp); made before the + lines for `+screenshot`.
  const std::string own_files = base.own_files(args.profile, args.basedir, args.game);
  ge::capture::ScreenshotSettings shot_settings;  // scr_format, scr_quality, scr_chroma
  ge::capture::Screenshots shots(own_files + "/screenshots", "shooters_edge", &shot_settings);
  ge::capture::add_screenshot_cvars(console.cvars(), shot_settings);
  ge::capture::add_screenshot_command(console, shots, "<game folder>/screenshots/");
  ge::platform::add_reveal_command(console);  // the screenshots page's SHOW IN FINDER
  game_menu.set_screenshots(shots.dir());     // OPTIONS > SCREENSHOTS looks through them
  game_menu.servers().set_query(shooters_edge::net::qw_query());         // JOIN A GAME
  game_menu.servers().set_routes(shooters_edge::net::qw_route_query());  // sb_findroutes
  game_menu.servers().set_cache_dir(
      std::filesystem::path(shots.dir()).parent_path());  // sb_listcache
  con_view.set_dump_folder(std::filesystem::path(shots.dir()).parent_path().string());  // condump
  // scr_conspeed in the loaded game's unit (client/game_start.hpp), known before the configs.
  cl::GameStart game_start;
  cl::LoadLine load_line;  // a classic game's map loading in the console (client/map_load.hpp)
  const auto& started_game = sp::game_or_quake(args.profile);
  cl::add_start_cvars(console, game_start, started_game);
  // The loaded game's own settings in its layer (client/cvar_layers.hpp), meaning a shared name.
  cl::GameCvarValues game_cvars;
  cl::SearchPath files;
  cl::Q2Screen& q2_screen = game_cvars.screen;  // Quake II's view and 2D (client/q2_view.hpp)
  cl::ScreenGraph screen_graph;  // timegraph in every other game (client/screen_2d.hpp)
  cl::UnderEar under_ear;        // the muffle under water (s_underwater, client/room_ear.hpp)
  cl::add_under_cvars(console, under_ear);
  const cl::GameBindings bindings{.files = &files,
                                  .crosshair = &hud.crosshair,
                                  .mouse = &controls.mouse,
                                  .crouch = &remote.q2_ahead.crouch,
                                  .show_misses = &remote.q2_ahead.show_misses,
                                  .shownet = &remote.q2_shownet,
                                  .showpackets = &remote.q2_showpackets,
                                  .noskins = &picture.stock_figures,
                                  .items_still = &picture.items_still,
                                  .model_shadows = &picture.model_shadows,
                                  .flash_blend = &picture.flash_blend,
                                  .view = &view_settings,
                                  .moves = &remote.q2_maxfps,
                                  .stuff_level = &remote.stuff.level,
                                  .frame_cap = &pacing.settings.max_fps,
                                  .light_intensity = &picture.light_intensity,
                                  .light_lift = &picture.light_lift,
                                  .no_glow = &picture.no_glow,
                                  .q2fx = &picture.q2fx,
                                  .chat = &remote.ignore.chat,
                                  .console_view = &con_view,
                                  .music = &music,
                                  .console = &con_look,
                                  .menu = &game_menu};
  cl::add_game_cvars(console, game_cvars, started_game, bindings);
  remote.stuff_file =
      cl::settings_dir(args.basedir, args.game).parent_path() / "stufftext-whitelist.txt";
  cl::add_save_global_command(console, series);  // the player's own config, not this copy's
  const auto save_settings = [&console, &series, &con_view] {
    cl::save_settings(console, series.empty() ? std::filesystem::path() : series.back());
    con_view.save_history();  // the typed lines, beside the config
  };
  const auto outside = [&](const std::function<void()>& run) {
    cl::without_game_cvars(console, game_cvars, started_game, bindings, run);
  };
  remote.q2_game = [&args] { return sp::game_or_quake(args.profile).id == "q2"; };  // +connect's
  const uint64_t first_ns = ge::platform::now_ns();
  ge::console::add_clock_macros(
      console, {.local = [] { return cl::local_time(); },
                .uptime_s =
                    [first_ns] {
                      return static_cast<double>(ge::platform::now_ns() - first_ns) * 1e-9;
                    }});  // $com_date, $com_time, $com_uptime, $random
  if (const cl::ConfigsRun ran = cl::start_configs(console, args.basedir, args.game, content_row(),
                                                   args.plus, {series, outside});
      ran.passed_over > 0)
    out::status("shooters_edge: " + std::to_string(ran.passed_over) +
                " lines of the install's configs name settings this game does not have; "
                "passed over");
  std::string base_seen = cl::set_base_game(console.cvars(), chosen);  // last opened (installs.hpp)
  console.cvars().apply_latched();

  // Quake II's status bar, drawn from its rules' own program (q2_bar.hpp).
  cl::Q2Bar q2_bar{.files = &files,
                   .binds = &console.binds().table(ge::console::BindMode::game),
                   .screen = &q2_screen,
                   .fx = &picture.q2fx,
                   .gamma = &picture.gamma,
                   .console = &console};
  cl::add_q2_bar_commands(console, q2_bar);  // dumpstatusbar, dumplayout, aashot, aadump
  // The HUD's type: the loaded classic game's glyph sheet writes its messages (hud/layout.hpp).
  const auto hud_look = [&] { return se::hud_style(cl::hud_art_of(files, classic_art, free_art)); };
  cl::ListenRecorder recorder;  // `record` and `stop` (listen_record.hpp)
  cl::add_record_commands(console, recorder, listen, files, playback.record, &remote.joined_record);
  remote.joined_record.screenshot = [&shots](const std::string& n, std::string& why) {
    return shots.request(n, &why);
  };
  remote.snap_grab = [&shots](int w, int h, int q) { return shots.grab(w, h, q); };  // snap
  remote.snap_take = [&shots](std::vector<uint8_t>& out) { return shots.take_grab(out); };
  std::string error;
  // No packs: the menu over the title; only `+map arena` starts the arena (the tests' fixture).
  const bool opened = cl::open_game_dir(args.basedir, args.game, files, error,
                                        sp::content_of(sp::game_or_quake(args.profile)));
  files.own = own_files;
  if (!opened) {
    if (!next_map.empty() && next_map != "arena") {
      out::error("shooters_edge: the game folder did not open: " + error +
                 "; see docs/shooters-edge.md § Playing it");
      return 2;
    }
    out::status("shooters_edge: no game yet (" + error + "); pick one on SELECT GAME");
    // The menu's clicks: the free recordings under the classic names (own_sounds.hpp).
#ifdef SE_CONTENT_DIR
    const std::string content = cl::find_content_dir(SE_CONTENT_DIR);
#else
    const std::string content = cl::find_content_dir("");
#endif
    cl::OwnSounds own;
    std::string why;
    if (content.empty() || !cl::mount_own_sounds(files, content, own, why))
      out::warn("shooters_edge: the menu is silent: " +
                (content.empty() ? std::string("no games/shooters_edge/content here") : why) +
                "; run from the tree's root");
  } else {
    out::status("shooters_edge: " + std::to_string(cl::archive_paths(files).size()) +
                " archives on the search path of " + files.folder);
  }
  // A joined server's files read from and kept in the window's game folders (net/window_words).
  bool locs_fetched = false;  // a map's place names fetched: read again when its level starts
  sn::wire_remote_files(remote, files, args.basedir, args.game, args.profile, locs_fetched);
  if (!args.demovideo.empty()) return cl::demo_video_main(console, args, files, picture, playback);
  // The window first, the title's night in it at once (client/title.hpp); the rest of the start
  // (the pictures, the programs) and the first map (client/map_load.hpp) come in behind it.
  auto window = ge::platform::open_window({.title = "Shooter's Edge",
                                           .width = args.width,
                                           .height = args.height,
                                           .hidden = args.hidden});
  if (!window) return 1;
  auto device = ge::gpu::create_device();
  if (!device) return 1;
  ge::gpu::Surface surface;
  if (!surface.create(*device, window->native())) return 1;
  ge::render::WindowShots shot_copy(gpu_debug.shot);  // screenshot's and gpudebug's frames
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
    GE_ZONE("frame.acquire");
    const uint64_t before = ge::platform::now_ns();
    wgpu::TextureView view = surface.acquire(*device);
    ge::platform::acquired(pacing, before, ge::platform::now_ns());
    return view;
  };
  const auto present_frame = [&] {
    GE_ZONE("frame.present");
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
  // The title with the spinner in its corner: the first frame, and a map's frames with no world.
  // The title's night with the menu over it; `lobby` puts the lobby's screen between them.
  const auto title_frame = [&](double t, double dt, std::string_view caption, double since,
                               bool lobby = false, bool no_night = false) {
    overlay.begin(w, h);
    cl::draw_title_backdrop(overlay, con_look, w, h, no_night);  // a classic game's own
    if (lobby) {
      const se::LobbyView lv = cl::listen_lobby_view(listen);
      shooters_edge::menu::draw_lobby_in(overlay, game_menu.classic(), game_menu.art(), lv, w, h,
                                         true);
    }
    game_menu.draw(overlay, w, h, t);
    con_view.frame(dt);
    cl::draw_console_shown(gpu_debug.shot.hiding(), overlay, con_view, con_look, t);
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
  // The main menu's maps, its classic pictures when the packs carry them, and enhance's packs.
  // The install's game's adapter, copied: LOAD GAME of another install swaps it for every reader.
  cl::GameAdapter adapter = cl::adapter_of(sp::game_or_quake(args.profile));
  remote.q2_game = [&adapter] { return adapter.game->id == "q2"; };
  shooters_edge::menu::Host menu_host{.console = &console,
                                      .maps = adapter.list_maps(adapter, files),
                                      .files = &files.vfs,
                                      .sheet = &remote.sheet.sheet};  // SERVER SETTINGS
  cl::add_adapter_commands(console, adapter, files, args.basedir);
  cl::read_configs_from(console, adapter, files);  // `exec` finds a cfg in the game's packs too
  con_view.load_history();                         // the lines typed in the last run
  // The map's place names, read as it starts and made in the game (client/place_names.hpp).
  cl::PlaceNames places;
  places.game_dir = std::filesystem::path(args.basedir) / args.game;
  // Where the player is: the view watched, else the window's own (client/place_seen.hpp).
  cl::PlaceSeen place_seen;
  playback.places = &places;       // the demo's players' table names their places
  recorder.places = &places.locs;  // a recording keeps the place names it is played with
  cl::add_place_queries(console, places, listen, place_seen, adapter, files);
  if (args.recipes) cl::add_entity_debug_commands(console, place_seen);  // entlist, enthide
  ge::render::add_gpu_debug(console, gpu_debug, device.get(), [&] {
    return cl::gpu_debug_notes({&view_from, &listen, &place_seen, &picture.look, &w, &h});
  });
  gpu_debug.shot.build = ge::capture::build_info_line(ge::capture::build_info());
  gpu_debug.shot.to_clipboard = ge::platform::set_clipboard_png;
  // CHAT: the player's line to everyone or to the team, its codes and glyphs filled in first,
  // and the f_ aliases the game's events run (client/team_chat.hpp).
  cl::TeamChat chat;
  chat.places = &places;
  sn::wire_team_chat(chat, listen, remote);  // said to the server joined, else the match hosted
  controls.choice.deathmatch = [&] {
    return cl::deathmatch_now(remote.active ? &remote.demo.state : nullptr, listen);
  };
  cl::add_team_chat(console, chat);
  // DEMOS: the game's recordings anywhere on the machine, each checked before it plays.
  const auto demo_places = [&] {
    game_menu.demos().set_game_places(
        {{"GAME", cl::settings_dir(args.basedir, args.game).parent_path().string()}});
  };
  game_menu.demos().set_extensions(cl::listed_extensions(adapter.game->demo_extensions));
  demo_places();
  game_menu.demos().set_check([&adapter](const std::string& p, std::string& f, std::string& why) {
    return cl::check_demo_line(p, adapter, f, why);
  });
  menu_host.table = &game_menu.set_table(adapter.menu(adapter), &files.vfs);  // the game's pages
  menu_host.game_open = opened && !cl::archive_paths(files).empty();  // packs: a game to play
  // ce_pictures: the menu's and the status bar's pictures enhanced from the packs' cache; read
  // again when it flips and when an enhance run ends (client/classic_pictures.hpp). The first read
  // is on a thread, behind the title; the menu opens once they are in.
  cl::RemixFiles remix;  // classic remix's other game's files (client/remix.hpp), opened below
  // SELECT GAME: the games added here, the one remix plays with marked (client/installs.hpp).
  game_menu.games().set_lister([&] {
    return cl::game_rows(base, args.profile, game_menu.game(),
                         remix.state.on ? remix.state.other : "", args.game);
  });
  const auto use_pictures = [&](cl::ClassicPictures pics, bool say) {
    cl::bar_pictures(q2_bar, pics);  // read again; its words in cr_menu's letters
    // The loaded game's own, or cr_menu's game's (client/remix_pictures.hpp).
    game_menu.set_art(std::move(pics.menu));
    shooters_edge::menu::classic_console(game_menu.art(), con_look);
    classic_art = std::move(pics.hud);
    classic_art.finish();
    if (say) cl::say_hud_pictures(classic_art, files, pics.hud_why);
    return pics.enhanced;
  };
  const auto load_pictures = [&](bool say) {
    return use_pictures(cl::remixed_pictures(files, remix, picture.ce.pictures, adapter), say);
  };
  bool pictures_enhanced = picture.ce.pictures;
  std::future<cl::ClassicPictures> first_pictures =
      std::async(std::launch::async, [&files, &adapter, enhance = picture.ce.pictures] {
        return cl::load_classic_pictures(files, enhance, {}, &adapter);
      });
  // The programs, the campaign and the first map the folder holds (client/game_folder.hpp).
  cl::GameContent game_content;
  if (!cl::load_programs(adapter, files, args.progs, game_content, error, *adapter.game)) {
    out::error("shooters_edge: the mod did not load: " + error +
               "; see docs/shooters-edge.md § Running your own mod");
    return 2;
  }
  // No map: the title, a classic game's recordings (`menu_attract`) under the console, or the menu
  // where none play; a measured run (timedemo, --frames) loads its map; -tv and `+connect` join.
  const bool measured = !args.timedemo.empty() || !args.playdemo.empty() || args.frames != 0;
  bool on_title = next_map.empty() && !measured && args.tv.empty() && !remote.active;
  bool menu_at_start = on_title;
  bool attract_at_start = on_title && opened && !game_menu.attract().empty();
  if (next_map.empty() && !attract_at_start && !remote.active) next_map = game_content.first_map;
  cl::ListenOptions listen_opt = shooters_edge::net::listen_options();
  listen_opt.files = &files;
  listen_opt.remix = &remix;          // a campaign's own map changes read through it (cr_monsters)
  listen_opt.bots_background = true;  // no frame waits for the bots' nav (bots/bot_seats.hpp)
  if (const ge::console::Cvar* n = console.cvars().find("name"); n != nullptr && !n->value.empty())
    listen_opt.player = n->value;
  cl::take_console_settings(listen_opt, listen.opt);  // the lobby, sv_port, sv_hostname, sv_login
  if (!prefs.empty())
    listen_opt.accounts_dir = (std::filesystem::path(prefs) / "accounts").string();
  listen_opt.registered = game_content.registered;
  listen_opt.episodes = game_content.episodes;
  listen_opt.item_names = game_content.item_names;
  listen_opt.arsenal = game_content.arsenal;
  listen_opt.module = &game_content.module;  // a mod folder's qwprogs.qvm plays its matches
  listen_opt.native_library = &game_content.native_library;  // or its game library
  listen_opt.voice = remote.voip;  // cl_voip: the window's match relays its players' voices
  // The saves (the campaign's whole game: listen.hpp's `listen_save`), `<game>/saves/<name>.save`.
  std::string load_ask;  // a save the console asked to load (loaded where a map change is made)
  cl::campaign_commands(console, listen, files, game_content, next_map, load_ask);
  cl::add_window_dedicated(console, prefs, args.basedir, adapter.game->id);  // NEW GAME, DEDICATED
  // The install's enhancing, in the background unless ce_auto is off or a run is measured.
  if (std::string basedir, why; opened && cl::check_game_folder(args.basedir, &basedir, &why,
                                                                sp::content_of(*adapter.game))) {
    game_menu.set_game(basedir);
    game_menu.servers().set_masters(shooters_edge::net::qw_install_masters(basedir));
    // The game played is the one the next start opens (fs_basegame, fs_basedir_<id>).
    cl::keep_base_game(console.cvars(), base, sp::content_of(*adapter.game).id, basedir,
                       &base_seen);
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

  // The held weapons (held.hpp): a classic game's own, else ours (the folder given, or the tree's).
  std::vector<ge::viewmodel::HeldModel> weapons;
  std::vector<std::string> model_dirs;
  if (!args.models.empty()) model_dirs.push_back(args.models);
  model_dirs.emplace_back("games/shooters_edge/content/models/view");
#ifdef SE_MODELS_DIR
  model_dirs.emplace_back(SE_MODELS_DIR);
#endif
  const bool weapons_at_eye = cl::load_held_set(adapter, files, model_dirs, weapons);
  // Each weapon's muzzle fire (world_fx.hpp) at its gun's size; a classic game's, its table's.
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
  const auto load_listen_world = [&](double at) {
    cl::load_world(loader, files, *listen.map, picture, listen.server->models, adapter, at,
                   listen.server->vwep);
  };
  cl::RemixSettings remix_settings;  // opened again as cr_enabled or the played game changes
  cl::bind_remix(console.cvars(), &remix_settings);
  loader.remix = &remix;
  // Quake II's bar over the classic status while the rules wear its weapons (remix_bar.hpp).
  // Under it: the chat lines on screen (scr_chathud) alone of the HUD, in its letters (scr_font).
  se::HudArt chat_art;  // its letters, copied again when scr_font's picture changes
  const ge::ui::Image* chat_font = nullptr;
  const auto q2_chat_lines = [&](cl::Q2Bar& bar, const se::Status& status) {
    const ge::ui::Image* font = cl::q2_font_image(bar);
    if (font != chat_font) {
      chat_font = font;
      chat_art.glyphs = font != nullptr ? *font : ge::ui::Image{};
    }
    se::fill_hud(hud, status, chat_art);
    se::keep_chat_only(hud);
    const ge::hud::Frame f = hud.layout.frame(w, h, se::hud_style());
    const cl::ChatExtras& x = remote.ignore.chat;  // at Quake II's scr_chathud_x and _y
    const auto kept = se::place_chat_px(hud, f, x.hud_x, x.hud_y, cl::q2_bar_scale(bar, w, h));
    if (console.cvars().find("scr_chathud_x") == nullptr) se::restore_chat(hud, kept);  // remix
    hud.layout.draw(overlay, f, se::hud_style(chat_art)), se::restore_chat(hud, kept);
  };
  std::vector<cl::BehindMover> behind_scratch;  // cl_thirdperson's doors and platforms
  const auto behind_movers = [&](const std::vector<ge::shooter::BrushModel>* models) {
    return cl::behind_movers(world.get(), models, behind_scratch);
  };
  cl::WornHud worn_hud;
  worn_hud.bar.files = &remix.files;
  worn_hud.bar.screen = &q2_screen;
  cl::HeldState in_hand;
  in_hand.hand.at_eye = weapons_at_eye;
  cl::ServerRemix told;  // a server's or a demo's remix, not the player's own (remix.hpp)
  const auto remix_now = [&]() -> const auto& { return told.of(remix_settings); };
  // `cr_items` (remix.hpp's `worn_arsenal`): the other game's things worn, from the next map.
  // `reopened`: the files opened again forget what was worn, so it is put on again even when the
  // parts compare the same (a demo recorded without a remix: everything back to the game's own).
  const auto remix_wear = [&](bool reopened) {  // true: the remix is worn as its settings say
    remix.mod_rules = !game_content.module.empty() || !game_content.native_library.empty();
    const bool dressed = cl::remix_dress(remix, remix_now(), args.profile);
    listen.opt.remix_info = cl::remix_info_lines(remix);  // told to everyone who joins
    if (!dressed && !reopened) return true;
    cl::worn_hud_reset(worn_hud, remix);       // read again from the files now open
    pictures_enhanced = !picture.ce.pictures;  // the pictures read again (cr_menu)
    cl::wear_lines(listen.opt, remix, game_content.arsenal);
    listen.opt.files = remix.from_files() ? &remix.files : &files;
    in_hand = {};
    in_hand.hand.at_eye = cl::load_worn_held(adapter, remix, files, model_dirs, weapons);
    picture.all_figures = remix.figure != nullptr ? std::string(remix.figure->folder) : "";
    console.print(cl::worn_line(remix));
    return true;
  };
  std::pair<bool, bool> remix_open{false, true};  // cr_enabled and cr_expansions as opened
  std::string remix_for;
  std::string remix_off;                     // why the remix is off though cr_enabled is on
  cl::RemixParts remix_items{};              // remix.hpp's `remix_parts`
  const auto remix_sync = [&]() -> bool {    // false: the loader is busy, in step next frame
    remix.balance = remix_settings.balance;  // the next map read weighs by them
    const auto open = std::pair(remix_now().enabled, remix_now().expansions);
    const bool same_files = open == remix_open && remix_for == args.basedir;
    if (same_files && cl::remix_parts(remix_now()) == remix_items) return true;
    if (cl::busy(loader)) return false;  // its threads may read the other game's files: next frame
    remix_items = cl::remix_parts(remix_now());
    // A cr_ setting changed while the remix is off takes nothing: the console says why.
    if (same_files && !remix.state.on) return console.print(cl::remix_off_line(remix_off)), true;
    if (same_files) return remix_wear(false);
    remix_open = open;
    remix_for = args.basedir;
    if (const std::string said =
            cl::open_remix_for(remix, remix_now(), base, args.profile, args.basedir, args.game);
        !said.empty())
      console.print(said), remix_off = remix.state.on ? "" : said;
    return remix_wear(true);
  };
  // A campaign's own change of map (a level's exit, the start map's skill gates) takes the cr_
  // settings changed during the map before it reads the next one, as the console's `map` does.
  listen.opt.before_map = [&] { remix_sync(); };
  ge::platform::DevicesPtr devices = ge::platform::open_devices();
  if (!devices) return 1;
  const auto pull = [](float* out, int frames, void* user) {
    static_cast<ge::audio::Synth*>(user)->render(out, frames);
  };
  // The hearings before the device (closed first: its thread never reads what they free).
  // A demo's sounds, voices and lines; the window's own game's (cl_footsteps, s_ambient over both).
  cl::DemoHearing hearing(synth, &picture.ce.sounds, &picture.q2fx);
  cl::ListenHearing heard_here(synth, &picture.ce.sounds, &picture.q2fx);
  console.add_command("soundlist", "soundlist: the sounds read, each with its length and rate",
                      [&](Con& c, const CArgs&) {
                        const cl::DemoSounds* read =
                            bench.active || remote.active ? hearing.sounds() : heard_here.sounds();
                        if (read == nullptr) return c.print("No sounds loaded.");
                        for (const std::string& line : read->list_lines()) c.print(line);
                      });
  cl::Cinematic cine;                 // Quake II's movies (cinematic.hpp)
  cl::MenuSounds menu_sounds(synth);  // the main menu's clicks (menu_sound.hpp), play
  cl::add_local_sound_commands(console, menu_sounds, adapter, files, synth, music.player.bus());
  cl::hear_worlds_on(&synth, &under_ear);  // each world's room on it (ce_acoustics)
  const std::unique_ptr<ge::platform::AudioOut> speaker =
      ge::platform::open_audio({.rate = ge::audio::kRate, .pull = pull, .user = &synth});
  cl::add_sound_status_commands(console, speaker ? speaker->driver() : nullptr, synth);
  // The microphone, opened the first time a talk key is held on a server joined (the system asks
  // the player's leave then, never at start) and read every frame after (net/qw_remote.hpp).
  std::unique_ptr<ge::platform::AudioIn> mic;
  bool mic_tried = false;
  std::vector<float> mic_samples(static_cast<size_t>(sn::kMicMostSamples));
  // Who has the mouse (input/mouse_capture.hpp): the look, whenever the window has the focus and
  // nothing in `in_mouse_release` asks for the pointer. Taken below on the first frame.
  ge::input::MouseCapture mouse;
  bool focused = ge::platform::has_focus(*window);
  const auto heard = [&] {  // the player's volume, unless s_auto_focus silences the window
    return ge::platform::heard_volume(win, focused, ge::platform::minimised(*window));
  };
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
  std::unique_ptr<cl::LoadedMap> bench_map;  // the benchmark's level (the world draws it)
  cl::DemoFolder bench_folder;               // the demo's game folder (timedemo.hpp's demo_folder)
  size_t bench_models = 0;  // the model list's length the demo's world was made with
  cl::FxFrame bench_fx;     // the demo's entities, players and effects this frame
  cl::FxState bench_fx_state = cl::fx_state_for(&places);  // the demo's effects
  cl::DemoFx bench_demo_fx;        // its temporary entities this level (demo_fx.hpp)
  ge::gpu::Target bench_off;       // the benchmark's frames that are not shown
  uint64_t bench_shown_ns = 0;     // when the benchmark last showed a frame
  wgpu::Future bench_in_flight{};  // the frame before this one, waited before the next
  if (!args.timedemo.empty()) bench_ask = args.timedemo;
  if (!args.playdemo.empty() || (on_title && !playback.startup.empty())) {  // or cl_startupdemo
    bench_ask = args.playdemo.empty() ? playback.startup : args.playdemo;
    play_ask = true;
    attract_at_start = false;
  }
  cl::TvWatch tv_watch{args.tv};  // `-tv <node>`: a live broadcast watched (client/tv_live.hpp)
  const auto window_at = [&](double t) {  // the new map's first view: where the player stands
    cl::LoadWindow at{device.get(),
                      static_cast<uint32_t>(w),
                      static_cast<uint32_t>(h),
                      surface.format(),
                      &picture,
                      weapons,
                      &files,
                      &adapter,
                      &game_content,
                      {}};
    at.view = [&, t] {
      cl::ViewState first;
      return cl::frame_view(listen, first, 0, t, 0);
    };
    return at;
  };
  // The world drawn again for `map` and `load`'s models, through `from` (a demo's folder): `now`
  // before the next frame, else read on a thread and built a step a frame (`keep`: the old drawn).
  cl::MapLoad demo_load;
  const auto rebuild_world = [&](const cl::LoadedMap& map, const cl::ModelLoad& load,
                                 const cl::SearchPath& from, bool now, bool keep = false) {
    if (!keep) world.reset();
    cl::load_world(demo_load, from, map, picture, load.names, adapter, 0, load.vweps);
    cl::level_music(music, map.spec.entities);  // the level's own track
    return !now || cl::step_demo_world(demo_load, window_at(0), true, world);
  };
  // The benchmark over: its report, vsync back, the listen server's world again.
  int bench_failed = 0;
  std::function<bool()> back_to_listen;  // below
  // `by_player`: the player stopped a played demo (Escape on a timedemo, or something else asked
  // for from the menu or the console over a paused one): not a failure.
  // THE RECORDINGS BEHIND THE MENU (`menu_attract`): the title's, in turn, ESCAPE closing the menu
  // to watch one; anything else asked for (a map, a game joined, a demo picked) stops them.
  bool attract = false;        // the demo playing is one of them
  bool attract_asked = false;  // one was asked for and has not started yet
  size_t attract_next = 0;     // the next one's place in the list
  bool attract_waits = false;  // one ended under the menu: the next once the menu closes
  // The last frame stays until the world is the demo's level; the console waits for `demo_shown`.
  bool demo_world = false;
  bool demo_shown = false;
  // A demo's first frames come before it places the player: seen only kDemoSettle s into its level,
  // the console held over the screen between the title's demos until then (`demo_hold`).
  double demo_level_t = 0;
  bool demo_hold = false;
  constexpr double kDemoSettle = 0.5;
  const auto start_attract = [&] {
    const std::string list = cl::attract_list(game_menu.attract(), args.basedir, args.game,
                                              content_row(), adapter.game->attract);
    const std::vector<std::string> words = cl::attract_words(list, files, adapter);
    if (words.empty() || cl::archive_paths(files).empty() || remote.active) return false;
    bench_ask = words[attract_next++ % words.size()];
    play_ask = true;
    attract_asked = true;
    return true;
  };
  // `startdemos <names>` sets the list and starts it with nothing else up; `stopdemo` stops one.
  bool stop_demo = false;
  cl::PitchDrift drift;  // the campaign's pitch drift (controls.hpp)
  console.add_command("startdemos", "startdemos <demo>...: the recordings the title plays in turn",
                      [&](Con& c, const CArgs& a) {
                        c.cvars().set("menu_attract", a.rest(0), ge::console::Source::local);
                        attract_next = 0;
                        if (!listen.map && !remote.active && !bench.active && next_map.empty())
                          start_attract();
                      });
  console.add_command("stopdemo", "stopdemo: the recording playing stopped",
                      [&](Con&, const CArgs&) { stop_demo = true; });
  cl::add_path_command(console, files);
  const auto end_bench = [&](const std::string& why, double now_s, bool by_player = false) {
    attract = false;
    if (std::exchange(demo_hold, false) && !game_start.loading)  // the next demo holds it again
      con_view.set_full(false);
    cl::team_event(console, "f_demoend");
    cl::tv_watch_stop(tv_watch);
    const std::string setup = std::to_string(w) + "x" + std::to_string(h) +
                              ", drawn offscreen, shown at 30 Hz, r_look " + picture.look + ", " +
                              device->report.name + " on " + device->report.backend;
    if (cl::bench_end_report(
            console, bench, why, by_player, setup, playback.bench_dumps,
            cl::settings_dir(args.basedir, args.game).parent_path() / "timedemo.log",
            [&](const std::string& line) { tell(line, now_s); }))
      bench_failed = 1;
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
    return rebuild_world(*listen.map, {listen.server->models, {}, listen.server->vwep}, files,
                         true);
  };

  ge::input::Queue queue;
  ge::input::InputState held;
  const ge::input::InputState idle;
  ge::ui::ConsoleKeys con_keys;
  con_keys.paste = [] { return ge::platform::clipboard_text(); };  // Command-V in the console
  con_keys.copy = [](const std::string& text) { ge::platform::set_clipboard_text(text); };
  bool selecting_text = false;  // the mouse's left button went down on the console's text
  double hand_paused = 0;       // s the game was paused: the held weapon's clock stood still
  bool text_on = false;
  ge::platform::InputSnapshot input;
  cl::ViewState shown;
  shown.q2 = &q2_screen;        // Quake II's cl_adjustfov and cl_kickangles
  bool mouse_off_said = false;  // the system refused the mouse once: said, never again
  cl::DemoFreeLook free_look;   // SHIFT held over a demo (demo_free_look.hpp)
  cl::DemoEyes demo_eyes{.gun_kick = &remote.gunkick, .view_kick = &q2_screen.kick_angles};
  ge::ui::Vec2 last_pointer;
  cl::FxState fx_state = cl::fx_state_for(&places);  // loc_draw's markers among the effects
  cl::FxFrame fx;
  // The pack's palette for the classic effects' specks, read again when the game folder changes.
  // and the effects the game's files define (particles/*.fx, world_fx.hpp), read with it.
  std::vector<uint8_t> fx_palette;
  ge::particles::Book fx_book;
  bool fx_palette_read = false;
  const auto ready_fx = [&](cl::FxState& s) {  // read once a folder, and handed to `s`
    if (!fx_palette_read) {
      cl::read_fx_palette(files, fx_palette);
      fx_book = {};
      std::vector<std::string> faults;
      cl::load_effects(files, fx_book, faults);
      for (const std::string& f : faults) console.print("particles: " + f);
      fx_palette_read = true;
      s.palette.clear();
    }
    if (s.palette.empty()) s.palette = fx_palette;
    s.book = &fx_book;
  };
  // LOAD GAME's folder, `import` or the settings (client/game_folder.hpp): the install opened in
  // place of the one playing (the old world and servers let go first), its first map started where
  // the map changes, enhanced in the background, kept. False, nothing changed, when it does not
  // open. `pack_ask`: SELECT GAME's mission pack over it, or -game's over a copy still to come in
  // (copy_in, the copy into the data folder); `install_ask`: fs_basegame's, none set.
  std::string folder_ask = copy_first, pack_ask = copy_first.empty() ? "" : args.game;
  cl::TakeIn copy_in;
  cl::InstallAsk install_ask = cl::ask_install(*window, args, measured);
  cl::add_take_in_commands(console, &folder_ask, &args.basedir, &music, &args.game,
                           &game_cvars.q2.remap_tracks);
  cl::add_home_dir(console, data_root);
  if (!measured) cl::offer_packs(console, data_root);    // the packs here not yet taken in
  cl::add_games_command(console, installs, folder_ask);  // `games`: the installs found, `games <n>`
  // A GAME SWITCHED SHOWS ITS LOADING: a console line and a frame before each long step of
  // `open_folder`, so the window never looks hung; the new game's look once its pictures are in.
  bool switching = false;
  double switch_t = 0;
  const auto switch_step = [&](const std::string& line) {
    if (!switching) return;
    console.print(line);
    title_frame(switch_t, 0, "", switch_t);
  };
  const auto open_folder = [&](const std::string& basedir) {
    // The loading shown as it goes (switch_step): the menu gone, the console down over the screen.
    struct Shown {
      bool& on;
      ~Shown() { on = false; }
    } shown_while{switching};
    if (!measured) {
      game_menu.hide();
      switching = true;
      con_view.set_full(true);
      switch_step("Opening " + std::filesystem::path(basedir).filename().string() + "...");
    }
    std::string root;  // whichever game the install is: its rows and adapter from here on
    const ge::classicenhance::Profile* content = cl::install_of(basedir, &root, &error);
    if (content == nullptr) return false;
    cl::SearchPath fresh;
    const std::string game_folder = cl::folder_over(basedir, *content, &pack_ask);
    if (!cl::open_game_dir(basedir, game_folder, fresh, error, *content)) return false;
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
    args.game = game_folder;
    files.own = base.own_files(args.profile, basedir, args.game);  // saves, recordings, pictures
    game_menu.set_screenshots(shots.set_dir(files.own + "/screenshots"));
    con_view.set_dump_folder(files.own);  // condump's, as at the start
    places.locs = {};
    places.map.clear();
    places.game_dir = std::filesystem::path(basedir) / args.game;
    const sp::GameProfile& was_game = *adapter.game;
    adapter = cl::adapter_of(sp::game_or_quake(args.profile));
    save_settings();  // the game left keeps its own settings in its folder
    cl::start_switch_game(console, game_start, was_game, *adapter.game);
    cl::switch_game_cvars(console, game_cvars, *adapter.game, bindings);
    game_menu.demos().set_extensions(cl::listed_extensions(adapter.game->demo_extensions));
    menu_host.table = &game_menu.set_table(adapter.menu(adapter), &files.vfs);
    menu_host.game_open = true;
    console.set_search_path(cl::config_dirs(args.basedir, args.game, *content));
    cl::run_game_settings(console, args.basedir, args.game, *content);  // its own meanings
    switch_step("Loading the game's rules...");
    if (!cl::load_programs(adapter, files, args.progs, game_content, error, *adapter.game))
      return false;
    listen.opt.registered = game_content.registered;
    listen.opt.episodes = game_content.episodes;
    listen.opt.item_names = game_content.item_names;
    listen.opt.arsenal = game_content.arsenal;
    cl::campaign_commands(console, listen, files, game_content, next_map, load_ask);
    cl::add_adapter_commands(console, adapter, files, basedir);
    menu_host.maps = adapter.list_maps(adapter, files);
    switch_step("Loading the game's pictures...");
    pictures_enhanced = load_pictures(true);
    switch_step("Loading the weapons...");  // the console in the new game's look from here
    weapons.clear();
    in_hand = {};
    in_hand.hand.at_eye = cl::load_held_set(adapter, files, model_dirs, weapons);
    if (std::string why; !cl::load_held_fire(held_fire, &why, held_names(in_hand.hand.at_eye)))
      out::error("shooters_edge: " + why + "; next: fix the weapon's fire in its table");
    fx_palette_read = false;
    next_map = game_content.first_map;
    game_menu.set_game(basedir);
    game_menu.servers().set_masters(shooters_edge::net::qw_install_masters(basedir));
    demo_places();
    cl::keep_base_game(console.cvars(), base, content->id, basedir, &base_seen);
    save_settings();  // the player's settings go on in the new folder
    out::status("shooters_edge: loaded " + basedir + " (" + files.folder + ")");
    if (std::string why; game_menu.auto_enhance() && !game_menu.enhance_game(true, &why))
      console.print("classic enhance: " + why);
    return true;
  };
  // A new map's or a new server's: what was seen of the last one let go, the user info sent again.
  const auto forget_seen = [&] {
    shown = {};
    fx_state = cl::fx_state_for(&places);
    const bool at_eye = in_hand.hand.at_eye;  // the loaded set's, not the level's
    in_hand = {};
    in_hand.hand.at_eye = at_eye;
    held_fire.shots = {};
    user_info.sent_to = nullptr;
    remix.undress(), picture.all_figures.clear();  // the remix's worked out again for the new game
    remix_for.clear();
  };
  // The window's own game heard this frame (`dt` s), at the player's volume.
  const auto hear_here = [&](double dt) {
    synth.bus_gain(0, static_cast<float>(heard()) / cl::kDemoVolume);
    heard_here.frame(listen, remix.heard_from(files), static_cast<float>(dt), remix.heard,
                     remix.heard_theirs());
  };
  // THE BOT CLIENT (`--bots=N --server=<host:port> --team=<name>`): client/bot_client.hpp.
  if (args.bots > 0) {
    cl::BotClientRun bots{args,    *window,     *devices,  input,         queue,
                          held,    console,     con_view,  con_keys,      text_on,
                          *device, surface,     *title,    files,         picture,
                          overlay, con_look,    shot_copy, shots,         w,
                          h,       px_per_unit, quit,      acquire_frame, present_frame};
    return cl::run_bot_client(bots);
  }
  size_t printed = 0, centred = 0;
  uint64_t last = first_ns;
  double game_t = 0;  // the game's clock, s: the sum of the frames' game times
  long presented = 0;
  double longest = 0;        // the longest frame, s: a hitch a load or a stall made
  if (!args.manual.empty())  // every command and setting registered: the manual's source
    return cl::write_manual(console, game_cvars, bindings, game_start, *adapter.game, args.manual);
  int quit_frames = 0;  // frames drawn after `quit` for a screenshot asked before it (at most 3)
  while ((args.frames == 0 || presented < args.frames) &&
         (!quit || (shots.asked() && quit_frames++ < 3))) {
    ge::platform::profile_tick(console, pacing);  // `profile N`: before any zone opens
    cl::start_apply_speed(console, game_start);
    shooters_edge::look::apply_ce_enabled(picture, console.cvars());  // ce_enabled 0: none of it
    music.held = cl::cinematic_on(cine);  // a movie's own sound: `ogg play` refused meanwhile
    ge::audio::music_frame(music, synth, console);
    cl::hear_swap(synth, under_ear);                      // s_swapstereo
    cl::hear_ears(synth, under_ear);                      // s_hrtf
    picture.q2fx.plain_stereo = !cl::hrtf_on(under_ear);  // al_merge_looping 1 merges only then
    if (std::exchange(attract_at_start, false) && !remote.active && next_map.empty() &&  // +map
        !start_attract())
      next_map = game_content.first_map;
    else if (attract_asked && std::exchange(menu_at_start, false))  // as SELECT GAME starts one
      cl::start_begin(game_start, con_view, console, game_t,
                      cl::start_pack_lines(cl::archive_paths(files), args.basedir));
    if (cl::arrived(first_pictures, measured))
      pictures_enhanced = use_pictures(first_pictures.get(), true);
    if (!first_pictures.valid() && picture.ce.pictures != pictures_enhanced)
      pictures_enhanced = load_pictures(false);
    // The frame's pacing BEFORE its input is read, so the input is as fresh as the waits allow:
    // the GPU's queue down to vid_framelatency, then the cap (a timedemo runs unpaced).
    if (vsync) {
      GE_ZONE("frame.wait");  // the GPU's queue and the cap: time waited, not spent
      if (pacing.settings.vsync != vsync_applied || pacing.settings.latency != latency_applied)
        configure_surface();
      const double gpu_s = gate.wait(*device, pacing.settings.latency);
      ge::platform::pace(*window, pacing, gpu_s);
    }
    queue.clear();
    ge::platform::poll_input(*window, input, *devices, queue);
    if (ge::platform::alt_enter(input, console))  // not the menu's ENTER too
      input.pressed[static_cast<size_t>(ge::platform::Key::enter)] = false;
    const uint64_t now = ge::platform::now_ns();
    const double frame_s = static_cast<double>(now - last) * 1e-9;
    // The game's time: whole refreshes at vsync (vid_steady: even motion); a timedemo's measured.
    const double wall = vsync ? ge::platform::game_dt(pacing, frame_s) : frame_s;
    game_t += wall;
    const double t = game_t;
    last = now;
    if (presented > 0) longest = std::max(longest, frame_s);
    if (input.quit) {
      save_settings();
      break;
    }
    ge::platform::follow_window_cvars(*window, win, speaker.get(), &console);  // and s_device
    ge::input::update(held, queue);
    // What the console and menu ask this frame, against before (a demo held behind it, below).
    const std::string map_before = next_map;
    const std::string demo_before = bench_ask;  // a title demo asked for and not started yet
    const bool joined_before = remote.active;
    con_keys.menu = game_menu.open();  // a menu has the keys: only F1..F12 run their binds
    con_keys.taken = cl::lobby_taken_keys(listen);  // the lobby's keys press no pack bind
    ge::ui::feed_console(console, con_view, queue, con_keys);
    if (con_view.typing() != text_on) {
      text_on = con_view.typing();
      ge::platform::set_text_input(*window, text_on);
    }
    // The console's text selected with the mouse (Command-C copies it, ui/console_keys.hpp).
    ge::ui::select_with_pointer(
        con_view, con_look.on() ? con_look.style : ge::ui::ConsoleStyle{}, w, h,
        input.pointer_x * px_per_unit, input.pointer_y * px_per_unit,
        (input.pointer_buttons & 1u) != 0 && con_view.open() && !game_menu.open(), selecting_text);
    // The main menu: Escape opens it and goes back out of it; while it is up it has the keys and
    // the pointer. Over a played demo it pauses it, the demo's frame the backdrop, and closing it
    // plays on (as it was: a demo paused by hand stays paused).
    const bool was_open = game_menu.open();
    const bool watched = bench.active && bench.paced;
    {
      using ge::platform::Key;
      ge::ui::MenuInput mi;
      if (!con_keys.held && was_open) {
        // the browser's filters, the quit box's Y and N, the screenshots page's O (show in Finder)
        ge::platform::menu_keys(input, "efmnopy", mi);
        if (mouse.pointer()) {  // a held pointer is frozen: the menu then reads only the keys
          mi.pointer_x = input.pointer_x * px_per_unit;
          mi.pointer_y = input.pointer_y * px_per_unit;
          mi.pointer_moved = mi.pointer_x != last_pointer.x || mi.pointer_y != last_pointer.y;
          last_pointer = {mi.pointer_x, mi.pointer_y};
          mi.click = held.mouse.buttons.went_up(0);
          mi.pointer_held = held.mouse.buttons.down(0);  // the setup page's figure turned
        }
      }
      menu_host.server = listen.server ? &listen.server->cvars : nullptr;
      menu_host.in_level = (listen.server || remote.active) && !attract;  // ESCAPE's in-game page
      menu_host.screen_up = q2_bar.layout_up;  // ESCAPE puts the help computer away first
      menu_host.escape_held = ge::input::key_held(held, ge::input::key::escape);  // held: the menu
      q2_bar.f1 = ge::input::key_held(held, 58) ? (bench.active ? 2 : 1) : 0;     // F1: the layout
      picture.view_size = q2_screen.viewsize;  // Quake II's viewsize (100 for every other game)
      // A benchmark's Escape stops the run (below) and opens nothing.
      const bool opening = (!con_keys.held && !was_open && (!bench.active || watched) &&
                            input.tapped(Key::escape)) ||
                           (!first_pictures.valid() && std::exchange(menu_at_start, false));
      const bool load_menu = cl::keep_autosave(listen, files);  // Quake II's; a death's ask
      if (opening || load_menu)  // the slots' lines read again: a save may have been made since
        menu_host.saves = cl::save_slot_labels(
            files, shooters_edge::menu::slot_names(shooters_edge::menu::table_of(menu_host)));
      game_menu.update(mi, opening, menu_host, wall);
      if (load_menu) game_menu.show(menu_host, "load");
      if (watched && !attract && !was_open &&
          game_menu.open()) {  // (Escape or `togglemenu`): held behind it
        paused_before_menu = playback.paused;
        playback.paused = true;
      }
      cl::keys_to_menu(game_menu, menu_host, queue, held);  // CUSTOMIZE CONTROLS' key
      // The click the presses made, from the game's own recordings, at the player's volume.
      synth.bus_gain(0, static_cast<float>(heard()) / cl::kDemoVolume);
      menu_sounds.play(game_menu.take_cue(), adapter, files, cl::kDemoVolume);
      if (std::string demo = game_menu.take_demo(); !demo.empty()) {  // DEMOS: checked, played
        bench_ask = std::move(demo);
        play_ask = true;
      }
      if (was_open && !game_menu.open()) {
        if (watched && !attract) playback.paused = paused_before_menu;  // the demo plays on
        if (std::exchange(attract_waits, false) && next_map.empty() && !listen.map && !bench.active)
          start_attract();
        save_settings();   // what the menu changed is kept
        on_title = false;  // the title's menu closed: the map is played (once it has loaded)
        // A setting the world reads once (the enhanced textures and skins, the rebaked light, the
        // bloom) changed: the world made again behind the window, the old one drawn meanwhile.
        if (world && listen.server != nullptr && listen.map && cl::world_stale(*world))
          load_listen_world(t);
      }
    }
    // Something else asked for while a played demo is up (a map, a save, a game folder, a server,
    // another demo): the demo stops (a title demo about to start is not started).
    const cl::AskedElsewhere elsewhere_ask{next_map != map_before, !load_ask.empty(),
                                           !folder_ask.empty(), remote.active && !joined_before,
                                           bench_ask != demo_before};
    if (watched && cl::demo_gives_way(elsewhere_ask)) {
      bench.quit_after = false;  // `-playdemo`'s window stays for what was asked
      if (!end_bench("the player asked for something else", t, true)) return 1;
      game_menu.hide();
    }
    if (attract_asked && !bench.active && cl::demo_gives_way(elsewhere_ask)) {
      if (bench_ask == demo_before) bench_ask.clear();  // another demo asked for plays instead
      attract_asked = false;
      play_ask = !bench_ask.empty();
      if (std::exchange(demo_hold, false) && !game_start.loading) con_view.set_full(false);
    }
    // The mouse is the look's whenever the window has the focus: a click on the window or on the
    // app's icon in the Dock takes it, no click in the window needed. Only what `in_mouse_release`
    // names lets it go: by default the console, a menu (its pointer hovers and clicks) and a demo
    // watched at its pace (its play bar); without the menu's bit the keys drive the menu.
    if (input.focus_lost) focused = false;
    if (input.focus_gained) focused = true;
    if (input.focus_gained) cl::team_event(console, "f_focusgained");
    // SHIFT held over a demo playing at its pace takes the mouse for free look (demo_free_look).
    const bool shift_look = bench.active && bench.paced && !remote.active && !con_view.open() &&
                            !game_menu.open() &&
                            (ge::input::key_held(held, ge::input::key::left_shift) ||
                             ge::input::key_held(held, ge::input::key::right_shift));
    const ge::input::CaptureFacts facts{
        .focused = focused && cl::q2_takes_mouse(game_cvars.q2, win.fullscreen),
        .console = con_view.open(),
        .menu = game_menu.open(),
        .watching = bench.active && bench.paced && !shift_look};
    switch (ge::input::follow_capture(mouse, facts, controls.mouse_release)) {
      case ge::input::CaptureStep::take:
        ge::platform::set_relative_mouse(*window, true, controls.mouse.raw);
        if (!ge::platform::relative_mouse(*window) && !std::exchange(mouse_off_said, true))
          console.print("The mouse could not be taken, so it does not turn the view.");
        break;
      case ge::input::CaptureStep::let_go:
        ge::platform::set_relative_mouse(*window, false);
        break;
      case ge::input::CaptureStep::none:
        // Held with the keys, but the OS let relative mode go (or held it back): asked again.
        if (mouse.held && focused && !ge::platform::relative_mouse(*window))
          ge::platform::set_relative_mouse(*window, true, controls.mouse.raw);
        break;
    }
    // The lobby's keys every frame, a server's joined as the window's own: F1 ready or break, F2
    // the other team, F4 the warmup or the screen, there F5 and F6 a bot more or less, ENTER start.
    {
      using ge::platform::Key;
      const bool keys = !con_keys.held && !con_view.open() && !game_menu.open();
      const auto tap = [&](Key k) { return keys && input.tapped(k); };
      const cl::LobbyKeys k{tap(Key::f1), tap(Key::f2), tap(Key::f4),
                            tap(Key::f5), tap(Key::f6), tap(Key::enter)};
      cl::lobby_keys(listen, k, to_server);
    }
    const bool captured = mouse.held;  // a recipe (-recipes) plays with the pointer free too
    const bool playing = !(con_keys.held || (!captured && !args.recipes) || was_open);
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
    // A game folder asked for: LOAD GAME's chooser answered, or the console's `import`.
    if (game_menu.take_load())
      ge::platform::pick_folder(*window, "Load game: the folder holding id1", args.basedir);
    // SELECT GAME: a game's install opened at once, or the folder chooser to find one.
    if (shooters_edge::menu::GameMenu::GameAsk ask = game_menu.take_game(); ask.asked) {
      if (ask.browse) ge::platform::pick_folder(*window, "Add game: a game's folder", args.basedir);
      folder_ask = ask.install;  // "" when browsing
      pack_ask = ask.pack;
    }
    if (std::string picked, why; ge::platform::folder_picked(*window, &picked, &why)) {
      if (!why.empty())
        game_menu.load_refused(why + ". You can also type \"import <folder>\" in the console.");
      folder_ask = picked;  // "" when the player cancelled
      if (!cl::answer_install(*window, install_ask, &folder_ask, &why)) game_menu.load_refused(why);
    }
    cl::ask_base_game(base, &base_seen, &folder_ask, console);  // the settings set: installs.hpp
    // A game pointed at is copied in on its own thread, its bar on the console (take_in.hpp).
    if (!bench.active && copy_in.start(&folder_ask, data_root, game_menu.open())) game_menu.hide();
    if (copy_in.finished(console, &folder_ask, &error)) {
      const bool from_menu = copy_in.from_menu();  // open_folder hides it
      switch_t = t;
      std::string basedir;
      if (folder_ask.empty()) {
        game_menu.load_refused(error);
      } else if (cl::install_of(folder_ask, &basedir, &error) == nullptr) {
        game_menu.load_refused(error);
        console.print("load game: " + error);
      } else if (open_folder(basedir)) {
        // As the classic games start: the packs added in the console (game_start.hpp).
        if (!measured) {
          cl::start_begin(game_start, con_view, console, t,
                          cl::start_pack_lines(cl::archive_paths(files), basedir));
          if (from_menu) {
            on_title = true;
            if (start_attract()) next_map.clear();
          }
        }
      } else {
        con_view.set_full(false);
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
    // A run of the enhancing ended: the world drawn again from what it wrote (when it wrote any).
    if (game_menu.take_ended()) {
      const ge::classicenhance::BoardView b = game_menu.job()->view();
      console.print(b.finished ? "classic enhance: " + game_menu.game() + " enhanced"
                               : "classic enhance: stopped: " + b.error);
      const ge::classicenhance::RunReport report = game_menu.job()->report();
      if (std::any_of(report.stages.begin(), report.stages.end(),
                      [](const ge::classicenhance::StageResult& r) { return r.ran; })) {
        pictures_enhanced = load_pictures(false);
        if (world && listen.server != nullptr && listen.map && !bench.active) load_listen_world(t);
      }
    }
    // A map started on the window's own server: its place names read (the same map keeps them).
    if (!remote.active && !bench.active && listen.map) {
      place_seen.on = false;
      cl::map_started(places, adapter, files, listen.map->name);
    }
    // Co-op as the window's mode: `sv_coop` turned on, the friendly-fire settings, the episodes.
    cl::coop_frame(coop_mode, listen, game_content.campaign != nullptr, game_content.start_map,
                   next_map);
    // A map asked for (the console's `map`, or the mod's level change at its limits), read behind
    // the window (client/map_load.hpp): started on the server once read, its world made behind
    // the window, drawn once its pipelines are compiled. A measured run waits for each step.
    if (next_map.empty() && !listen.pending_map.empty() && !cl::busy(loader) &&
        !cl::cinematic_holds(cine))
      next_map = listen.pending_map;
    const bool elsewhere = bench.active || remote.active || bench_map;  // the world is theirs
    if (!elsewhere) told.on = false;   // the player's own cr_ settings again
    if (elsewhere) heard_here.stop();  // a demo or a server joined is heard instead
    const bool remix_ready =
        cl::remix_waits(listen.map != nullptr, !next_map.empty()) || remix_sync();
    if (!next_map.empty() && !loader.map.valid() && !elsewhere && remix_ready) {  // remix first
      cl::load_map(loader, files, next_map, t);
      next_map.clear();
    }
    if (!elsewhere) {
      bool fresh = false;
      const bool was_loading = cl::busy(loader);  // and failed, nothing up: the title again
      if (const int code = cl::take_load(loader, window_at(t), listen, console, measured, t, world,
                                         world_current, fresh))
        return code;
      if (was_loading && !cl::busy(loader) && !listen.map && !measured && !on_title) {
        on_title = true;
        game_menu.show(menu_host);
      }
      if (fresh) forget_seen();
      if (fresh && listen.map) cl::level_music(music, listen.map->spec.entities);
    }
    // A save asked for: the game it holds now playing (its world made behind the window, below).
    if (!load_ask.empty())
      console.print(cl::load_save_asked(listen, game_content, files, load_ask));
    if (attract_asked && remote.active) {  // a game joined first: no title demo over it
      bench_ask.clear();
      attract_asked = play_ask = false;
    }
    // A benchmark: vsync off, the listen server waits until it is done (a demo at pace keeps it).
    if (!bench_ask.empty()) {
      const bool quit_after = bench_ask == args.timedemo || bench_ask == args.playdemo;
      const bool for_attract = std::exchange(attract_asked, false);
      if (!cl::timedemo_start(files, bench_ask, bench, error, adapter)) {
        console.print((for_attract ? "menu_attract: " : "timedemo: ") + error);
        // None of the game's recordings plays: the title's night, the first map loading behind.
        if (for_attract && next_map.empty() && !listen.map) next_map = game_content.first_map;
        if (std::exchange(demo_hold, false) && !game_start.loading) con_view.set_full(false);
        if (quit_after) {
          out::error("shooters_edge: the timedemo did not start: " + error +
                     "; see docs/shooters-edge.md § Benchmarking a demo");
          return 2;
        }
      } else {
        bench.quit_after = quit_after;
        attract = for_attract;
        demo_world = false;  // the old world is not this demo's: the last frame stays
        demo_shown = false;
        bench.paced = play_ask;
        bench.hear = play_ask;  // its sounds and lines gathered for the draw (demo_heard.hpp)
        bench.demo.track = playback.track;
        console.print((play_ask ? "playdemo: " : "timedemo: ") + bench.found);
        cl::team_event(console, "f_demostart");
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
    // A game started: its console let go once its recording is seen or its map is in.
    cl::start_step(game_start, con_view, t,
                   (attract && bench.active && demo_shown) || listen.map != nullptr);
    // The microphone: opened the first time a voice key is held where voice is carried (a server
    // joined, or the window's own match), read every frame into it; the key names who hears it.
    const ge::console::TalkVoice talking = ge::console::talk_voice(talk);
    const bool talk_key = talking.on || (remote.voip_key != nullptr && remote.voip_key->down());
    const bool voice_here = !remote.active && cl::listen_voice(listen);
    if (!mic_tried && (sn::remote_talking(remote) || (voice_here && talk_key))) {
      mic_tried = true;
      mic = ge::platform::open_audio_in(ge::net::kVoiceRate);
      tell(mic ? "voice: the microphone is on while you hold a talk key"
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
      else if (voice_here) {
        if (talking.on)  // told every frame he talks: a new map seats him afresh
          cl::listen_command(listen,
                             "voicetarg " + std::string(ge::console::chat_to_word(talking.to)));
        cl::listen_mic(listen, heard_mic, talk_key);
      }
    }
    auto joined = sn::RemoteStep::waiting;
    if (remote.active || (bench_map && !bench.active)) {
      const bool hands = playing && !game_menu.open();
      remote.speedjump_view = user_info.speedjump_view;  // cl_speedjump_view, live
      cl::Moves moves = hands ? cl::button_moves(controls) : cl::Moves{};
      if (remote.active) {  // the weapon groups and the quick weapon off what the server last
                            // said (weapon_groups.hpp, quick_fire.hpp)
        const se::Status said = remote.q2 ? cl::q2_joined_status(remote.demo.state, t)
                                          : cl::demo_status(remote.demo.state, t);
        cl::weapon_moves(controls, adapter.game->weapon_groups, said, t, moves);
      }
      sn::remote_hands(remote, moves, played, controls, wall);
      std::vector<std::string> lines;
      joined = sn::remote_step(remote, wall, lines);
      for (const std::string& line : lines) tell(line, t);
      sn::remote_run_stuffed(remote, console);  // what the server stuffed, run in the console
      if (!remote.active && bench_map && !back_to_listen()) return 1;
    }
    sn::remote_queue_frame(remote, console);    // `ranked`: the game found is joined
    sn::remote_console_foot(remote, con_view);  // the file coming, its bar, the console held down
    copy_in.show(con_view);                     // the game being copied in, the same way
    if (std::exchange(remote.left, false) && !remote.active && !listen.map && !bench.active &&
        next_map.empty()) {
      if (!start_attract()) next_map = game_content.first_map;
      on_title = true;
      game_menu.show(menu_host);  // a server left with no game of our own behind: the title
    }
    const bool drawn_remote = remote.active && (bench_map || joined == decltype(joined)::new_level);
    cl::release_load_line(loader, load_line, con_view, game_start.loading || demo_hold,
                          remote.active || bench.active);
    const bool stop = std::exchange(stop_demo, false);  // `stopdemo` with none playing: nothing
    if (bench.active && !bench.paced && (stop || input.tapped(ge::platform::Key::escape))) {
      if (!end_bench(stop ? "stopdemo" : "Escape pressed", t, true)) return 1;
      continue;
    }
    if (std::string why; !cl::tv_watch_feed(tv_watch, bench, now / 1000, why) && !end_bench(why, t))
      return 1;  // a live broadcast's blocks onto the demo, played once the lead came (tv_live.hpp)
    if (bench.active || drawn_remote) {
      std::vector<std::string> said;
      const bool paced = bench.paced || drawn_remote;  // every frame shown, nothing timed
      bench.lerp.strides = picture.lerp_move;          // r_lerpmove: a monster's strides glide
      const bool keys_free = !con_view.open() && !game_menu.open();  // else the bar's left alone
      if (const int to = slm::watch_keys(live_map, input.pressed, bench.demo.state, tv_watch.on,
                                         keys_free && !drawn_remote);
          to >= 0)
        bench.demo.track = playback.track = to;  // the live map's Space (livemap/watch.hpp)
      playback.rate = drawn_remote ? 1 : slm::watch_fly_step(live_map, wall);  // the map's fly (M)
      const cl::TimedemoStep step =
          drawn_remote ? (joined == decltype(joined)::new_level ? cl::TimedemoStep::new_level
                                                                : cl::TimedemoStep::frame)
          : tv_watch.on
              ? cl::tv_live_hold(tv_watch.live, tv_watch.client.watch,
                                 cl::playdemo_step(bench, wall, playback.rate, error), bench)
          : bench.paced
              ? cl::playback_step(
                    playback, bench,
                    keys_free
                        ? cl::PlaybackInput{input.pressed, input.pointer_x * px_per_unit,
                                            input.pointer_y * px_per_unit,
                                            (input.pointer_buttons & 1u) != 0,
                                            cl::zoom_of(cl::button_moves(controls))}
                        : cl::PlaybackInput{{}, playback.pointer_x, playback.pointer_y, false},
                    w, h, wall, t, said, error)
              : cl::timedemo_step(bench, error);
      for (const std::string& line : said) console.print(line);
      if (std::exchange(playback.jumped, false)) con_view.clear_notify();  // as a jump does
      // A paced demo is drawn between its frames (demo_lerp.hpp), so it moves smoothly.
      const cl::DemoState& ds = drawn_remote ? remote.demo.state : cl::demo_drawn(bench);
      cl::demo_music(music, ds);     // the track its server names
      cl::see_from(place_seen, ds);  // where it stands, looks, and what it draws
      place_seen.map = bench_map ? &bench_map->bsp : nullptr;
      // The place names the recording kept (or the server stuffed) are the ones it played with:
      // they stand in for any file while its level lasts.
      if (!ds.places.points.empty() && (place_seen.kept_level != ds.levels ||
                                        place_seen.kept_count != ds.places.points.size())) {
        place_seen.kept_level = ds.levels;
        place_seen.kept_count = ds.places.points.size();
        places.locs = ds.places;
        places.locs.from = "the recording";
        places.map = ds.map;  // the level's start keeps them (map_started: the same map)
      }
      if (step == cl::TimedemoStep::new_level) {
        std::vector<std::string> lines;
        told.follow(ds.serverinfo), remix_sync();  // its server's remix: the level read from it
        const cl::SearchPath& from = remix.state.on ? remix.files
                                                    : cl::demo_folder(bench_folder, args.basedir,
                                                                      args.game, ds, files, lines);
        for (const std::string& line : lines)  // a server joined plays from what the window has
          if (!drawn_remote) console.print("playdemo: " + line);
        if (locs_fetched) places.map.clear();  // the same map, with the file it just fetched
        locs_fetched = false;
        cl::map_started(places, adapter, from, ds.map);  // its place names, from where it played
        bench_map = cl::load_named_map(from, ds.map, error);
        if (!bench_map) {
          if (!end_bench("the demo's map " + ds.map + ": " + error, t)) return 1;
          continue;
        }
        if (std::string v = cl::q2_map_version(ds.map_checksum, bench_map->file, drawn_remote);
            !v.empty()) {  // a server's other map drops the game; a recording's plays on
          console.print(v);
          if (drawn_remote) console.append("disconnect");
        }
        if (!rebuild_world(*bench_map, {ds.models, {}, cl::demo_vweps(ds)}, from, !paced)) return 1;
        demo_world = true;
        demo_level_t = t;
        bench_models = ds.models.size();
        bench_fx_state = cl::fx_state_for(&places);  // a new level: nothing burning
        bench_demo_fx = {};
        if (drawn_remote)
          sn::remote_world(remote, &bench_map->spec.models, bench_map->spec.brushes.get(),
                           &bench_map->bsp.map);
        if (paced) hearing.level(ds, from, &bench_map->bsp.map, remix.heard, remix.heard_theirs());
      } else if (step != cl::TimedemoStep::frame) {
        const bool next = attract && step != cl::TimedemoStep::error;  // the next one, in turn
        if (!end_bench(step == cl::TimedemoStep::error ? error : "", t, attract)) return 1;
        // The next one, unless the menu is up: then the loop waits for it to close.
        attract_waits = next && game_menu.open();
        if (next && !quit && !attract_waits && start_attract()) {
          // The next one loads under the console, as the classic games went from demo to demo.
          console.print("Loading " + bench_ask + "...");
          con_view.set_full(true);
          demo_hold = true;
        }
        continue;
      }
      // A client recording's model list may grow after its level started: the world made again
      // (and `r_reload`'s, read from the files again).
      if (bench_map &&
          (ds.models.size() != bench_models || std::exchange(render_words.reload_asked, false))) {
        const cl::SearchPath& from = bench_folder.path ? *bench_folder.path : files;
        if (!rebuild_world(*bench_map, {ds.models, {}, cl::demo_vweps(ds)}, from, !paced, true))
          return 1;
        bench_models = ds.models.size();
      }
      if (cl::busy(demo_load) && !cl::step_demo_world(demo_load, window_at(t), false, world))
        return 1;
      if (paced) {  // what the frames read carried: heard, and its lines shown
        synth.bus_gain(0, static_cast<float>(heard()) / cl::kDemoVolume);
        std::vector<std::string> lines;
        std::vector<bool> chat_lines;  // which are chat (cl_chat_notify, scr_chathud)
        cl::DemoHeard joined_heard;    // a game joined: what its frames read carried (remote_step)
        if (drawn_remote) joined_heard = {ds.started, ds.stopped, ds.voice, ds.prints, ds.centre};
        cl::hear_fx(bench_fx, (drawn_remote ? joined_heard : bench.heard).started);  // ce_shells
        if (hearing.frame(ds, drawn_remote ? joined_heard : bench.heard, static_cast<float>(wall),
                          lines, hud.centre, &chat_lines))
          hud.centre_until = t + picture.centre_time;  // scr_centertime
        for (size_t i = 0; i < lines.size(); ++i) tell(lines[i], t, chat_lines[i]);
      }
      // No level of the demo's yet: the last frame stays (a joined game's world is its own).
      if (!world || (!drawn_remote && !demo_world)) continue;
      // Bob, roll and kick rebuilt on the recorded eye; a game joined by its prediction.
      cl::apply_view_cvars(camera_cvars, shown);
      cl::DemoBody body;
      if (drawn_remote && remote.predicted) {
        body.known = true;
        body.velocity = remote.ahead.velocity;
        body.on_ground = remote.ahead.ground >= 0;
      }
      const cl::DemoState& ds_read = drawn_remote ? remote.demo.state : bench.demo.state;
      const ge::shooter::BrushModel* ground =
          bench_map && !bench_map->spec.models.empty() ? &bench_map->spec.models[0] : nullptr;
      cl::third_person_eyes(playback, !drawn_remote || remote.end.cfg.spectator, demo_eyes);
      ge::render::FrameView view = cl::demo_eyes_view(
          ds, ds_read, demo_eyes, shown.row, ground, body,
          cl::q2_demo_fov_y(ds, camera_cvars.fov, !drawn_remote, q2_screen, w, h),
          drawn_remote      ? t
          : bench.clock < 0 ? ds.time
                            : bench.clock,
          t);
      const bool was_free = free_look.held;  // SHIFT lets go of the player and takes him back
      cl::free_look_step(free_look, shift_look && mouse.held, held.mouse.dx, held.mouse.dy,
                         controls);
      if (free_look.held != was_free)
        cl::spectate_event(console, free_look.held ? "f_freeflyspectate" : "f_trackspectate");
      cl::free_look_apply(free_look, view);
      const double own_behind =  // cl_thirdperson: the player's own view from behind him
          demo_eyes.behind > 0
              ? 0
              : cl::q2_third_person(game_cvars.q2, view, ground,
                                    behind_movers(bench_map ? &bench_map->spec.models : nullptr));
      bench_demo_fx.draw_own = demo_eyes.behind > 0 || own_behind > cl::kOwnFigureUnits;
      bench_demo_fx.own_alpha = demo_eyes.behind > 0 ? 1 : float(cl::own_figure_alpha(own_behind));
      view.frame = static_cast<uint64_t>(presented);
      cl::from_above(*world, !drawn_remote &&
                                 slm::watch_fly_view(live_map, view, w, h, playback.bar_bottom_px));
      overlay.begin(w, h);
      if (!drawn_remote && bench_map)  // the live map under the HUD: TV and demos alone
        slm::watch_frame(live_map, bench_folder.path ? *bench_folder.path : files, bench_map->file,
                         ds, overlay, w, h, playback.bar_bottom_px);
      se::Status hud_status = cl::demo_status(ds, t);
      if (drawn_remote) cl::team_frame(console, chat, hud_status, ds.map, ds.bonuses, &ds);
      if (drawn_remote) cl::group_status(controls.groups, hud_status, t);  // the weapon group
      cl::q2_bar_net(q2_bar, drawn_remote ? sn::remote_net(remote) : nullptr,
                     sn::remote_unanswered(remote), t);  // scr_lag_draw's packets
      if (!cl::draw_recorded_q2_bar(  // its own bar, the centre print in its letters
              overlay, q2_bar, ds_read, w, h,
              t < hud.centre_until ? std::string_view(hud.centre) : std::string_view())) {
        se::fill_hud(hud, hud_status, cl::hud_art_of(files, classic_art, free_art));
        playback.bar_bottom_px = cl::hud_inset(
            hud.layout.draw(overlay, hud.layout.frame(w, h, se::hud_style()), hud_look()), h);
      } else {
        q2_chat_lines(q2_bar, hud_status);
      }
      if (drawn_remote)
        cl::draw_joined_level_end(overlay, ds, game_menu.art(), w, h, win.print_speed);
      if (bench.active && bench.paced && !attract && !game_menu.open())  // none on a game joined
        cl::playback_draw(overlay, playback, bench, w, h, t);
      cl::draw_corners(overlay, w, h, pacing, hud.readouts, cl::corner_readings(nullptr, t),
                       drawn_remote ? sn::remote_net(remote) : nullptr);
      cl::draw_trouble_icons(overlay, w, h, cl::hud_art_of(files, classic_art, free_art),
                             pacing.times, win.show_turtle, sn::remote_net_trouble(remote));
      if (paced) {  // a demo watched or a game joined: the menu and the console over it
        game_menu.draw(overlay, w, h, t);
        con_view.frame(wall);
        cl::draw_console_shown(gpu_debug.shot.hiding(), overlay, con_view, con_look, t);
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
      ready_fx(bench_fx_state);
      bench_fx_state.models = &cl::world_models(*world);
      bench_fx_state.on = slm::watch_fx(live_map, cl::fx_switches(picture));
      const double fx_dt =  // the demo's own clock: it stands while the demo is held
          cl::playback_fx_step(playback, !drawn_remote && bench.paced && !tv_watch.on, wall);
      const double fx_t = playback.fx_t;
      cl::demo_fx_step(ds, bench_demo_fx, bench_map.get(), view, bench_fx_state, fx_t, fx_dt,
                       bench_fx);
      // See-through liquids on a joined server where its `watervis` allows; a demo always may.
      bench_fx.see_through = !drawn_remote || shooters_edge::look::watervis_allowed(ds.serverinfo);
      // The quick weapon's preview held in place of the game's weapon (quick_fire.hpp).
      in_hand.show = drawn_remote ? cl::quick_show(controls.quick, cl::demo_status(ds, t), t) : -1;
      in_hand.recording = !drawn_remote;  // r_viewmodellastfired plays back alone
      auto hand = cl::step_demo_held(ds, in_hand, view_settings, weapons, view, fx_t, fx_dt);
      if (drawn_remote) cl::quick_hand(controls.quick, hand);
      cl::held_cases(bench_fx_state.cases, held_fire, hand, bench_fx_state.on.shells, fx_t);
      view.shift_y = se::view_over_bars(hud, h);  // the view over the classic status bars
      auto* gun = (!drawn_remote && live_map.fly.p < 1) || demo_eyes.behind > 0 || own_behind > 0
                      ? nullptr
                      : &hand;
      cl::draw_world(*device, *world, view, bench_fx, gun, fx_dt, overlay, target);
      ge::render::gpu_watch_frame(gpu_debug.watch, cl::world_stats(*world), wall, console);
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
        if (demo_world && t - demo_level_t >= kDemoSettle) {
          demo_shown = true;
          if (std::exchange(demo_hold, false) && !game_start.loading) con_view.set_full(false);
        }
        ++presented;
        continue;
      }
      // At most one frame in flight (a swap chain of two): the GPU's time is in the frame rate.
      const wgpu::Future done = device->queue.OnSubmittedWorkDone(
          wgpu::CallbackMode::WaitAnyOnly, [](wgpu::QueueWorkDoneStatus, wgpu::StringView) {});
      if (bench_in_flight.id != 0) ge::gpu::wait(*device, bench_in_flight);
      bench_in_flight = done;
      cl::timedemo_mark(bench, ge::platform::now_ns());
      ++presented;
      continue;
    }
    cl::release_load_line(loader, load_line, con_view, game_start.loading);
    // The title (no world of the server's map yet, or the menu still up over the title): the
    // menu and the console over the night, the spinner while a map loads, the server waiting. A
    // save loaded from it is its world made behind the window.
    if (!world || !world_current || listen.server == nullptr || (on_title && game_menu.open())) {
      if (listen.map_changed && listen.map && listen.server != nullptr) {
        listen.map_changed = false;
        world_current = false;
        load_listen_world(t);
      }
      const bool own_load = !remote.active && !bench.active;  // no game joined, no demo
      std::string caption = own_load ? cl::load_caption(loader) : std::string();  // or console
      const bool console_load = own_load && con_look.on() &&
                                cl::console_load_line(loader, load_line, console, con_view, t);
      if (console_load) caption.clear();
      if (!cl::busy(loader)) ge::audio::play_menu_music(music);  // no game: the menu's track
      title_frame(t, wall, caption, loader.since_s, false, console_load);
      ++presented;
      continue;
    }
    cl::send_user_info(listen, console, user_info);    // the name and colours, as they change
    listen.speedjump_view = user_info.speedjump_view;  // cl_speedjump_view, live
    // The lobby's screen (hud/lobby_page.hpp, or the classic look's): the player is not on the
    // map, so it is drawn over the main menu's backdrop while the server runs on (the bots and the
    // warmup fight on without him), until he joins the warmup or the match starts.
    if (cl::on_lobby_screen(listen)) {
      const int frames = cl::listen_advance(listen, wall, ge::shooter::MoveCmd{});
      cl::record_tick(recorder, listen, frames, console);
      hear_here(wall);
      if (std::exchange(listen.restarted, false)) forget_seen();
      printed = cl::game_lines_joined(remote.ignore, listen.prints, listen.print_levels, printed, t,
                                      [&](const std::string& line) { tell(line, t); });
      title_frame(t, wall, {}, 0, true);
      ++presented;
      continue;
    }
    cl::Moves moves = playing ? cl::button_moves(controls) : cl::Moves{};
    // The quick weapon (quick_fire.hpp): the fire keys as the weapon to ask for and the attack.
    if (listen.server != nullptr) {  // the weapon groups first (weapon_groups.hpp)
      const se::Status said = cl::status_of(listen, t);
      cl::weapon_moves(controls, adapter.game->weapon_groups, said, t, moves);
    }
    // A single-player program's intermission holds the view where the program set it.
    const bool held_view = listen.campaign_host && listen.campaign.end != cl::LevelEnd::none;
    if (!held_view) cl::turn_view(played, moves, controls, wall, listen.view);
    const ge::shooter::MoveCmd cmd = cl::read_controls(moves, played, controls);
    if (listen.server != nullptr && !held_view) {
      const cl::PlayerState me = cl::player_state(listen);
      if (listen.campaign_host)  // a keyboard player's pitch drifts to the slope, as the original's
        cl::drift_pitch(moves, controls, drift, me.on_ground, me.noclip, cmd, me.ideal_pitch, wall,
                        listen.view);
      else
        cl::spring_view(played, controls, me.on_ground, cmd, me.ideal_pitch, wall, listen.view);
      // The aim help's turn: toward the player a shot of his was bent onto, while he fires.
      ge::shooter::Vec3f eye, middle;
      const bool seen = cl::aim_turn_points(listen, listen.aim_turn.target, eye, middle);
      cl::aim_turn_view(controls, listen.aim_turn, listen.view, eye, seen ? &middle : nullptr,
                        (cmd.buttons & ge::shooter::kButtonAttack) != 0, wall);
    }
    if (cmd.impulse != 0) listen.impulse = cmd.impulse;
    // A single-player game waits while the console or the menu is up (a co-op game with
    // another player in it runs on, as the original's does), or a movie plays.
    const bool ui_up = con_view.open() || game_menu.open();
    if (!listen.cinematic.empty()) cl::cinematic_start(cine, std::exchange(listen.cinematic, {}));
    const bool movie = cl::cinematic_frame(cine, files, synth, cmd.buttons != 0, ui_up,
                                           !listen.pending_map.empty(), t);
    for (const std::string& line : std::exchange(cine.said, {})) tell(line, t);
    const bool paused = movie || (game_cvars.q2.autopause && listen.campaign_host && ui_up &&
                                  cl::listen_players(listen) <= 1);                // cl_autopause
    const double moved = cl::q2_game_dt(game_cvars.q2, win.host_framerate, wall);  // its clock
    cl::q2_session_frame(console, game_cvars.q2, remote, listen, paused);  // cl_beginmapcmd ...
    const int frames = paused ? 0 : cl::listen_advance(listen, moved, cmd);
    cl::record_tick(recorder, listen, frames, console);
    hear_here(wall);
    // What the program stuffed into the player's console runs there, as the original's does.
    for (const std::string& line : std::exchange(listen.stuffed, {}))
      console.execute(line, ge::console::Source::server);
    // A track the program asked for (the intermission's, an episode's end) plays, looping.
    if (const int track = std::exchange(listen.campaign.cdtrack, -1); track >= 0)
      ge::audio::play_level_music(music, track);
    if (!listen.failed.empty()) {
      out::error("shooters_edge: the campaign's next map did not start: " + listen.failed +
                 "; see docs/shooters-edge.md § Playing it");
      return 2;
    }
    if (listen.map_changed) {  // the campaign went on to its next map: its world made behind
      listen.map_changed = false;
      world_current = false;
      load_listen_world(t);
      continue;  // the title from the next frame, not the last map from the new one's eyes
    }
    if (std::exchange(listen.restarted, false)) forget_seen();  // the lobby's match restarted
    const bool sky_typed = std::exchange(render_words.sky_asked, false);  // `sky`'s box
    if (std::exchange(render_words.reload_asked, false) || sky_typed) load_listen_world(t);
    if (listen.campaign.sellscreen) {  // the shareware episode's end: the help pages
      listen.campaign.sellscreen = false;
      game_menu.show(menu_host, "help");
    }
    printed = cl::game_lines_joined(remote.ignore, listen.prints, listen.print_levels, printed, t,
                                    [&](const std::string& line) { tell(line, t); });
    if (centred > listen.centres.size()) centred = 0;     // a new listen
    for (; centred < listen.centres.size(); centred++) {  // a centre print: the last one shows
      hud.centre = listen.centres[centred];
      hud.centre_until = hud.centre.empty() ? 0 : t + picture.centre_time;  // scr_centertime
    }
    // The eyes on the player and what the world throws, then the overlay: the HUD, the console.
    shown.lerp = picture.lerp_move;  // r_lerpmove: the eye between the server's frames too
    shown.aspect = h > 0 ? static_cast<double>(w) / h : shown.aspect;  // cl_adjustfov 0's
    cl::apply_view_cvars(camera_cvars, shown);
    ge::render::FrameView view = cl::frame_view(listen, shown, frames, t, wall);
    const double behind = cl::q2_third_person(  // cl_thirdperson
        game_cvars.q2, view,
        listen.map && !listen.map->spec.models.empty() ? &listen.map->spec.models[0] : nullptr,
        behind_movers(listen.map ? &listen.map->spec.models : nullptr));
    view.frame = static_cast<uint64_t>(presented);
    fx_state.models = world ? &cl::world_models(*world) : nullptr;
    ready_fx(fx_state);
    fx_state.on = cl::fx_switches(picture);  // the enhanced weapon effects, by their ce_ switches
    cl::step_fx(listen, shown, fx_state, t, wall, fx);
    if (behind > cl::kOwnFigureUnits && fx.has_self) {  // fading as a wall pushes the camera in
      fx.models.push_back(fx.self);
      fx.models.back().alpha *= float(cl::own_figure_alpha(behind));
    }
    // See-through liquids where the server allows them (its `watervis`).
    fx.see_through = listen.server == nullptr ||
                     shooters_edge::look::watervis_allowed(listen.server->serverinfo);
    view_settings.lerp = picture.lerp_models;  // r_lerpmodels: the held weapon's frames too
    // The quick weapon's preview held in place of the game's weapon (quick_fire.hpp).
    in_hand.show =
        listen.server != nullptr ? cl::quick_show(controls.quick, cl::status_of(listen, t), t) : -1;
    if (paused) hand_paused += wall;  // a paused game holds the hand: its bob is the frozen run's
    shooters_edge::view::HandFrame hand = cl::step_held(
        listen, shown, in_hand, view_settings, weapons, t - hand_paused, paused ? 0 : wall);
    if (listen.server != nullptr) cl::quick_hand(controls.quick, hand);
    // The hand's flash lit (cl_muzzleflash 1), its fire (ce_muzzles) and cases (ce_shells).
    cl::hand_fx(fx_state, held_fire, hand, picture.muzzle_flash == 1, view.time_s, t, fx,
                listen.sounds, cl::kMostListenSounds);
    overlay.begin(w, h);
    const shooters_edge::server::Q2Server* q2 =
        listen.rules ? shooters_edge::server::q2_of(*listen.rules) : nullptr;
    if (listen.campaign.end == cl::LevelEnd::none) {
      se::Status hud_status = cl::status_of(listen, t);
      cl::team_frame(console, chat, hud_status, listen.map ? listen.map->name : "", listen.bonuses);
      // Quake II's bars (its own, or over the classic status while wearing its things) or ours.
      const int cross = hud.layout.find("crosshair");
      if (!cl::draw_q2_bars(
              overlay, q2_bar, worn_hud.bar, listen.rules, files, remix, hud_status,
              !listen.campaign_host,
              cross >= 0 && hud.layout[static_cast<size_t>(cross)].place.show ? &hud.crosshair
                                                                              : nullptr,
              t < hud.centre_until ? std::string_view(hud.centre) : std::string_view(), t, w, h)) {
        cl::group_status(controls.groups, hud_status, t);  // the weapon group (weapon_groups.hpp)
        se::fill_hud(hud, hud_status,
                     q2 ? worn_hud.art : cl::hud_art_of(files, classic_art, free_art));
        const ge::hud::Frame hf = hud.layout.frame(w, h, se::hud_style());
        cl::draw_hud_2d(overlay, hud.layout, hf, hud_look(), q2_screen);  // scr_draw2d
        cl::draw_screen_graph(overlay, screen_graph, &q2_screen, t, w, h, cl::q2_bar_scale(w, h));
        view.shift_y = se::view_over_bars(hud, h);  // the view over the classic status bars
      } else {
        q2_chat_lines(q2 ? q2_bar : worn_hud.bar, hud_status);
      }
    }
    if (listen.room && listen.server != nullptr) {
      // The warmup's banner in the packs' own menu pictures when they carry them, else modern.
      se::LobbyView lv = cl::listen_lobby_view(listen);
      // A classic game counts down in its own centre print (server/announce.hpp).
      lv.count = shooters_edge::server::words_of(listen.server->localinfo, "count_head").empty();
      shooters_edge::menu::draw_lobby_in(overlay, game_menu.classic(), game_menu.art(), lv, w, h,
                                         false);
    }
    const bool scores_on =  // the score keys (TAB: +showscores), unless scr_draw2d hides them
        !con_keys.held && !was_open && cl::shows_scores(q2_screen);
    cl::draw_scores(overlay, listen, game_menu.art(), w, h, scores_on && cl::scores_held(controls),
                    win.level_stats, board_view, scores_on && cl::team_scores_held(controls),
                    win.show_pause, win.print_speed);
    cl::draw_corners(overlay, w, h, pacing, hud.readouts, cl::corner_readings(&listen));
    cl::draw_trouble_icons(overlay, w, h, cl::hud_art_of(files, classic_art, free_art),
                           pacing.times, win.show_turtle, false);
    cl::draw_cinematic(overlay, cine, w, h);
    game_menu.draw(overlay, w, h, t);
    con_view.frame(wall);
    cl::draw_console_shown(gpu_debug.shot.hiding(), overlay, con_view, con_look, t);
    cl::draw_loading(overlay, loader, listen, game_menu.art(), w, h, t);  // the next world made
    wgpu::TextureView target = acquire_frame();
    if (!target) continue;
    cl::apply_view_from(view_from, view,
                        std::string(adapter.game->id) + " " + (listen.map ? listen.map->name : ""));
    cl::draw_world(*device, *world, view, fx, behind > 0 ? nullptr : &hand, wall, overlay, target);
    ge::render::gpu_watch_frame(gpu_debug.watch, cl::world_stats(*world), wall, console);
    shot_copy.frame(*device, surface.frame_target(), shots, console);
    present_frame();
    ++presented;
    if (const int asked = std::exchange(render_words.refresh_asked, 0); asked > 0)
      console.print(
          cl::refresh_line(cl::time_refresh(*device, *world, view, fx, cl::kTimeRefreshFrames,
                                            asked == 2, w, h, acquire_frame, present_frame),
                           cl::kTimeRefreshFrames));
  }
  if (quit) {  // the player's f_exit first, run at once
    cl::team_event(console, "f_exit");
    console.run();
    save_settings();
  }
  const double seconds = static_cast<double>(ge::platform::now_ns() - first_ns) * 1e-9;
  out::status(cl::run_line(presented, seconds, longest, w, h, device->errors.load()));
  return device->errors.load() == 0 && !device->lost.load() && bench_failed == 0 ? 0 : 1;
}

// What nothing else caught ends the program here with a line saying so, not std::terminate's.
int main(int argc, char** argv) {
  try {
    return run_main(argc, argv);
  } catch (const std::exception& e) {
    cl::say_crash(std::string("an uncaught exception: ") + e.what());
  } catch (...) {
    cl::say_crash("an uncaught exception that is not a std::exception");
  }
  return 1;
}
