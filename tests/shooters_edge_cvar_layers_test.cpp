// The window's cvar layers (games/shooters_edge/client/cvar_layers.hpp): every layer's settings
// registered as the window registers them, for each classic game, meet without a clash; each
// game's own rows are in its layer, and loading another game swaps them.
//
// ge:summary Tests the window's cvar layers: no clash in any game, Quake II's crosshair, swaps
// ge:keywords test cvar-layer game-cvars per-game-cvar clash one-name-one-meaning game-switch
// ge:keywords series-config one-config save-scope without_game_cvars
// ge:skill console shooter testing
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "audio/music_console.hpp"
#include "brush/map.hpp"
#include "camera/cvars.hpp"
#include "capture/screenshot.hpp"
#include "check.hpp"
#include "console/console.hpp"
#include "console/view.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/controls.hpp"
#include "shooters_edge/client/coop_mode.hpp"
#include "shooters_edge/client/cvar_layers.hpp"
#include "shooters_edge/client/demo_playback.hpp"
#include "shooters_edge/client/game_start.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/settings.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/hud/layout.hpp"
#include "shooters_edge/menu/game_menu.hpp"
#include "shooters_edge/net/q2_remote.hpp"
#include "shooters_edge/view/hand.hpp"

namespace cl = shooters_edge::client;
namespace pr = shooters_edge::profiles;
namespace con = ge::console;

namespace {

// The window's settings, registered in client/main.cpp's order and layers (less the window's
// and the pacing's own, which link the platform: engine settings no game declares).
struct Window {
  con::Console console{false};
  cl::CoopMode coop;
  cl::ControlSettings controls;
  shooters_edge::look::Picture picture;
  shooters_edge::view::ViewSettings view;
  ge::camera::CameraCvars camera;
  cl::UserInfo user;
  cl::DemoPlayback playback;
  cl::Timedemo bench;
  shooters_edge::hud::Hud hud = shooters_edge::hud::make_hud();
  shooters_edge::menu::GameMenu menu;
  ge::capture::ScreenshotSettings shots;
  ge::audio::MusicConsole music;  // bgmvolume and cd, the window's before the game's
  cl::GameStart start;
  cl::GameCvarValues game;
  int crouch = 1;            // the remote's cl_predict_crouch
  int noskins = 0;           // the picture's stock_figures (Quake II's cl_noskins)
  bool items_still = false;  // the picture's items_still (Quake II's cl_nobob)
  int moves = 62;            // the remote's q2_maxfps (Quake II's cl_maxfps)
  double frame_cap = 0;      // the pacing's frame cap (our cl_maxfps, Quake II's r_maxfps)
  cl::ChatExtras chat;       // the chat's extras (Quake II's cl_chat_sound)
  bool show_pause = true;    // window_cvars.cpp's showpause (Quake II's scr_showpause)
  bool minimize = true;      // platform/window_cvars.cpp's vid_minimize_on_focus_loss

  explicit Window(const pr::GameProfile& g) {
    cl::add_coop_cvars(console, coop);
    cl::add_cvars(console, controls);
    // The remote's cl_nopred as net/qw_remote.cpp registers it (Quake II's cl_predict maps on it).
    console.cvars().add({.name = "cl_nopred", .fallback = "0", .kind = con::CvarKind::boolean});
    console.cvars().set_layer(cl::kLayerClassic);
    shooters_edge::look::add_cvars(console, picture);
    console.cvars().set_layer(cl::kLayerEngine);
    shooters_edge::view::add_view_cvars(console.cvars(), view);
    cl::add_view_camera_cvars(console.cvars(), camera);
    cl::add_user_info_cvars(console, user);
    cl::add_playback_commands(console, playback, bench);
    shooters_edge::hud::add_cvars(console, hud);
    console.cvars().set_layer(cl::kLayerShell);
    menu.add_cvars(console);
    // showpause as window_cvars.cpp registers it (that file links the platform).
    console.cvars().bind({.name = "showpause",
                          .fallback = "1",
                          .flags = con::kArchive,
                          .help = "Whether a paused campaign shows the pause picture.",
                          .kind = con::CvarKind::boolean,
                          .values = "0: no picture; 1: the picture while paused (the default)"},
                         &show_pause);
    console.cvars().set_layer(cl::kLayerEngine);
    ge::capture::add_screenshot_cvars(console.cvars(), shots);
    ge::audio::add_music_commands(console, music);
    // The window's minimising as platform/window_cvars.cpp registers it (that file links the
    // platform).
    console.cvars().bind(
        {.name = "vid_minimize_on_focus_loss",
         .fallback = "1",
         .flags = con::kArchive,
         .help = "Whether the full-screen game minimises when you switch to "
                 "another window, the screen going back to the desktop's own mode.",
         .kind = con::CvarKind::boolean,
         .values = "1: minimised (the default, but on a Mac); 0: it stays full "
                   "screen behind the other window (the default on a Mac)"},
        &minimize);
    // The pacing's frame cap as platform/pacing.cpp registers it (that file links the platform).
    console.cvars().bind({.name = "cl_maxfps",
                          .fallback = "0",
                          .flags = con::kArchive,
                          .help = "The most frames a second, held on an even grid.",
                          .min = 0,
                          .max = 10000},
                         &frame_cap);
    cl::add_chat_extras(console, chat);
    cl::add_start_cvars(console, start, g);
    cl::add_game_cvars(console, game, g,
                       {.crosshair = &hud.crosshair,
                        .mouse = &controls.mouse,
                        .crouch = &crouch,
                        .noskins = &noskins,
                        .items_still = &items_still,
                        .view = &view,
                        .moves = &moves,
                        .frame_cap = &frame_cap,
                        .light_intensity = &picture.light_intensity,
                        .light_lift = &picture.light_lift,
                        .no_glow = &picture.no_glow,
                        .chat = &chat});
  }
};

std::string clash_lines(const con::Console& c) {
  std::string out;
  for (const con::CvarClash& k : c.cvars().clashes()) out += con::clash_text(k) + "\n";
  return out;
}

}  // namespace

TEST(no_layer_meets_another_without_saying_how_in_any_game) {
  for (const pr::GameProfile& g : pr::kGameProfiles) {
    Window w(g);
    CHECK_MSG(w.console.cvars().clashes().empty(),
              std::string(g.id) + ": the window's cvar layers clash:\n" + clash_lines(w.console));
  }
}

