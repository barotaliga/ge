// The simple lenses (engine/camera/lenses.hpp): the orbit circles the subject at its radius and
// over the ground, the planted lens stands still while it pans after the subject and zooms to
// hold its size, and the free lens is the subject's own eye.
//
// ge:summary Tests: the orbit's circle, the planted lens's pan and zoom, the free lens
// ge:keywords test camera orbit planted trackside zoom free
// ge:skill camera testing
// ge:test tests/camera_lenses_test.cpp
#include <cmath>

#include "camera/lenses.hpp"
#include "camera/scenarios.hpp"
#include "check.hpp"
#include "core/math.hpp"

namespace cam = ge::camera;

TEST(the_orbit_circles_at_its_radius) {
  const cam::OrbitRow row;
  cam::OrbitState st;
  cam::Subject s;
  const cam::World flat{};
  for (int i = 0; i < 100; ++i) {
    const cam::Pose p = cam::frame_orbit(row, s, flat, st, 0.1);
    CHECK(std::fabs(ge::core::hypot(p.eye.x, p.eye.z) - row.radius_m) < 1e-9);
  }
}

TEST(the_planted_lens_stands_still_pans_and_zooms_in_as_the_subject_goes_away) {
  const cam::Scenario* sc = cam::find_camera_scenario("walk");
  cam::ScriptRun r = cam::start_script(*sc);
  cam::PlantedRow row;
  row.eye = {6, 2, 0};
  row.hold_m = 3;
  cam::PlantedState st;
  cam::Pose first{}, last{};
  for (int i = 1; i <= 360; ++i) {
    const cam::Subject s = cam::advance_to(r, i / 60.0);
    const cam::Pose p = cam::frame_planted(row, s, st, 1.0 / 60);
    CHECK(p.eye.x == 6 && p.eye.y == 2 && p.eye.z == 0);
    if (i == 1) first = p;
    last = p;
  }
  CHECK(last.fov_y < first.fov_y);
  CHECK(std::fabs(ge::core::angle_diff(first.yaw, last.yaw)) > 0.5);
}

TEST(the_free_lens_is_the_subjects_eye) {
  cam::Subject s;
  s.pos = {1, 2, 3};
  s.look_yaw = 0.4;
  s.look_pitch = -0.2;
  const cam::Pose p = cam::frame_free(cam::FreeRow{}, s);
  CHECK(p.eye.x == 1 && p.eye.y == 2 && p.eye.z == 3 && p.yaw == 0.4 && p.pitch == -0.2);
}
