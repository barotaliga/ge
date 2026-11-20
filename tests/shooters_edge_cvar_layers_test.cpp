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
#include <string>
#include <vector>

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
  cl::GameStart start;
  cl::GameCvarValues game;
  bool predict = true;  // the remote's cl_predict
  int crouch = 1;       // the remote's cl_predict_crouch
  int noskins = 0;      // the picture's stock_figures (Quake II's cl_noskins)

  explicit Window(const pr::GameProfile& g) {
    cl::add_coop_cvars(console, coop);
    cl::add_cvars(console, controls);
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
    console.cvars().set_layer(cl::kLayerEngine);
    ge::capture::add_screenshot_cvars(console.cvars(), shots);
    cl::add_start_cvars(console, start, g);
    cl::add_game_cvars(console, game, g,
                       {.crosshair = &hud.crosshair,
                        .mouse = &controls.mouse,
                        .predict = &predict,
                        .crouch = &crouch,
                        .noskins = &noskins,
                        .view = &view});
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
                         .predict = &w.predict,
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
  cl::switch_game_cvars(w.console, w.game, pr::kGameProfiles[0], {.crosshair = &w.hud.crosshair});
  CHECK(v.find("crosshair")->layer != "game:q2" && v.find("crosshair")->value == "5");
  CHECK(w.hud.crosshair.mark == 5 && !w.hud.crosshair.pictures);
  CHECK(v.find("vid_gamma") == nullptr);
  CHECK(v.find("scr_centertime")->value == "2");
  CHECK(v.clashes().empty());
}

TEST(each_games_fov_and_move_speeds) {
  Window w(pr::kGameProfiles[0]);
  con::Cvars& v = w.console.cvars();
  v.set("fov", "110", con::Source::local);  // Quake's spelling of the lens
  CHECK(v.find("cl_fov")->value == "110");
  CHECK(v.find("cl_forwardspeed")->value == "400");
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
                         .predict = &w.predict,
                         .crouch = &w.crouch,
                         .noskins = &w.noskins,
                         .view = &w.view});
  for (const char* name : {"cl_forwardspeed", "cl_backspeed", "cl_sidespeed", "cl_upspeed"})
    CHECK_MSG(v.find(name)->value == "200", name);  // Quake II's walk
  CHECK(v.find("w_switch") == nullptr);             // Quake II's servers have no such rule
  CHECK(!w.console.has_command("viewalias"));       // Quake's spelling of `alias <name>`
  v.set("hand", "1", con::Source::local);           // Quake II's user info
  CHECK(v.find("hand")->value == "1" && (v.find("hand")->flags & con::kUserInfo) != 0);
  CHECK(w.view.hand == 1);
  CHECK(v.find("gender")->value == "male" && v.find("gender_auto")->value == "1");
  CHECK(cl::auto_gender("Female/athena") == "female" && cl::auto_gender("cyborg/oni911") == "male");
  CHECK(cl::auto_gender("crakhor/x") == "none" && cl::auto_gender("crackhor") == "female");
  CHECK(w.controls.mouse.per_frame);             // its own m_accel
  v.set("cl_predict", "0", con::Source::local);  // our cl_nopred the other way round
  CHECK(!w.predict);
  v.set("cl_predict_crouch", "2", con::Source::local);  // the eye's height on every server
  CHECK(w.crouch == 2);
  v.set("cl_noskins", "2", con::Source::local);  // its own, beside the shared noskins
  CHECK(w.noskins == 2 && v.find("noskins")->value == "0");
  v.set("fov", "100", con::Source::local);
  CHECK(v.find("cl_fov")->value == "100");
  v.set("cl_sidespeed", "300", con::Source::local);  // the player's own stays across the switch
  cl::switch_game_cvars(w.console, w.game, pr::kGameProfiles[0],
                        {.crosshair = &w.hud.crosshair,
                         .mouse = &w.controls.mouse,
                         .predict = &w.predict,
                         .crouch = &w.crouch,
                         .noskins = &w.noskins,
                         .view = &w.view});
  CHECK(v.find("cl_forwardspeed")->value == "400");
  CHECK(!w.controls.mouse.per_frame && v.find("hand") == nullptr);
  CHECK(w.predict && v.find("cl_predict") == nullptr);       // Quake always predicts
  CHECK(v.find("cl_predict_crouch") == nullptr);             // and never ducks
  CHECK(v.find("cl_noskins") == nullptr && w.noskins == 0);  // its figures its own
  CHECK(w.view.hand == 0);                                   // its weapon always drawn
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
                         .predict = &w.predict,
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
                         .predict = &w.predict,
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
                         .predict = &w.predict,
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
  CHECK(st.row.bob == 0);
  v.set("v_viewheight", "9", con::Source::local);  // past the range: held at 4
  CHECK(w.camera.viewlift == 4);
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
  CHECK(u.find("r_viewmodeloffset") == nullptr && u.find("cl_gun_y") != nullptr);
  u.set("cl_gun_y", "3", con::Source::local);
  u.set("cl_gun_z", "-1", con::Source::local);
  shooters_edge::view::game_offset_m(q.view, add);
  CHECK(add[0] == 0 && std::fabs(add[1] + ge::brush::kUnitM) < 1e-12);
  CHECK(std::fabs(add[2] - 3 * ge::brush::kUnitM) < 1e-12);
  CHECK(std::fabs(shooters_edge::view::eye_row(q.view).fov_y -
                  ge::camera::fov_y_of(ge::core::kPi / 2, 4.0 / 3.0)) < 1e-12);  // cl_gunfov 90
  u.set("r_viewmodel_fov", "50", con::Source::local);                            // ours set wins
  CHECK(std::fabs(shooters_edge::view::eye_row(q.view).fov_y - 50 * ge::core::kPi / 180) < 1e-12);
}