TEST(q2_crosshair_is_its_own_and_the_first_games_comes_back) {
  Window w(pr::kGameProfiles[0]);
  con::Cvars& v = w.console.cvars();
  v.set("crosshair", "5", con::Source::local);  // Quake's: a ring
  CHECK(w.hud.crosshair.mark == 5 && !w.hud.crosshair.pictures);
  const pr::GameProfile* q2 = pr::find_game("q2");
  CHECK(q2 != nullptr);
  if (q2 == nullptr) return;
  cl::switch_game_cvars(w.console, w.game, *q2,
                        {.crosshair = &w.hud.crosshair,
                         .mouse = &w.controls.mouse,
                         .crouch = &w.crouch,
                         .noskins = &w.noskins,
                         .view = &w.view});
  const con::Cvar* c = v.find("crosshair");
  CHECK(c != nullptr && c->layer == "game:q2" && c->max == 3);
  CHECK(w.hud.crosshair.pictures && w.hud.crosshair.mark == 1);  // its own default, picture 1
  v.set("crosshair", "3", con::Source::local);  // its third picture, never the engine's cross
  CHECK(w.hud.crosshair.mark == 3);
  v.set("vid_gamma", "0.8", con::Source::local);  // its spelling of gamma
  CHECK(w.picture.gamma == 0.8);
  CHECK(v.find("scr_centertime")->value == "2.5");
  CHECK(v.find("vid_minimize_on_focus_loss")->value == "0" && !w.minimize);  // stays full screen
  v.set("vid_flip_on_switch", "1", con::Source::local);  // its spelling of the same setting
  CHECK(w.minimize);
  cl::switch_game_cvars(w.console, w.game, pr::kGameProfiles[0], {.crosshair = &w.hud.crosshair});
  CHECK(v.find("crosshair")->layer != "game:q2" && v.find("crosshair")->value == "5");
  CHECK(w.hud.crosshair.mark == 5 && !w.hud.crosshair.pictures);
  CHECK(v.find("vid_gamma") == nullptr);
  CHECK(v.find("scr_centertime")->value == "2");
  CHECK(v.find("vid_flip_on_switch") == nullptr);
  CHECK(v.clashes().empty());
}

TEST(each_games_fov_and_move_speeds) {
  Window w(pr::kGameProfiles[0]);
  con::Cvars& v = w.console.cvars();
  v.set("fov", "110", con::Source::local);  // Quake's spelling of the lens
  CHECK(v.find("cl_fov")->value == "110");
  CHECK(v.find("cl_forwardspeed")->value == "400");
  CHECK(v.find("rate")->value == "2500" && (v.find("rate")->flags & con::kUserInfo) != 0);
  CHECK(v.find("password")->value.empty());
  v.set("w_switch", "8", con::Source::local);  // Quake's servers' switch rule
  CHECK(v.find("w_switch")->value == "8" && (v.find("w_switch")->flags & con::kUserInfo) != 0);
  w.console.execute("cvar_reset w_switch; alias go \"echo go\"");
  CHECK(v.find("w_switch")->value.empty());  // its spelling of `reset`
  v.set("w_switch", "8", con::Source::local);
  w.console.clear_output();
  w.console.execute("viewalias go");
  CHECK(!w.console.output().empty() && w.console.output().back() == "go : echo go");
  const pr::GameProfile* q2 = pr::find_game("q2");
  CHECK(q2 != nullptr);
  if (q2 == nullptr) return;
  cl::switch_game_cvars(w.console, w.game, *q2,
                        {.crosshair = &w.hud.crosshair,
                         .mouse = &w.controls.mouse,
                         .crouch = &w.crouch,
                         .noskins = &w.noskins,
                         .items_still = &w.items_still,
                         .view = &w.view,
                         .menu = &w.menu});
  for (const char* name : {"cl_forwardspeed", "cl_backspeed", "cl_sidespeed", "cl_upspeed"})
    CHECK_MSG(v.find(name)->value == "200", name);  // Quake II's walk
  CHECK(v.find("rate")->value == "15000");          // its servers may send more
  CHECK(v.find("w_switch") == nullptr);             // Quake II's servers have no such rule
  CHECK(!w.console.has_command("viewalias"));       // Quake's spelling of `alias <name>`
  v.set("hand", "1", con::Source::local);           // Quake II's user info
  CHECK(v.find("hand")->value == "1" && (v.find("hand")->flags & con::kUserInfo) != 0);
  CHECK(w.view.hand == 1);
  v.set("cl_gun", "3", con::Source::local);  // its own: 3 always on the left, not r_drawviewmodel
  CHECK(w.view.gun == 3 && w.view.draw);
  v.set("ch_x", "8", con::Source::local);  // its crosshair's place is our cl_crossx
  CHECK(v.find("cl_crossx")->value == "8" && w.hud.crosshair.offset[0] == 8);
  v.set("ch_green", "0.25", con::Source::local);
  v.set("ch_health", "1", con::Source::local);
  CHECK(w.hud.crosshair.shade[1] == 0.25 && w.hud.crosshair.by_health);
  CHECK(v.find("gender")->value == "male" && v.find("gender_auto")->value == "1");
  CHECK(cl::auto_gender("Female/athena") == "female" && cl::auto_gender("cyborg/oni911") == "male");
  CHECK(cl::auto_gender("crakhor/x") == "none" && cl::auto_gender("crackhor") == "female");
  CHECK(w.controls.mouse.per_frame);             // its own m_accel
  v.set("cl_predict", "0", con::Source::local);  // our cl_nopred the other way round
  CHECK(v.find("cl_nopred")->value == "1");
  v.set("cl_predict", "1", con::Source::local);
  CHECK(v.find("cl_nopred")->value == "0");
  v.set("cl_predict_crouch", "2", con::Source::local);  // the eye's height on every server
  CHECK(w.crouch == 2);
  v.set("cl_noskins", "2", con::Source::local);  // its own, beside the shared noskins
  CHECK(w.noskins == 2 && v.find("noskins")->value == "0");
  v.set("cl_nobob", "1", con::Source::local);  // its floating items lie still
  CHECK(w.items_still && v.find("fs_autoexec")->value == "1");
  v.set("ui_scale", "2", con::Source::local);  // its menus' size; its browser's sort and pace
  v.set("ui_sortservers", "-4", con::Source::local);
  v.set("ui_pingrate", "20", con::Source::local);
  CHECK(w.menu.menu_scale() == 2 && w.menu.servers().game_sort == -4 &&
        w.menu.servers().game_ping_rate == 20);
  v.set("fov", "100", con::Source::local);
  CHECK(v.find("cl_fov")->value == "100");
  v.set("cl_sidespeed", "300", con::Source::local);  // the player's own stays across the switch
  cl::switch_game_cvars(w.console, w.game, pr::kGameProfiles[0],
                        {.crosshair = &w.hud.crosshair,
                         .mouse = &w.controls.mouse,
                         .crouch = &w.crouch,
                         .noskins = &w.noskins,
                         .items_still = &w.items_still,
                         .view = &w.view,
                         .menu = &w.menu});
  CHECK(v.find("cl_forwardspeed")->value == "400");
  CHECK(v.find("rate")->value == "2500");  // Quake's default again
  CHECK(!w.controls.mouse.per_frame && v.find("hand") == nullptr);
  CHECK(v.find("cl_predict") == nullptr);                            // its spelling gone
  CHECK(v.find("cl_predict_crouch") == nullptr);                     // and never ducks
  CHECK(v.find("cl_noskins") == nullptr && w.noskins == 0);          // its figures its own
  CHECK(v.find("cl_nobob") == nullptr && !w.items_still);            // nothing floats in Quake
  CHECK(w.menu.menu_scale() == 0 && v.find("ui_scale") == nullptr);  // its menus fill the window
  CHECK(w.menu.servers().game_sort == shooters_edge::menu::ServersPage::kNoGameSort &&
        w.menu.servers().game_ping_rate == -1);
  CHECK(v.find("fs_autoexec") == nullptr);
  CHECK(w.view.hand == 0 && w.view.gun == 1);  // its weapon always drawn
  CHECK(v.find("cl_gun") == nullptr && v.find("ch_red") == nullptr);
  CHECK(w.hud.crosshair.shade[1] == 1 && !w.hud.crosshair.by_health);  // Quake II's set aside
  CHECK(v.find("cl_sidespeed")->value == "300");
  CHECK(v.find("w_switch") != nullptr);  // Quake's again (its value from its config)
  CHECK(v.clashes().empty());
}

