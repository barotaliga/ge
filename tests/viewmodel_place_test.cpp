// The held weapon's place (engine/viewmodel/place.hpp): at rest it sits at its offset; the bob
// pushes it forward by its share; a turn to the right leaves it pointing left of the look and it
// catches up; a shot pushes it back and lifts the muzzle and both settle; put away it drops out of
// the picture; comfort 0 removes every motion the player did not command; and the same frames
// give the same bits.
//
// ge:summary Tests: the held weapon's rest, bob, sway, kick, put-away, comfort and repeatability
// ge:keywords test viewmodel place bob sway kick recoil lower comfort frame_viewmodel
// ge:skill render testing
// ge:test tests/viewmodel_place_test.cpp
#include <cmath>

#include "check.hpp"
#include "viewmodel/light.hpp"
#include "viewmodel/place.hpp"

namespace vm = ge::viewmodel;

namespace {
constexpr double kDt = 1.0 / 60.0;
}

TEST(at_rest_it_sits_at_its_offset) {
  const vm::PlaceRow row = vm::classic_row();
  vm::PlaceState st;
  vm::PlaceInput in;
  vm::Placement p;
  for (int i = 0; i < 30; ++i) p = vm::frame_viewmodel(row, in, st, kDt);
  for (int r = 0; r < 3; ++r) CHECK(std::fabs(p.m[r * 4 + 3] - row.offset_m[r]) < 1e-9);
  CHECK(std::fabs(p.m[0] - 1) < 1e-12 && std::fabs(p.m[5] - 1) < 1e-12);
}

TEST(the_bob_pushes_it_forward_by_its_share) {
  const vm::PlaceRow row = vm::classic_row();
  vm::PlaceState st;
  vm::PlaceInput in;
  in.bob_m = 0.05;
  const vm::Placement p = vm::frame_viewmodel(row, in, st, kDt);
  CHECK(std::fabs(p.shift_m[2] - row.bob_forward * 0.05) < 1e-12);
}

TEST(a_turn_leaves_it_behind_and_it_catches_up) {
  const vm::PlaceRow row = vm::classic_row();
  vm::PlaceState st;
  vm::PlaceInput in;
  vm::frame_viewmodel(row, in, st, kDt);
  double least = 0;
  vm::Placement p;
  for (int i = 0; i < 20; ++i) {  // turning right at 3 rad/s
    in.look_yaw += 3 * kDt;
    p = vm::frame_viewmodel(row, in, st, kDt);
    least = p.turn[0] < least ? p.turn[0] : least;
  }
  CHECK(least < -0.01);  // it points left of the look
  CHECK(least >= -row.sway_max - 1e-12);
  for (int i = 0; i < 120; ++i) p = vm::frame_viewmodel(row, in, st, kDt);
  CHECK(std::fabs(p.turn[0]) < 1e-3);  // and settles on it
}

TEST(a_shot_kicks_it_back_and_up_and_it_settles) {
  const vm::PlaceRow row = vm::classic_row();
  vm::PlaceState st;
  vm::PlaceInput in;
  vm::frame_viewmodel(row, in, st, kDt);
  in.shot = 1;
  vm::Placement p = vm::frame_viewmodel(row, in, st, kDt);
  in.shot = 0;
  double back = p.shift_m[2], up = p.turn[1];
  for (int i = 0; i < 6; ++i) {
    p = vm::frame_viewmodel(row, in, st, kDt);
    back = p.shift_m[2] < back ? p.shift_m[2] : back;
    up = p.turn[1] > up ? p.turn[1] : up;
  }
  CHECK(back < -0.005);
  CHECK(up > 0.01);
  for (int i = 0; i < 120; ++i) p = vm::frame_viewmodel(row, in, st, kDt);
  CHECK(std::fabs(p.shift_m[2]) < 1e-4 && std::fabs(p.turn[1]) < 1e-4);
}

TEST(put_away_it_drops_and_tips_down) {
  const vm::PlaceRow row = vm::classic_row();
  vm::PlaceState st;
  vm::PlaceInput in;
  in.lowered = 1;
  st.fresh = false;  // raised when the order came
  vm::Placement p;
  for (int i = 0; i < 60; ++i) p = vm::frame_viewmodel(row, in, st, kDt);
  CHECK(std::fabs(p.shift_m[1] + row.lower_m) < 1e-9);
  CHECK(std::fabs(p.turn[1] + row.lower_pitch) < 1e-9);
}

