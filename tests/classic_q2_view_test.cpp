// QUAKE II'S VIEW BY THE SERVER (plugins/classic/q2_view.hpp): standing still is the bare
// view height; running bobs the eye and nods and dips the view by the stride and the run; a strafe
// rolls it right side down; a blow from the right kicks it toward the blow and fades over half a
// second; a hard landing dips it nose down and lowers the eye; a shot's kick lasts one frame; the
// dead view has no kick; the offset never leaves the box.
//
// ge:summary Tests: Quake II's view: bob, run tilts, blow and fall kicks, shots, the dead, clamps
// ge:keywords test q2 view bob run_pitch run_roll kick-angles view-offset fall damage-kick
// ge:skill classic testing
#include <cmath>
#include <string>

#include "check.hpp"
#include "classic/q2_view.hpp"

namespace sh = ge::shooter;

namespace {

bool near(float a, float b, float tol = 1e-4f) { return std::fabs(a - b) <= tol; }

ge::classic::Q2ViewIn running(sh::Vec3f velocity) {
  ge::classic::Q2ViewIn in;
  in.velocity = velocity;
  in.view_angles = {0, 0, 0};  // looking along +x
  return in;
}

}  // namespace

TEST(standing_still_is_the_view_height_and_no_kick) {
  ge::classic::Q2ViewState st;
  const ge::classic::Q2View v = ge::classic::step_q2_view(st, {}, running({0, 0, 0}));
  CHECK(v.offset.x == 0 && v.offset.y == 0 && v.offset.z == 22);
  CHECK(v.kick.x == 0 && v.kick.y == 0 && v.kick.z == 0);
}

TEST(running_bobs_the_eye_and_nods_the_view) {
  ge::classic::Q2ViewState st;
  const ge::classic::Q2View v = ge::classic::step_q2_view(st, {}, running({300, 0, 0}));
  // A quarter step at over 210: the share sin(pi/4).
  const float share = std::sqrt(0.5f);
  CHECK_MSG(near(v.offset.z, 22 + share * 300 * 0.005f, 1e-3f), std::to_string(v.offset.z));
  CHECK_MSG(near(v.kick.x, 300 * 0.002f + share * 0.002f * 300, 1e-3f), std::to_string(v.kick.x));
  // The lift is held to 6 units however fast he runs.
  ge::classic::Q2ViewState fast;
  ge::classic::Q2ViewSettings big;
  big.bob_up = 1;
  fast.bob_time = 0.25f;  // the next frame at the stride's top
  CHECK(near(ge::classic::step_q2_view(fast, big, running({300, 0, 0})).offset.z, 28));
}

TEST(a_strafe_right_rolls_the_view_right_side_down) {
  ge::classic::Q2ViewState st;
  st.bob_time = -0.25f;  // the frame lands on a stride's end: no bob roll
  const ge::classic::Q2View v = ge::classic::step_q2_view(st, {}, running({0, -300, 0}));
  CHECK_MSG(near(v.kick.z, 300 * 0.005f, 1e-3f), std::to_string(v.kick.z));
}

TEST(a_blow_kicks_toward_it_and_fades_over_half_a_second) {
  ge::classic::Q2ViewState st;
  ge::classic::q2_view_blow(st, 20, 50, {0, -100, 0});  // from his right
  const float rolls[6] = {15, 12, 9, 6, 3, 0};
  for (const float want : rolls) {
    const ge::classic::Q2View v = ge::classic::step_q2_view(st, {}, running({0, 0, 0}));
    CHECK_MSG(near(v.kick.z, want, 1e-3f),
              std::to_string(v.kick.z) + " for " + std::to_string(want));
    CHECK(near(v.kick.x, 0, 1e-3f));
  }
  // From ahead it tips the view up (the pitch negative).
  ge::classic::q2_view_blow(st, 20, 20, {100, 0, 0});
  CHECK(ge::classic::step_q2_view(st, {}, running({0, 0, 0})).kick.x < -1);
  // No push: no kick.
  ge::classic::Q2ViewState calm;
  ge::classic::q2_view_blow(calm, 30, 0, {0, -100, 0});
  CHECK(ge::classic::step_q2_view(calm, {}, running({0, 0, 0})).kick.z == 0);
}