TEST(loading_another_game_takes_the_old_games_own_away_and_back) {
  const pr::GameProfile& first = pr::kGameProfiles[0];
  for (const pr::GameProfile& next : pr::kGameProfiles) {
    if (next.id == first.id) continue;
    Window w(first);
    con::Cvars& v = w.console.cvars();
    const auto in_layer = [&v](const std::string& layer) {
      std::vector<std::string> out;
      for (const con::Cvar* c : v.sorted())
        if (c->layer == layer) out.push_back(c->name);
      for (const con::Synonym& s : v.synonyms())
        if (s.layer == layer) out.push_back(s.name);
      return out;
    };
    const std::vector<std::string> own_first = in_layer(cl::game_layer(first.id));
    cl::switch_game_cvars(w.console, w.game, next, {.crosshair = &w.hud.crosshair});
    CHECK(in_layer(cl::game_layer(first.id)).empty());
    const std::vector<std::string> own_next = in_layer(cl::game_layer(next.id));
    cl::switch_game_cvars(w.console, w.game, first, {.crosshair = &w.hud.crosshair});
    CHECK(in_layer(cl::game_layer(next.id)).empty());
    CHECK(in_layer(cl::game_layer(first.id)) == own_first);
    for (const std::string& name : own_next)
      CHECK_MSG(v.find(name) == nullptr || v.find(name)->layer != cl::game_layer(next.id),
                name + " stayed Quake II's after Quake came back");
    CHECK_MSG(v.clashes().empty(), clash_lines(w.console));
  }
}

// ONE CONFIG FOR THE SERIES (client/settings.hpp): the series' file holds our names with the
// values they have outside the loaded game; the game's own file only its own meanings.
TEST(the_series_config_keeps_our_names_and_the_game_file_its_own_meanings) {
  Window w(pr::kGameProfiles[0]);
  con::Cvars& v = w.console.cvars();
  const pr::GameProfile* q2 = pr::find_game("q2");
  CHECK(q2 != nullptr);
  if (q2 == nullptr) return;
  v.set("crosshair", "5", con::Source::local);  // Quake's ring
  cl::switch_game_cvars(w.console, w.game, *q2,
                        {.crosshair = &w.hud.crosshair,
                         .mouse = &w.controls.mouse,
                         .crouch = &w.crouch,
                         .noskins = &w.noskins,
                         .view = &w.view});
  v.set("crosshair", "3", con::Source::local);    // Quake II's third picture
  v.set("vid_gamma", "0.8", con::Source::local);  // its spelling of our gamma
  const auto outside = [](std::string_view l) { return !cl::is_game_layer(l); };
  const std::string series = w.console.save_text({.binds = true, .layers = outside});
  const std::string own = w.console.save_text({.binds = false, .layers = cl::is_game_layer});
  CHECK_MSG(series.find("seta crosshair \"5\"") != std::string::npos, series);
  CHECK_MSG(series.find("seta gamma \"0.8\"") != std::string::npos, series);
  CHECK(series.find("vid_gamma") == std::string::npos);
  // Not set by the player: Quake's own default, never Quake II's 2.5.
  CHECK_MSG(series.find("seta scr_centertime \"2\"") != std::string::npos, series);
  CHECK_MSG(own.find("seta crosshair \"3\"") != std::string::npos, own);
  CHECK(own.find("gamma") == std::string::npos && own.find("\nbind ") == std::string::npos);
  CHECK(own.find("scr_centertime") == std::string::npos);

  // Read back with Quake II loaded: the series' ring is Quake's, Quake II keeps 3.
  Window again(pr::kGameProfiles[0]);
  cl::switch_game_cvars(again.console, again.game, *q2, {.crosshair = &again.hud.crosshair});
  again.console.cvars().set("crosshair", "2", con::Source::local);  // from the install's config
  cl::without_game_cvars(again.console, again.game, *q2, {.crosshair = &again.hud.crosshair}, [&] {
    again.console.append(series);
    again.console.run();
  });
  CHECK(again.console.cvars().find("crosshair")->value == "2");  // Quake II's, kept
  again.console.append(own);
  again.console.run();
  CHECK(again.hud.crosshair.mark == 3 && again.hud.crosshair.pictures);
  CHECK(again.picture.gamma == 0.8);
  cl::switch_game_cvars(again.console, again.game, pr::kGameProfiles[0],
                        {.crosshair = &again.hud.crosshair});
  CHECK(again.console.cvars().find("crosshair")->value == "5");
  CHECK(again.hud.crosshair.mark == 5 && !again.hud.crosshair.pictures);
}

// A GAME'S OWN VALUE (console/cvar.hpp § SCOPES): in a config, a game's spelling stands over our
// name for that game alone, saved in the game's file; the other games keep the common value.
TEST(a_games_spelling_in_a_config_is_its_own_value_over_the_common_one) {
  Window w(pr::kGameProfiles[0]);
  const pr::GameProfile* q2 = pr::find_game("q2");
  CHECK(q2 != nullptr);
  if (q2 == nullptr) return;
  const std::string scope = cl::game_scope(*q2);
  CHECK(!scope.empty() && scope != cl::game_scope(pr::kGameProfiles[0]));
  cl::switch_game_cvars(w.console, w.game, *q2,
                        {.crosshair = &w.hud.crosshair,
                         .mouse = &w.controls.mouse,
                         .crouch = &w.crouch,
                         .noskins = &w.noskins,
                         .view = &w.view});
  CHECK(w.console.cvars().scope() == scope);
  w.console.append("vid_gamma 0.8\ngamma 1.1\n", con::Source::local, {"config.cfg", 1});
  w.console.run();
  CHECK(w.picture.gamma == 0.8);
  const auto mine = [&scope](std::string_view s) { return s == scope; };
  const std::string own =
      w.console.save_text({.binds = false, .layers = cl::is_game_layer, .scopes = mine});
  CHECK_MSG(own.find("seta " + scope + ":gamma \"0.8\"") != std::string::npos, own);
  const auto outside = [](std::string_view l) { return !cl::is_game_layer(l); };
  const auto others = [&scope](std::string_view s) { return s != scope; };
  const std::string series =
      w.console.save_text({.binds = true, .layers = outside, .scopes = others});
  CHECK_MSG(series.find("seta gamma \"1.1\"") != std::string::npos, series);
  CHECK(series.find(":gamma") == std::string::npos);
  cl::switch_game_cvars(w.console, w.game, pr::kGameProfiles[0], {.crosshair = &w.hud.crosshair});
  CHECK(w.picture.gamma == 1.1);
  w.console.execute("gamma 1.3");  // typed: the common value, Quake II's own kept
  cl::switch_game_cvars(w.console, w.game, *q2,
                        {.crosshair = &w.hud.crosshair,
                         .mouse = &w.controls.mouse,
                         .crouch = &w.crouch,
                         .noskins = &w.noskins,
                         .view = &w.view});
  CHECK(w.picture.gamma == 0.8);
}

