// The camera stack (engine/camera/camera.hpp): rungs found by name, a cut is instant, a blend and
// an inertialized change are continuous where a cut is not, every rig is kept warm, a teleport
// drops a transition and snaps, an explosion shakes, the settings' blend 0 cuts, and the player's
// fov reaches a first-person rig.
//
// ge:summary Tests: the camera stack's rungs, edits, warm rigs, teleports, shake and settings
// ge:keywords test camera stack rung cut blend inertialize warm teleport explosion settings fov
// ge:skill camera testing
// ge:test tests/camera_camera_test.cpp
#include <cmath>
#include <string>
#include <vector>

#include "camera/camera.hpp"
#include "camera/ladders.hpp"
#include "camera/scenarios.hpp"
#include "check.hpp"
#include "core/math.hpp"

namespace cam = ge::camera;

namespace {

constexpr double kHz = 60;

cam::Camera reference() {
  const auto l = cam::reference_ladder();
  return cam::make_camera(l.data(), static_cast<int>(l.size()), 1);
}

// The largest frame-to-frame jump of the eye's velocity against the subject's, m/s, over frames
// `from`..`to` of a carve with a change of rung chase -> helmet by `edit` at frame 120.
double worst_jerk(cam::Edit edit, double duration) {
  const cam::Scenario* sc = cam::find_camera_scenario("carve");
  const cam::World w = cam::scenario_world(*sc);
  cam::ScriptRun r = cam::start_script(*sc);
  cam::Camera c = reference();
  c.rung = cam::find_rung(c, "chase");
  std::vector<cam::Vec3> rel;
  for (int i = 1; i <= 240; ++i) {
    const cam::Subject s = cam::advance_to(r, i / kHz);
    if (i == 120) cam::set_rung(c, cam::find_rung(c, "helmet"), edit, duration);
    const cam::Pose p = cam::update(c, s, w, 1 / kHz);
    rel.push_back({p.eye.x - s.pos.x, p.eye.y - s.pos.y, p.eye.z - s.pos.z});
  }
  double worst = 0;
  for (size_t i = 2; i < rel.size(); ++i) {
    const double ax = (rel[i].x - 2 * rel[i - 1].x + rel[i - 2].x) * kHz;
    const double ay = (rel[i].y - 2 * rel[i - 1].y + rel[i - 2].y) * kHz;
    const double az = (rel[i].z - 2 * rel[i - 1].z + rel[i - 2].z) * kHz;
    const double j = ge::core::hypot3(ax, ay, az);
    if (i > 100 && j > worst) worst = j;
  }
  return worst;
}

}  // namespace

TEST(rungs_are_found_by_name) {
  const cam::Camera c = reference();
  CHECK(cam::find_rung(c, "eyes") == 0);
  CHECK(cam::find_rung(c, "trackside") == 7);
  CHECK(cam::find_rung(c, "nowhere") == -1);
}

TEST(a_blend_and_an_inertialized_change_are_smooth_where_a_cut_is_not) {
  const double cut = worst_jerk(cam::Edit::cut, 0.4);
  const double blend = worst_jerk(cam::Edit::blend, 0.4);
  const double inert = worst_jerk(cam::Edit::inertialize, 0.4);
  CHECK_MSG(blend < cut / 4 && inert < cut / 4,
            "cut " + std::to_string(cut) + ", blend " + std::to_string(blend) + ", inertialize " +
                std::to_string(inert) + " (m/s of velocity jump a frame)");
}

TEST(a_change_lands_on_the_new_rig) {
  const cam::Scenario* sc = cam::find_camera_scenario("walk");
  const cam::World w = cam::scenario_world(*sc);
  cam::ScriptRun r = cam::start_script(*sc);
  cam::Camera c = reference();
  c.rung = cam::find_rung(c, "chase");
  cam::Pose p{};
  for (int i = 1; i <= 120; ++i) {
    const cam::Subject s = cam::advance_to(r, i / kHz);
    if (i == 30) cam::set_rung(c, cam::find_rung(c, "far"), cam::Edit::blend, 0.5);
    p = cam::update(c, s, w, 1 / kHz);
    if (i == 45) CHECK(cam::in_transition(c));
  }
  CHECK(!cam::in_transition(c));
  // At rest the camera's own frame is the far rig's (both kept in the stack's memory).
  const cam::Local want = c.last[cam::find_rung(c, "far")];
  CHECK(std::fabs(c.out.off.z - want.off.z) < 1e-12 && c.out.fov == want.fov);
  (void)p;
}

TEST(blend_zero_in_the_settings_makes_every_change_a_cut) {
  cam::Camera c = reference();
  c.settings.blend = 0;
  cam::Subject s;
  const cam::World w{};
  cam::update(c, s, w, 1 / kHz);
  cam::set_rung(c, cam::find_rung(c, "far"), cam::Edit::blend, 0.5);
  CHECK(!cam::in_transition(c));
}

TEST(a_teleport_drops_a_transition) {
  cam::Camera c = reference();
  c.rung = 1;
  cam::Subject s;
  const cam::World w{};
  cam::update(c, s, w, 1 / kHz);
  cam::set_rung(c, 2, cam::Edit::blend, 1);
  cam::update(c, s, w, 1 / kHz);
  CHECK(cam::in_transition(c));
  cam::add_moment(s, cam::Moment::teleport, 0);
  cam::update(c, s, w, 1 / kHz);
  CHECK(!cam::in_transition(c));
}

TEST(an_explosion_shakes_the_picture) {
  cam::Camera c = reference();
  c.rung = cam::find_rung(c, "eyes");
  cam::Subject s;
  const cam::World w{};
  const cam::Pose still = cam::update(c, s, w, 1 / kHz);
  cam::add_moment(s, cam::Moment::explosion, 1);
  cam::update(c, s, w, 1 / kHz);
  s.event_count = 0;
  double most = 0;
  for (int i = 0; i < 30; ++i) {
    const cam::Pose p = cam::update(c, s, w, 1 / kHz);
    most = std::fabs(p.roll - still.roll) > most ? std::fabs(p.roll - still.roll) : most;
  }
  CHECK(most > 0.005);
}

TEST(the_players_fov_reaches_a_first_person_rig) {
  cam::Camera c = reference();
  c.rung = cam::find_rung(c, "eyes");
  c.settings.fov_h = ge::core::kPi / 2;
  cam::Subject s;
  const cam::Pose p = cam::update(c, s, cam::World{}, 1 / kHz);
  CHECK(std::fabs(p.fov_y - cam::fov_y_of(ge::core::kPi / 2)) < 1e-12);
}