TEST(a_hard_landing_dips_the_view_and_the_eye) {
  ge::classic::Q2ViewState st;
  ge::classic::q2_view_fall(st, 10);  // under 15: nothing
  CHECK(ge::classic::step_q2_view(st, {}, running({0, 0, 0})).kick.x == 0);
  ge::classic::q2_view_fall(st, 40);  // 20 degrees, 8 units
  ge::classic::Q2View v = ge::classic::step_q2_view(st, {}, running({0, 0, 0}));
  CHECK(near(v.kick.x, 20, 1e-3f) && near(v.offset.z, 22 - 8, 1e-3f));
  v = ge::classic::step_q2_view(st, {}, running({0, 0, 0}));
  CHECK_MSG(near(v.kick.x, 20.0f * 2 / 3, 1e-3f), std::to_string(v.kick.x));
  ge::classic::step_q2_view(st, {}, running({0, 0, 0}));
  CHECK(ge::classic::step_q2_view(st, {}, running({0, 0, 0})).kick.x == 0);
}

TEST(a_shots_kick_lasts_one_frame_and_the_dead_have_none) {
  ge::classic::Q2ViewState st;
  st.weapon_angles = {-2, 0, 0};
  st.weapon_origin = {-2, 0, 0};
  ge::classic::Q2View v = ge::classic::step_q2_view(st, {}, running({0, 0, 0}));
  CHECK(v.kick.x == -2 && v.offset.x == -2);
  v = ge::classic::step_q2_view(st, {}, running({0, 0, 0}));
  CHECK(v.kick.x == 0 && v.offset.x == 0);
  ge::classic::q2_view_blow(st, 50, 50, {0, -100, 0});
  ge::classic::Q2ViewIn dead = running({0, 0, 0});
  dead.dead = true;
  dead.view_height = -2;
  v = ge::classic::step_q2_view(st, {}, dead);
  CHECK(v.kick.x == 0 && v.kick.z == 0 && v.offset.z == -2);
}

TEST(the_offset_stays_in_the_box) {
  ge::classic::Q2ViewState st;
  st.weapon_origin = {40, -40, 0};
  ge::classic::Q2ViewIn in = running({0, 0, 0});
  in.view_height = 50;
  const ge::classic::Q2View v = ge::classic::step_q2_view(st, {}, in);
  CHECK(v.offset.x == 14 && v.offset.y == -14 && v.offset.z == 30);
}

TEST(the_gun_sways_with_the_stride_lags_a_turn_and_moves_by_the_settings) {
  ge::classic::Q2ViewState st;
  ge::classic::Q2ViewIn in = running({0, 0, 0});
  in.velocity = {300, 0, 0};
  in.view_angles = {0, 0, 0};
  ge::classic::Q2View v = ge::classic::step_q2_view(st, {}, in);  // the clock at 0.25: share 0.707
  const float sway = 300 * 0.70710677f;
  CHECK_MSG(near(v.gun_angles.x, sway * 0.005f, 1e-3f), std::to_string(v.gun_angles.x));
  CHECK(near(v.gun_angles.y, sway * 0.01f, 1e-3f) && near(v.gun_angles.z, sway * 0.005f, 1e-3f));
  in.velocity = {};
  in.view_angles = {0, 100, 0};  // a turn of 100 degrees left: the gun trails 45 of it
  v = ge::classic::step_q2_view(st, {}, in);
  CHECK(near(v.gun_angles.y, -9, 1e-4f) && near(v.gun_angles.z, -4.5f, 1e-4f));
  in.view_angles = {0, -170, 0};  // on past 180: the change wraps to -90, held to -45
  v = ge::classic::step_q2_view(st, {}, in);
  CHECK(near(v.gun_angles.y, -9, 1e-4f));
  ge::classic::Q2ViewSettings set;
  set.gun_x = 2;
  set.gun_z = 3;
  in.view_angles = {0, -170, 0};
  v = ge::classic::step_q2_view(st, set, in);
  CHECK(v.gun_angles.y == 0);
  const ge::shooter::ViewAxes ax = ge::shooter::angle_vectors(in.view_angles);
  const ge::shooter::Vec3f want = ax.right * 2.0f - ax.up * 3.0f;
  CHECK(near(v.gun_offset.x, want.x, 1e-4f) && near(v.gun_offset.z, want.z, 1e-4f));
}