TEST(zoom_narrows_the_view_and_slows_the_mouse_while_held) {
  Window w(pr::kGameProfiles[0]);
  con::Cvars& v = w.console.cvars();
  v.set("sensitivity", "4", con::Source::local);
  v.set("fov", "100", con::Source::local);  // Quake's spelling of cl_fov
  w.console.execute("+zoom");
  CHECK(v.find("cl_fov")->value == "35");
  CHECK_MSG(v.find("sensitivity")->value == "1.4", v.find("sensitivity")->value);  // 4 * 35 / 100
  w.console.execute("+zoom");  // a second key down changes nothing
  CHECK(v.find("sensitivity")->value == "1.4");
  w.console.execute("-zoom");
  CHECK(v.find("cl_fov")->value == "100" && v.find("sensitivity")->value == "4");
  w.console.execute("-zoom");  // nothing held: nothing to put back
  CHECK(v.find("cl_fov")->value == "100");
  v.set("cl_fov", "0", con::Source::local);  // the rig's own 90
  w.console.execute("+zoom");
  CHECK(std::atof(v.find("sensitivity")->value.c_str()) == 4 * 35.0 / 90);
  w.console.execute("-zoom");
  CHECK(v.find("cl_fov")->value == "0" && v.find("sensitivity")->value == "4");
}

TEST(view_height_lifts_the_eye_and_stops_the_bob) {
  Window w(pr::kGameProfiles[0]);
  con::Cvars& v = w.console.cvars();
  cl::ViewState st;
  cl::apply_view_cvars(w.camera, st);
  CHECK(st.row.eye_lift_m == 0 && st.row.bob > 0);  // 0: the game's own eye and bob
  v.set("v_viewheight", "-6", con::Source::local);
  CHECK(w.camera.viewlift == -6);
  cl::apply_view_cvars(w.camera, st);
  CHECK(std::fabs(st.row.eye_lift_m - -6 * w.camera.unit_m) < 1e-12);
  CHECK(st.row.bob_eye == 0 && st.row.bob > 0);    // the eye's bob given up, the weapon's kept
  v.set("v_viewheight", "9", con::Source::local);  // past the range: held at 4
  CHECK(w.camera.viewlift == 4);
  // cl_bobhead: the weapon's bob carried by the eye too (Quake's, gone in Quake II).
  CHECK(v.find("cl_bobhead")->layer == "classic");
  v.set("cl_bobhead", "1", con::Source::local);
  cl::apply_view_cvars(w.camera, st);
  CHECK(st.row.bob_eye == 1);  // the weapon's bob alone, over v_viewheight
  v.set("v_viewheight", "0", con::Source::local);
  cl::apply_view_cvars(w.camera, st);
  CHECK(st.row.bob_eye == 2);
}

TEST(autosens_slows_the_mouse_by_the_lens) {
  Window w(pr::kGameProfiles[0]);
  con::Cvars& v = w.console.cvars();
  w.controls.lens_fov = &w.camera.fov;
  CHECK(ge::shooter::auto_sens(w.controls).x == 1);  // off by default
  v.set("m_autosens", "1", con::Source::local);      // tuned at 90
  v.set("cl_fov", "45", con::Source::local);
  const ge::shooter::AutoSens half = ge::shooter::auto_sens(w.controls);
  CHECK(half.x == 0.5);
  CHECK(std::fabs(half.y - 34.52 / 73.74) < 1e-3);  // the 4:3 heights: 2 atan(0.75 tan(fov / 2))
  v.set("m_autosens", "120", con::Source::local);   // tuned at 120
  v.set("cl_fov", "0", con::Source::local);         // the rig's own 90
  CHECK(ge::shooter::auto_sens(w.controls).x == 0.75);
  // With it on, +zoom narrows the lens and leaves the sensitivity to it.
  v.set("sensitivity", "4", con::Source::local);
  w.console.execute("+zoom");
  CHECK(v.find("cl_fov")->value == "35" && v.find("sensitivity")->value == "4");
  w.console.execute("-zoom");
  CHECK(v.find("cl_fov")->value == "0");
}

TEST(each_games_own_gun_place_adds_to_ours_and_its_lens_stands_in) {
  Window w(pr::kGameProfiles[0]);
  con::Cvars& v = w.console.cvars();
  CHECK(w.view.unit_m == ge::brush::kUnitM);
  v.set("cl_gunx", "0.1", con::Source::local);
  v.set("r_viewmodeloffset", "4 2", con::Source::local);  // right 4, down 2, forward missing: 0
  v.set("r_viewmodelsize", "0.5", con::Source::local);
  double add[3];
  shooters_edge::view::game_offset_m(w.view, add);
  CHECK(std::fabs(add[0] - 4 * ge::brush::kUnitM) < 1e-12);
  CHECK(std::fabs(add[1] + 2 * ge::brush::kUnitM) < 1e-12 && add[2] == 0);
  const ge::viewmodel::PlaceRow row = shooters_edge::view::eye_row(w.view);
  CHECK(std::fabs(row.offset_m[0] - (0.1 + 4 * ge::brush::kUnitM)) < 1e-12);  // the sum
  CHECK(row.depth == 0.75);
  // Quake II: its offset by its axes (x right, y forward, z up) and its lens across.
  Window q(*pr::find_game("q2"));
  con::Cvars& u = q.console.cvars();
  CHECK(u.find("r_viewmodeloffset") != nullptr && u.find("cl_gun_y") != nullptr);  // every game's
  u.set("cl_gun_y", "3", con::Source::local);
  u.set("cl_gun_z", "-1", con::Source::local);
  shooters_edge::view::game_offset_m(q.view, add);
  CHECK(add[0] == 0 && std::fabs(add[1] + ge::brush::kUnitM) < 1e-12);
  CHECK(std::fabs(add[2] - 3 * ge::brush::kUnitM) < 1e-12);
  CHECK(std::fabs(shooters_edge::view::eye_row(q.view).fov_y -
                  ge::camera::fov_y_of(ge::core::kPi / 2, 4.0 / 3.0)) < 1e-12);  // cl_gunfov 90
  u.set("r_viewmodel_fov", "50", con::Source::local);                            // ours set wins
  CHECK(std::fabs(shooters_edge::view::eye_row(q.view).fov_y - 50 * ge::core::kPi / 180) < 1e-12);
  // cl_gunalpha: how solid the weapon is drawn, every game's.
  CHECK(q.view.alpha == 1);
  u.set("cl_gunalpha", "0.4", con::Source::local);
  CHECK(q.view.alpha == 0.4);
  v.set("cl_gunalpha", "0.5", con::Source::local);
  CHECK(w.view.alpha == 0.5 && w.view.gun_fov_h == 0);  // Quake's lens the view's
}