TEST(comfort_zero_holds_it_still) {
  const vm::PlaceRow row = vm::modern_row();
  vm::PlaceState st;
  vm::PlaceInput in;
  in.bob_m = 0.04;
  in.ground_speed = 6;
  in.stride_s = 0.3;
  for (int i = 0; i < 30; ++i) {
    in.look_yaw += 0.05;
    in.shot = i == 10 ? 1 : 0;
    const vm::Placement p = vm::frame_viewmodel(row, in, st, kDt, 0.0);
    for (int k = 0; k < 3; ++k) CHECK(p.shift_m[k] == 0 && p.turn[k] == 0);
  }
}

TEST(the_same_frames_give_the_same_bits) {
  auto run = [] {
    const vm::PlaceRow row = vm::modern_row();
    vm::PlaceState st;
    vm::PlaceInput in;
    vm::Placement p;
    for (int i = 0; i < 200; ++i) {
      in.look_yaw = 0.3 * std::sin(i * 0.05);
      in.ground_speed = 5;
      in.stride_s = i * kDt;
      in.bob_m = 0.02 * std::sin(i * 0.3);
      in.shot = i % 50 == 0 ? 1 : 0;
      p = vm::frame_viewmodel(row, in, st, kDt);
    }
    return p;
  };
  const vm::Placement a = run(), b = run();
  for (int k = 0; k < 12; ++k) CHECK_BITS(a.m[k], b.m[k]);
}

TEST(a_held_point_lands_on_the_same_screen_ray_in_the_world) {
  vm::PlaceState st;
  const vm::Placement p = vm::frame_viewmodel(vm::classic_row(), vm::PlaceInput{}, st, kDt);
  ge::camera::Pose view;
  view.eye = {1, 2, 3};
  view.yaw = 0.3;
  view.pitch = -0.1;
  view.fov_y = 1.2;
  const float tip[3] = {0.02f, 0.01f, 0.4f};
  float in_view[3];
  vm::place_point(p, tip, in_view);
  // Drawn through its own narrower lens, the tip is on screen where (x, y) / (z tan(gun / 2)) says;
  // the world point, seen through the view's lens, is at the same place on the screen.
  const double gun = 0.9;
  double w[3];
  vm::held_to_world(p, gun, view, tip, w);
  double eye_rel[3] = {w[0] - view.eye.x, w[1] - view.eye.y, w[2] - view.eye.z};
  // Back into the view's frame through the yaw and pitch (the inverse of view_to_world).
  const ge::core::Vec3 fwd = ge::camera::look_forward(view.yaw, view.pitch);
  const double z = eye_rel[0] * fwd.x + eye_rel[1] * fwd.y + eye_rel[2] * fwd.z;
  CHECK(std::fabs(z - in_view[2]) < 1e-5);  // the same depth
  const double screen_gun = in_view[0] / (in_view[2] * std::tan(gun / 2));
  // The world point's offset across, measured along the view's right.
  const double len_sq = eye_rel[0] * eye_rel[0] + eye_rel[1] * eye_rel[1] + eye_rel[2] * eye_rel[2];
  const double across = std::sqrt(len_sq - z * z);
  const double screen_view = across / (z * std::tan(view.fov_y / 2));
  const double gun_across =
      std::sqrt(double(in_view[0]) * in_view[0] + double(in_view[1]) * in_view[1]);
  CHECK(std::fabs(screen_view - gun_across / (in_view[2] * std::tan(gun / 2))) < 1e-5);
  CHECK(screen_gun > 0);
  // The view's own lens moves nothing: the world point is the placed point.
  double same[3], plain[3];
  vm::held_to_world(p, 0, view, tip, same);
  vm::view_to_world(view, in_view, plain);
  for (int k = 0; k < 3; ++k) CHECK(same[k] == plain[k]);
  // The muzzle's direction comes out unit.
  const float dir[3] = {0, 0, 1};
  double d[3];
  vm::held_dir_to_world(p, gun, view, tip, dir, d);
  CHECK(std::fabs(d[0] * d[0] + d[1] * d[1] + d[2] * d[2] - 1) < 1e-9);
}
