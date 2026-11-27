// Quake II's view and 2D settings (client/q2_view.hpp): the field of view shown, its height on a
// wide screen, and the cvars in its layer.
//
// ge:summary Tests Quake II's field of view as shown (sent, recorded, uf) and its view cvars
// ge:keywords q2 fov cl_adjustfov uf scr_scale cl_kickangles q2_shown_fov q2_own_fov
// ge:skill shooter
#include "shooters_edge/client/q2_view.hpp"

#include "camera/first_person.hpp"
#include "camera/pose.hpp"
#include "check.hpp"
#include "console/console.hpp"
#include "shooters_edge/client/demo_lerp.hpp"

namespace cl = shooters_edge::client;

namespace {

constexpr double kDeg = 3.14159265358979323846 / 180;

bool near(double a, double b) { return a - b < 1e-9 && b - a < 1e-9; }

}  // namespace

TEST(a_joined_server_shows_the_field_of_view_it_sends) {
  CHECK(cl::q2_shown_fov(40, 110, false, 0) == 40);  // a mod's zoom
  CHECK(cl::q2_shown_fov(90, 110, false, 0) == 90);  // fixed by the server's flag
  CHECK(cl::q2_shown_fov(0, 110, false, 0) == 110);  // none sent: the player's own
  CHECK(cl::q2_shown_fov(0, 0, false, 0) == 90);     // under 1: 90
  CHECK(cl::q2_shown_fov(0, 300, false, 0) == 160);  // held to 160
}

TEST(the_rules_are_told_the_players_own_field_of_view_never_zero) {
  // cl_fov 0 (the rig's own) is told as 90: the rules hold a 0 to one degree, a view a sliver wide.
  CHECK(cl::q2_own_fov(0) == 90);
  CHECK(cl::q2_own_fov(0.5) == 90);
  CHECK(cl::q2_own_fov(110) == 110);
  CHECK(cl::q2_own_fov(300) == 160);
}

TEST(a_recording_shows_the_players_own_unless_it_zooms_or_uf_says) {
  CHECK(cl::q2_shown_fov(90, 110, true, 0) == 110);  // 90 or more recorded: his own
  CHECK(cl::q2_shown_fov(120, 100, true, 0) == 100);
  CHECK(cl::q2_shown_fov(30, 110, true, 0) == 30);     // a recorded zoom still shows
  CHECK(cl::q2_shown_fov(30, 110, true, 4) == 110);    // uf 4: always his own
  CHECK(cl::q2_shown_fov(120, 110, true, 32) == 120);  // uf 32: always the recorded one
}

TEST(cl_adjustfov_keeps_the_4_3_height_or_the_angle_across) {
  const double kept = cl::q2_fov_y(90, true, 16.0 / 9.0);
  CHECK(near(kept, ge::camera::fov_y_of(90 * kDeg, 4.0 / 3.0)));
  const double across = cl::q2_fov_y(90, false, 16.0 / 9.0);
  CHECK(near(across, ge::camera::fov_y_of(90 * kDeg, 16.0 / 9.0)));
  CHECK(across < kept);  // the same angle across a wider screen: a shorter view
}

TEST(a_recording_of_another_game_keeps_the_cameras_field_of_view) {
  cl::DemoState ds;
  ds.format = cl::DemoFormat::client;
  ds.fov = 30;
  cl::Q2Screen s;
  CHECK(near(cl::q2_demo_fov_y(ds, 110, true, s, 1920, 1080), ge::camera::FirstPersonRow{}.fov_y));
  ds.format = cl::DemoFormat::q2;
  CHECK(near(cl::q2_demo_fov_y(ds, 110, true, s, 1920, 1080), cl::q2_fov_y(30, true, 1)));
  s.uf = "4";
  CHECK(near(cl::q2_demo_fov_y(ds, 110, true, s, 1920, 1080), cl::q2_fov_y(110, true, 1)));
}

TEST(the_sent_field_of_view_is_drawn_between_two_frames) {
  cl::DemoLerp l;
  cl::DemoState s;
  s.format = cl::DemoFormat::q2;
  s.frames = 2;
  s.time = 1.0;
  s.fov = 90;
  cl::lerp_frame(l, s, true);
  s.time = 1.1;
  s.fov = 30;  // zoomed in the next frame
  cl::lerp_frame(l, s);
  CHECK(near(cl::lerp_at(l, s, 1.05).fov, 60));
  CHECK(cl::lerp_at(l, s, 1.2).fov == 30);
}

TEST(the_cvars_are_registered_with_their_defaults) {
  ge::console::Console c;
  cl::Q2Screen s;
  cl::add_q2_screen_cvars(c.cvars(), s);
  CHECK(c.cvars().find("scr_draw2d") == nullptr);  // every game's (screen_2d.hpp)
  CHECK(s.scale == 0 && s.adjust_fov == 1 && s.kick_angles);
  c.execute("scr_scale 3; scr_alpha 0.5; scr_lag_draw 2; scr_lag_x 8; uf 32");
  CHECK(s.scale == 3 && s.alpha == 0.5 && s.lag_draw == 2 && s.lag_x == 8);
  CHECK(s.uf == "32");
  c.execute("cl_kickangles 0");  // a cheat: refused while sv_cheats is 0
  CHECK(s.kick_angles);
}