TEST(the_weapons_place_and_lens_are_kept_across_a_game_change) {
  Window w(pr::kGameProfiles[0]);
  con::Cvars& v = w.console.cvars();
  v.set("cl_gun_x", "2", con::Source::local);
  v.set("r_viewmodeloffset", "1 0 0", con::Source::local);
  const pr::GameProfile* q2 = pr::find_game("q2");
  CHECK(q2 != nullptr);
  if (q2 == nullptr) return;
  cl::switch_game_cvars(w.console, w.game, *q2, {.view = &w.view});
  CHECK(w.view.gun_units[0] == 2 && w.view.offset_text == "1 0 0" && w.view.gun_fov_h == 90);
  cl::switch_game_cvars(w.console, w.game, pr::kGameProfiles[0], {.view = &w.view});
  CHECK(w.view.gun_units[0] == 2 && w.view.offset_text == "1 0 0" && w.view.gun_fov_h == 0);
  CHECK(v.find("cl_gun_x")->layer == cl::kLayerClassic && v.clashes().empty());
}

TEST(quake_twos_cl_maxfps_is_its_move_rate_and_r_maxfps_its_frame_cap) {
  Window w(pr::kGameProfiles[0]);
  con::Cvars& v = w.console.cvars();
  v.set("cl_maxfps", "144", con::Source::local);  // our frame cap, in Quake
  CHECK(w.frame_cap == 144);
  const pr::GameProfile* q2 = pr::find_game("q2");
  const cl::GameBindings b{
      .crosshair = &w.hud.crosshair, .view = &w.view, .moves = &w.moves, .frame_cap = &w.frame_cap};
  cl::switch_game_cvars(w.console, w.game, *q2, b);
  // Loaded: its cl_maxfps the moves a second, its r_maxfps the cap, started at the one in force.
  CHECK(v.find("cl_maxfps")->value == "62" && w.moves == 62);
  CHECK(v.find("r_maxfps")->value == "144" && w.frame_cap == 144);
  v.set("cl_maxfps", "125", con::Source::local);
  v.set("r_maxfps", "200", con::Source::local);
  CHECK(w.moves == 125 && w.frame_cap == 200);
  CHECK(shooters_edge::net::q2_move_ms(125) == 8 && shooters_edge::net::q2_move_ms(62) == 16);
  CHECK(shooters_edge::net::q2_move_ms(0) == 8 && shooters_edge::net::q2_move_ms(4) == 100);
  // Saved: the cap set in it with the game's own settings, ours with the series'.
  const auto outside = [](std::string_view l) { return !cl::is_game_layer(l); };
  const std::string series = w.console.save_text({.binds = false, .layers = outside});
  const std::string own = w.console.save_text({.binds = false, .layers = cl::is_game_layer});
  CHECK_MSG(own.find("seta r_maxfps \"200\"") != std::string::npos, own);
  CHECK_MSG(own.find("seta cl_maxfps \"125\"") != std::string::npos, own);
  CHECK(series.find("r_maxfps") == std::string::npos);
  CHECK_MSG(series.find("seta cl_maxfps \"144\"") != std::string::npos, series);  // ours kept
  // Left: our cl_maxfps comes back with its value, the cap with it.
  cl::switch_game_cvars(w.console, w.game, pr::kGameProfiles[0], b);
  CHECK(v.find("r_maxfps") == nullptr || v.find("r_maxfps")->layer != cl::game_layer(q2->id));
  CHECK_MSG(v.find("cl_maxfps")->value == "144", v.find("cl_maxfps")->value);
  CHECK_MSG(w.frame_cap == 144, std::to_string(w.frame_cap));
}

// Quake II's light settings, with its defaults and limits, scale the maps' light in both looks;
// Quake's own light stays as it was, and gl_modulate (every game's) keeps the player's value.
TEST(quake_twos_intensity_and_gl_modulate_light_its_maps) {
  Window w(*pr::find_game("q2"));
  con::Cvars& v = w.console.cvars();
  CHECK(v.find("intensity")->value == "2" && v.find("gl_modulate")->value == "1");
  CHECK(v.find("gl_brightness")->value == "0");
  v.set("intensity", "3", con::Source::local);
  v.set("gl_modulate", "2", con::Source::local);
  v.set("gl_brightness", "0.2", con::Source::local);
  CHECK(w.picture.light_intensity == 3 && w.picture.light_modulate == 2);
  CHECK(std::fabs(w.picture.light_lift - 0.2) < 1e-12);
  CHECK(shooters_edge::look::light_scale(w.picture) == 3);  // 3 x 2 / 2
  namespace look = shooters_edge::look;
  const ge::brush::app::Options o = look::world_options(look::Look::classic, {}, w.picture);
  CHECK(o.classic_light == 3 && std::fabs(o.classic_lift - 0.2) < 1e-12);
  const ge::brush::app::Options m = look::world_options(look::Look::modern, {}, w.picture);
  CHECK(m.brightness == 3 * ge::brush::app::Options{}.brightness);
  v.set("intensity", "9", con::Source::local);  // held to 1..5
  CHECK(w.picture.light_intensity <= 5);
  // Quake loaded: its maps lit as they were.
  cl::switch_game_cvars(
      w.console, w.game, pr::kGameProfiles[0],
      {.light_intensity = &w.picture.light_intensity, .light_lift = &w.picture.light_lift});
  CHECK(v.find("intensity") == nullptr && v.find("gl_brightness") == nullptr);
  // gl_modulate is every game's (the classic look's): the player's 2 kept.
  CHECK(v.find("gl_modulate") != nullptr && v.find("gl_modulate")->value == "2");
  CHECK(shooters_edge::look::light_scale(w.picture) == 2 && w.picture.light_lift == 0);
}

// The names every game shares (docs/porting-games.md § One name, one meaning): QuakeWorld's
// spellings reach ours while Quake is loaded, and the settings once Quake II's alone are every
// game's, keeping the player's value across a game change.
TEST(the_names_every_game_shares_reach_one_setting_in_both) {
  Window w(pr::kGameProfiles[0]);
  con::Cvars& v = w.console.cvars();
  con::ConsoleView view(w.console);  // con_height, con_bgalpha, con_font: the console's own
  v.add({.name = "vid_vsync", .fallback = "0"});       // the window's (platform/pacing.cpp)
  v.add({.name = "sv_maxclients", .fallback = "32"});  // the listen server's (listen_console)
  w.console.add_command("playdemo", "the window's (client/demo_playback.cpp)",
                        [](con::Console&, const con::Args&) {});
  w.console.add_command("snd_restart", "the window's (client/menu_sound.cpp)",
                        [](con::Console&, const con::Args&) {});
  cl::switch_game_cvars(w.console, w.game, pr::kGameProfiles[0], {});  // read again with them
  CHECK(w.console.has_command("s_restart"));     // QuakeWorld's spelling of the sound restart
  v.set("maxplayers", "8", con::Source::local);  // Quake's word for the next game's players
  CHECK(v.find("sv_maxclients")->value == "8");
  v.set("scr_consize", "0.3", con::Source::local);
  v.set("scr_conalpha", "0.6", con::Source::local);
  v.set("gl_consolefont", "povo5", con::Source::local);
  CHECK(v.find("con_height")->value == "0.3" && v.find("con_bgalpha")->value == "0.6");
  CHECK(v.find("con_font")->value == "povo5");
  v.set("r_drawparticles", "0", con::Source::local);  // its rulesets lock this name
  CHECK(v.find("r_particles")->value == "0" && !w.picture.particles);
  v.set("cl_net_clientport", "27001", con::Source::local);
  v.set("in_grab_windowed_mouse", "0", con::Source::local);
  CHECK(v.find("net_clientport")->value == "27001" && w.game.q2.grab == 0);
  CHECK(v.find("gl_modulate")->layer == cl::kLayerClassic && w.picture.anisotropy == 0);
  v.set("gl_modulate", "1.5", con::Source::local);
  CHECK(w.picture.light_modulate == 1.5);
  const pr::GameProfile* q2 = pr::find_game("q2");
  CHECK(q2 != nullptr);
  if (q2 == nullptr) return;
  cl::switch_game_cvars(w.console, w.game, *q2, {});
  CHECK(v.find("scr_consize") == nullptr && v.find("cl_net_clientport") == nullptr);
  CHECK(v.find("in_grab")->value == "0" && w.game.q2.grab == 0);  // the player's, kept
  CHECK(v.find("net_clientport")->value == "27001" && w.picture.light_modulate == 1.5);
  CHECK(v.find("gl_anisotropy")->value == "1" && w.picture.anisotropy == 1);  // Quake II's 1
  v.set("gl_swapinterval", "1", con::Source::local);  // its spelling of the window's vsync
  CHECK(v.find("vid_vsync")->value == "1");
  CHECK(w.console.has_command("demo"));  // its spelling of playdemo
  CHECK(!w.console.has_command("s_restart"));
  CHECK(v.find("maxplayers") == nullptr && v.synonym("maxplayers") == nullptr);
  cl::switch_game_cvars(w.console, w.game, pr::kGameProfiles[0], {});
  CHECK(v.find("gl_swapinterval") == nullptr && v.find("in_grab")->value == "0");
  CHECK(!w.console.has_command("demo"));
  CHECK(v.clashes().empty());
}

