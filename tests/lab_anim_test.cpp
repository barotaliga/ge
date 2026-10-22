// engine/lab/anim: a smooth swing has no faults; a planted pop, stretch and clip are each found
// on their joint at their frame; the box wire, the frame spread, the filmstrip's size and the
// motion series' columns.
//
// ge:summary Test: the pose track's measures find planted faults and nothing on a smooth swing
// ge:keywords test anim pose track pop snap stretch clip filmstrip wireframe
// ge:skill testing debug
#include <cmath>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/libm/libm.hpp"
#include "lab/anim.hpp"

namespace gl = ge::lab;
using ge::core::Vec3;

namespace {

// A three-joint arm (shoulder, elbow, hand, bones of 0.3 m) swinging at 1 Hz, 120 frames a
// second for two seconds, over a floor at y = -1.
gl::PoseTrack swing() {
  gl::PoseTrack t;
  t.rig.joints = {"shoulder", "elbow", "hand"};
  t.rig.parent = {-1, 0, 1};
  t.rig.side = {0, 1, 1};
  t.rig.ends = {2};
  t.dt = 1.0 / 120;
  t.has_floor = true;
  t.floor = -1;
  for (int f = 0; f < 240; f++) {
    const double a = 0.6 * ge::core::libm::sin(2 * 3.14159265358979 * f / 120.0);
    const double b = a + 0.5;
    const Vec3 e{0, -0.3 * ge::core::libm::cos(a), 0.3 * ge::core::libm::sin(a)};
    const Vec3 h{0, e.y - 0.3 * ge::core::libm::cos(b), e.z + 0.3 * ge::core::libm::sin(b)};
    const Vec3 pose[] = {{0, 0, 0}, e, h};
    t.add_frame(pose);
  }
  return t;
}

bool has(const std::vector<gl::AnimFinding>& found, const std::string& kind, size_t joint,
         size_t first, size_t last) {
  for (const gl::AnimFinding& a : found) {
    if (a.kind == kind && a.joint == joint && a.frame >= first && a.frame <= last) return true;
  }
  return false;
}

}  // namespace

TEST(a_smooth_swing_has_no_faults) {
  const gl::PoseTrack t = swing();
  CHECK(t.frames() == 240);
  const gl::Motion m = gl::measure_motion(t);
  CHECK(gl::anim_findings(t, m).empty());
  // The elbow's angle is 180 less the swing's 0.5 rad bend, every frame.
  CHECK(std::fabs(m.angle[100 * 3 + 1] - (180 - 0.5 * 180 / 3.14159265358979)) < 1e-6);
  CHECK(std::isnan(m.angle[100 * 3 + 0]) && std::isnan(m.angle[100 * 3 + 2]));
  CHECK(std::fabs(m.stretch[50 * 3 + 2]) < 1e-9);
}

TEST(a_planted_pop_stretch_and_clip_are_found) {
  gl::PoseTrack t = swing();
  t.at(60, 2).x += 0.05;                // the hand jumps for one frame
  for (size_t f = 120; f < 140; f++) {  // the forearm grows 10% for 20 frames
    const Vec3 e = t.at(f, 1);
    Vec3& h = t.at(f, 2);
    h = Vec3{e.x + (h.x - e.x) * 1.1, e.y + (h.y - e.y) * 1.1, e.z + (h.z - e.z) * 1.1};
  }
  for (size_t f = 200; f < 210; f++) t.at(f, 2).y = -1.1;  // through the floor
  const std::vector<gl::AnimFinding> found = gl::anim_findings(t, gl::measure_motion(t));
  CHECK(has(found, "pop", 2, 60, 61));
  CHECK(!has(found, "snap", 2, 55, 66));  // a pop's snap is the pop's
  CHECK(has(found, "stretch", 2, 120, 139));
  CHECK(has(found, "clip", 2, 200, 209));
  CHECK(!has(found, "pop", 0, 0, 239));  // the shoulder never moved
  for (const gl::AnimFinding& a : found) {
    CHECK(a.first <= a.frame && a.frame <= a.last);
    CHECK(a.note.find(t.rig.joints[a.joint]) != std::string::npos);
  }
}

TEST(a_box_has_twelve_edges_of_its_size) {
  std::vector<gl::WireSeg> wire;
  const double size[3] = {1, 2, 3};
  gl::add_box_wire(wire, Vec3{0, 0, 0}, ge::core::identity(), size);
  CHECK(wire.size() == 12);
  double total = 0;
  for (const gl::WireSeg& s : wire) {
    total += std::sqrt((s.b.x - s.a.x) * (s.b.x - s.a.x) + (s.b.y - s.a.y) * (s.b.y - s.a.y) +
                       (s.b.z - s.a.z) * (s.b.z - s.a.z));
  }
  CHECK(std::fabs(total - 4 * (1 + 2 + 3)) < 1e-12);
}

TEST(the_views_and_the_series) {
  const gl::PoseTrack t = swing();
  const std::vector<size_t> frames = gl::spread_frames(t, 5);
  CHECK(frames.size() == 5 && frames.front() == 0 && frames.back() == 239);
  gl::StripSpec spec;
  spec.frames = frames;
  spec.columns = 3;
  spec.panel = 100;
  const gl::Picture strip = gl::filmstrip(t, spec);
  CHECK(strip.width == 300 && strip.height == 2 * (100 + 12));
  const gl::Picture onion = gl::onion_skin(t, gl::AnimView::behind, frames, 200);
  CHECK(onion.width == 200);
  const gl::Series s = gl::motion_series(t, gl::measure_motion(t));
  CHECK(s.rows() == 240);
  CHECK(s.columns.size() == 1 + 3 * 2 + 1);  // t_s, each joint's accel and jolt, the elbow's angle
  CHECK(s.find("elbow.angle") != nullptr && s.unit("elbow.angle") == "deg");
}