TEST(one_camera_behind_the_player_in_every_game) {
  Window w(pr::kGameProfiles[0]);
  con::Cvars& v = w.console.cvars();
  CHECK(v.find("cam_dist")->value == "100" && w.game.q2.dist == v.find("cam_dist"));
  v.set("chase_back", "80", con::Source::local);  // Quake's spellings of the distance and switch
  CHECK(v.find("cam_dist")->value == "80" && w.playback.behind == 80);
  v.set("sv_cheats", "1", con::Source::server);  // the server allows cheats
  v.set("chase_active", "1", con::Source::local);
  CHECK(v.find("cl_thirdperson")->value == "1" && w.game.q2.third_person);
  const pr::GameProfile* q2 = pr::find_game("q2");
  CHECK(q2 != nullptr);
  if (q2 == nullptr) return;
  cl::switch_game_cvars(w.console, w.game, *q2, {});
  CHECK(v.find("chase_back") == nullptr && w.game.q2.third_person);  // the switch is kept
  v.set("cl_thirdperson_range", "40", con::Source::local);  // Quake II's spelling of the distance
  CHECK(v.find("cam_dist")->value == "40" && w.game.q2.dist->number == 40);
  CHECK(v.find("cl_thirdperson")->layer == cl::kLayerClassic);
  cl::switch_game_cvars(w.console, w.game, pr::kGameProfiles[0], {});
  CHECK(v.find("cl_thirdperson_range") == nullptr && v.find("chase_active") != nullptr);
  CHECK(v.clashes().empty());
}

TEST(the_competitive_settings_are_every_classic_games) {
  // What a competition player tunes to see clearly, one setting each in every classic game: the
  // flat walls, the plain sky, the liquids seen through, the team's and the enemy's skins, the
  // flames and power glows, the crosshair's picture, the tints' strengths and the weapon's place.
  const char* names[] = {
      "r_drawflat",      "r_wallcolor",       "r_floorcolor",   "r_fastsky",
      "r_skycolor",      "r_skyname",         "r_skyfog",       "r_wateralpha",
      "r_slimealpha",    "r_lavaalpha",       "r_telealpha",    "teamskin",
      "enemyskin",       "r_fullbrightskins", "r_drawflame",    "r_powerupglow",
      "crosshairimage",  "v_contentblend",    "v_damagecshift", "v_quadcshift",
      "v_pentcshift",    "v_suitcshift",      "v_ringcshift",   "gl_cshiftpercent",
      "r_viewmodelsize", "r_viewmodeloffset", "cl_gun_x",       "cl_gunfov",
      "cl_gunalpha",     "cl_bobhead"};
  for (const pr::GameProfile& g : pr::kGameProfiles) {
    Window w(g);
    for (const char* n : names) {
      const con::Cvar* c = w.console.cvars().find(n);
      CHECK_MSG(c != nullptr, std::string(g.id) + ": " + n + " is not registered");
      if (c == nullptr) continue;
      const bool default_only =  // the game's default for it, a `same` row
          !c->earlier.empty() && c->earlier.back().how == con::Overlap::same;
      CHECK_MSG(default_only || c->layer == cl::kLayerClassic || c->layer == cl::kLayerEngine ||
                    c->layer == cl::kLayerShooter,
                std::string(g.id) + ": " + n + " is the game's own (" + c->layer + ")");
    }
  }
}

TEST(color_sets_both_colours_as_quake_keeps_them) {
  Window w(pr::kGameProfiles[0]);
  con::Cvars& v = w.console.cvars();
  w.console.execute("color 4");
  CHECK(v.find("topcolor")->value == "4" && v.find("bottomcolor")->value == "4");
  w.console.execute("color 15 17");  // past 13: 13; 17 wraps to 1
  CHECK(v.find("topcolor")->value == "13" && v.find("bottomcolor")->value == "1");
  w.console.clear_output();
  w.console.execute("color");
  CHECK(!w.console.output().empty() && w.console.output()[0].find("13") != std::string::npos);
}

TEST(an_old_quake_configs_cl_name_and_cl_color_dress_you) {
  Window w(pr::kGameProfiles[0]);
  con::Cvars& v = w.console.cvars();
  CHECK(v.find("name") == nullptr || v.synonym("_cl_name") != nullptr);  // name: the settings'
  w.console.execute("_cl_color \"77.000000\"");  // shirt 4, pants 13, as Quake saved them
  CHECK(v.find("topcolor")->value == "4" && v.find("bottomcolor")->value == "13");
  w.console.clear_output();
  w.console.execute("_cl_color");
  CHECK(!w.console.output().empty() && w.console.output()[0] == "\"_cl_color\" is \"77\"");
}

TEST(q2_chat_sound_and_auto_chat_are_its_own_and_go_with_it) {
  Window w(pr::kGameProfiles[0]);
  con::Cvars& v = w.console.cvars();
  ge::console::ConsoleView con_view(w.console);  // its game_chat and stamps, as the window's
  CHECK(w.chat.named_sound.empty() && con_view.game_chat == -1);  // Quake: s_chat_custom, con_chat
  CHECK(v.find("cl_chat_sound") == nullptr && v.find("con_auto_chat") == nullptr);
  CHECK(v.find("cl_chat_notify") != nullptr && v.find("scr_chathud") != nullptr);  // both games
  const pr::GameProfile* q2 = pr::find_game("q2");
  CHECK(q2 != nullptr);
  if (q2 == nullptr) return;
  v.set("con_timestamps", "2", con::Source::local);  // Quake's: chat lines, the seconds too
  CHECK(w.chat.timestamps == 2);
  const cl::GameBindings b{.chat = &w.chat, .console_view = &con_view};
  cl::switch_game_cvars(w.console, w.game, *q2, b);
  CHECK(w.chat.named_sound == "1" && con_view.game_chat == 0);
  // Its own con_timestamps: every console line stamped, the chat's own stamps set aside.
  CHECK(w.chat.timestamps == 0 && !con_view.stamps && v.find("con_timestamps")->value == "0");
  v.set("con_timestamps", "1", con::Source::local);
  v.set("con_timestampsformat", "[%H]", con::Source::local);
  v.set("con_timestampscolor", "red", con::Source::local);
  CHECK(con_view.stamps && con_view.stamp_format == "[%H]" && con_view.stamp_colour == "red");
  con_view.add(ge::console::LineKind::output, "stamped");
  CHECK(con_view.lines().back().stamp.size() == 4 && con_view.lines().back().text == "stamped");
  v.set("cl_chat_sound", "2", con::Source::local);
  v.set("con_auto_chat", "2", con::Source::local);
  CHECK(std::string(cl::q2_chat_sound(w.chat.named_sound)) == "misc/talk1.wav");
  CHECK(con_view.game_chat == 2);
  cl::switch_game_cvars(w.console, w.game, pr::kGameProfiles[0], b);
  CHECK(w.chat.named_sound.empty() && con_view.game_chat == -1);
  CHECK(!con_view.stamps && w.chat.timestamps == 2);  // Quake's chat stamps back
  con_view.add(ge::console::LineKind::output, "plain");
  CHECK(con_view.lines().back().stamp.empty());
  CHECK(v.clashes().empty());
}

TEST(q2_spells_our_effect_switches_and_runs_its_own_session_settings) {
  Window w(pr::kGameProfiles[0]);
  con::Cvars& v = w.console.cvars();
  CHECK(v.find("cl_entities") == nullptr && v.find("cl_beginmapcmd") != nullptr);  // every game's
  v.set("cl_beginmapcmd", "record", con::Source::local);                           // Quake too
  CHECK(w.game.q2.begin_cmd == "record");
  const pr::GameProfile* q2 = pr::find_game("q2");
  CHECK(q2 != nullptr);
  if (q2 == nullptr) return;
  cl::switch_game_cvars(w.console, w.game, *q2, {});
  v.set("cl_entities", "0", con::Source::local);   // our r_drawentities
  v.set("cl_particles", "0", con::Source::local);  // our r_particles
  v.set("cl_lights", "0", con::Source::local);     // our r_dynamic
  v.set("cl_blend", "0", con::Source::local);      // our gl_polyblend
  CHECK(!w.picture.draw_entities && !w.picture.particles && !w.picture.dynamic);
  CHECK(!w.picture.poly_blend);
  const cl::GameBindings gibs{
      .no_glow = &w.picture.no_glow, .q2fx = &w.picture.q2fx, .music = &w.music};
  cl::switch_game_cvars(w.console, w.game, *q2, gibs);
  v.set("cl_gibs", "0", con::Source::local);  // our cl_gibfilter the other way round
  v.set("cl_noglow", "1", con::Source::local);
  CHECK(w.picture.gib_filter && w.picture.no_glow);
  v.set("cl_disable_particles", "24", con::Source::local);  // a rocket's trail, blood
  v.set("cl_disable_explosions", "2", con::Source::local);
  v.set("cl_dlight_hacks", "6", con::Source::local);
  v.set("cl_muzzlelight_time", "32", con::Source::local);
  v.set("cl_muzzleflashes", "0", con::Source::local);
  v.set("cl_footsteps", "0", con::Source::local);
  v.set("s_ambient", "3", con::Source::local);
  v.set("ogg_volume", "0.25", con::Source::local);  // our bgmvolume
  v.set("ogg_enable", "0", con::Source::local);     // the music's switch
  v.set("ogg_shuffle", "1", con::Source::local);
  v.set("ogg_menu_track", "7", con::Source::local);
  CHECK(w.music.volume == 0.25 && !w.music.enabled && w.music.shuffle && w.music.menu_track == 7);
  v.set("cl_hit_markers", "1", con::Source::local);
  v.set("scr_hit_marker_time", "250", con::Source::local);
  CHECK(w.picture.q2fx.hit_markers == 1 && w.picture.q2fx.hit_marker_ms == 250);
  v.set("cl_railtrail_type", "2", con::Source::local);
  v.set("cl_railcore_color", "#0f0", con::Source::local);
  CHECK(w.picture.q2fx.rail_type == 2 && w.picture.q2fx.core_colour == "#0f0");
  float rgb[3] = {};
  CHECK(ge::classic::q2_colour(w.picture.q2fx.core_colour, rgb) && rgb[1] == 1 && rgb[0] == 0);
  CHECK(ge::classic::q2_colour("magenta", rgb) && rgb[0] == 1 && rgb[1] == 0 && rgb[2] == 1);
  CHECK(ge::classic::q2_colour("#336699", rgb) && rgb[0] == 0.2f && rgb[2] == 0.6f);
  CHECK(!ge::classic::q2_colour("purple", rgb) && !ge::classic::q2_colour("#12345z", rgb));
  v.set("scr_showpause", "0", con::Source::local);  // its spelling of our showpause
  CHECK(!w.show_pause && w.picture.q2fx.footsteps == 0 && w.picture.q2fx.ambient == 3);
  const ge::classic::Q2FxSwitches& fx = w.picture.q2fx;
  CHECK(fx.no_particles == 24 && fx.no_explosions == 2 && fx.light_hacks == 6);
  CHECK(fx.muzzle_light_ms == 32 && !fx.muzzle_models);
  CHECK(ge::classic::q2_no_blast_ball(fx, ge::classic::kQ2Rocket));
  CHECK(!ge::classic::q2_no_blast_ball(fx, ge::classic::kQ2Grenade));
  CHECK(!ge::classic::q2_no_blast_specks(fx, ge::classic::kQ2Rocket));  // 4 is not in 24
  CHECK(ge::classic::q2_muzzle_age(fx, 0.02) == 0.01);  // twice as long: half as old
  // The picture's switches, Quake II's defaults while it is loaded.
  CHECK(fx.part_scale == 2 && fx.part_shape == 0 && !fx.additive_parts && !fx.round_beams);
  CHECK(fx.draw_sky && fx.smooth_sky && fx.saturation == 1 && fx.fog);
  CHECK(w.picture.anisotropy == 1);  // every game's, Quake II's 1 (none) while it is loaded
  CHECK(!fx.water_warp && !fx.invert && (v.find("gl_drawsky")->flags & con::kArchive) == 0);
  v.set("gl_partshape", "2", con::Source::local);
  v.set("gl_partstyle", "1", con::Source::local);
  v.set("gl_beamstyle", "1", con::Source::local);
  v.set("gl_drawsky", "0", con::Source::local);
  v.set("gl_saturation", "3", con::Source::local);  // held to 0..1
  v.set("gl_anisotropy", "16", con::Source::local);
  v.set("gl_waterwarp", "1", con::Source::local);
  v.set("gl_fog", "0", con::Source::local);
  CHECK(fx.part_shape == 2 && fx.additive_parts && fx.round_beams && !fx.draw_sky);
  CHECK(fx.saturation == 1 && w.picture.anisotropy == 16 && fx.water_warp && !fx.fog);
  // The light's switches, Quake II's defaults: the models lit twice, shaded, the other players'
  // lean the game's own, the soft falloff.
  CHECK(fx.double_light && fx.dot_shading && fx.roll_hack && fx.dlight_falloff == 1);
  CHECK(fx.modulate_world == 1 && fx.modulate_entities == 1 && !fx.lerp_styles);
  v.set("gl_coloredlightmaps", "0.25", con::Source::local);
  v.set("cl_lerp_lightstyles", "1", con::Source::local);
  CHECK(fx.lightmap_saturation == 0.25 && fx.lerp_styles);
  // The ink line and the explosions' steps.
  CHECK(fx.cel_shading == 0 && fx.smooth_explosions);
  v.set("gl_celshading", "20", con::Source::local);  // held to 0..10
  v.set("cl_smooth_explosions", "0", con::Source::local);
  CHECK(fx.cel_shading == 10 && !fx.smooth_explosions);
  CHECK(ge::classic::q2_outline_px(fx.cel_shading, 0) == 10);
  CHECK(ge::classic::q2_outline_px(fx.cel_shading, 350) == 5);     // half at 350 units
  CHECK(ge::classic::q2_outline_share(fx.cel_shading, 695) == 0);  // gone past about 693
  CHECK(ge::classic::q2_outline_px(0, 0) == 0);
  CHECK(ge::classic::q2_blast_age(fx, 0.27) == 0.2 && ge::classic::q2_blast_age(fx, 0.05) == 0);
  // A glow map's light: twice the setting, 0.75 by default.
  CHECK(fx.glowmap_intensity == 0.75 &&
        ge::classic::q2_glow_map_gain(fx.glowmap_intensity) == 1.5f);
  v.set("gl_glowmap_intensity", "0", con::Source::local);
  CHECK(ge::classic::q2_glow_map_gain(fx.glowmap_intensity) == 0);
  v.set("cl_beginmapcmd", "record", con::Source::local);
  v.set("cl_autopause", "0", con::Source::local);
  CHECK(w.game.q2.begin_cmd == "record" && !w.game.q2.autopause);
  CHECK(v.find("sv_running") != nullptr && v.find("cl_running")->value == "1");
  cl::switch_game_cvars(w.console, w.game, pr::kGameProfiles[0], gibs);
  CHECK(w.picture.gib_filter && !w.picture.no_glow);  // one setting: the player's kept
  CHECK(w.picture.q2fx.no_particles == 0 && w.picture.q2fx.muzzle_models);  // set aside
  CHECK(v.find("cl_disable_particles") == nullptr && v.find("scr_showpause") == nullptr);
  CHECK(w.picture.q2fx.footsteps == 1 && w.picture.q2fx.ambient == 1 && w.music.enabled);
  CHECK(!w.music.shuffle && w.music.menu_track == 0);
  CHECK(w.picture.q2fx.rail_type == 0 && w.picture.q2fx.core_colour == "red");
  CHECK(w.picture.q2fx.part_scale == 0 && w.picture.q2fx.part_shape == -1);  // the engine's own
  CHECK(w.picture.q2fx.draw_sky && w.picture.q2fx.fog);
  CHECK(w.picture.anisotropy == 16);  // gl_anisotropy is every game's: the player's 16 kept
  CHECK(!w.picture.q2fx.double_light && !w.picture.q2fx.roll_hack);
  CHECK(w.picture.q2fx.dlight_falloff == -1 && w.picture.q2fx.lightmap_saturation == 1);
  CHECK(w.picture.q2fx.cel_shading == 0 && w.picture.q2fx.smooth_explosions);
  CHECK(ge::classic::q2_blast_age(w.picture.q2fx, 0.27) == 0.27);  // as it is
  CHECK(w.game.q2.begin_cmd == "record" && w.game.q2.autopause && w.game.q2.timescale == 1);
  CHECK(v.find("cl_beginmapcmd") != nullptr && v.find("sv_running") == nullptr);
  CHECK(v.clashes().empty());
}

TEST(q2s_logfile_keeps_what_the_console_prints) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "se_layers_logfile";
  std::filesystem::remove_all(dir);
  con::Console c;
  std::vector<std::string> shown;
  c.set_sink([&shown](std::string_view l) { shown.emplace_back(l); });
  c.set_search_path({dir});
  cl::GameCvarValues v;
  const pr::GameProfile* q2 = pr::find_game("q2");
  CHECK(q2 != nullptr);
  if (q2 == nullptr) return;
  cl::add_game_cvars(c, v, *q2, {});
  c.execute("logfile 2; logfile_prefix \"\"");
  c.print("kept");
  const std::string opened =
      "Logging console to " + (dir / "logs" / "console.log").generic_string();
  // The console's own sink still sees it, after the log's opening.
  CHECK(shown.size() == 2 && shown[0] == opened && shown[1] == "kept");
  c.execute("logfile 0");
  c.print("not kept");
  CHECK(shown.size() == 4 && shown[2] == "Closing console log." && shown[3] == "not kept");
  std::ifstream in(dir / "logs" / "console.log");
  std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  CHECK_MSG(text == opened + "\nkept\nClosing console log.\n", text);
  std::filesystem::remove_all(dir);
}

TEST(quake_twos_m_side_is_quakes_sidestep_a_whole_unit_a_count) {
  // m_side is one setting in both games (shooter/controls.cpp): Quake's default 0.8, Quake II's 1,
  // the field written by whichever is loaded; Quake's comes back when Quake II leaves.
  Window w(pr::kGameProfiles[0]);
  con::Cvars& v = w.console.cvars();
  CHECK(v.find("m_side")->value == "0.8" && w.controls.m_side == 0.8);
  const cl::GameBindings b{.view = &w.view};
  cl::switch_game_cvars(w.console, w.game, *pr::find_game("q2"), b);
  CHECK(v.find("m_side")->value == "1" && w.controls.m_side == 1);
  cl::switch_game_cvars(w.console, w.game, pr::kGameProfiles[0], b);
  CHECK(v.find("m_side")->value == "0.8" && w.controls.m_side == 0.8);
}

TEST(quake_twos_screenshots_are_numbered_quake000_up_and_packed_by_its_setting) {
  // scr_template is the engine's (capture/screenshot.hpp): named by the time unless a game names
  // a template; Quake II's are quakeXXX, spelled gl_screenshot_template, and its
  // gl_screenshot_compression is scr_compression.
  Window w(pr::kGameProfiles[0]);
  con::Cvars& v = w.console.cvars();
  CHECK(v.find("scr_template")->value.empty() && w.shots.name_template.empty());
  const cl::GameBindings b{.view = &w.view};
  cl::switch_game_cvars(w.console, w.game, *pr::find_game("q2"), b);
  CHECK(w.shots.name_template == "quakeXXX");
  w.console.execute("gl_screenshot_template shotXX; gl_screenshot_compression 0");
  CHECK(w.shots.name_template == "shotXX" && w.shots.compression == 0);
  cl::switch_game_cvars(w.console, w.game, pr::kGameProfiles[0], b);  // one setting: the player's
  CHECK(v.find("scr_template")->value == "shotXX" && w.shots.name_template == "shotXX");
}
