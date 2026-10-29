// THE TOSS RULES HELD TO THEIR RECORDINGS: every flight of tests/golden/shooter_toss_recording.txt
// (a bouncing grenade at 13 ms and 100 ms frames, a missile, a tossed gib, a flying nail against
// the floor and the wall of one scene, measured from the classic loop) replayed through
// `physics_toss` frame by frame, our own state carried, each frame within the tolerance
// (plugins/shooter/toss_recording.hpp) with on the ground and hit equal. A failure prints the
// flight's first frame out of tolerance and what differs.
//
// ge:summary Tests: thrown entities against every recorded frame, within tolerance
// ge:keywords test toss bounce flymissile fly gib grenade rocket nail recording vectors parity
// ge:skill shooter testing
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "check.hpp"
#include "shooter/entity.hpp"
#include "shooter/toss_recording.hpp"

namespace sh = ge::shooter;

namespace {

const sh::TossRecording& recording() {
  static const sh::TossRecording rec = [] {
    std::ifstream f(ge::test::root_path("tests/golden/shooter_toss_recording.txt"));
    std::stringstream ss;
    ss << f.rdbuf();
    sh::TossRecording r;
    std::string error;
    if (!sh::parse_toss_recording(ss.str(), r, error))
      std::fprintf(stderr, "tests/golden/shooter_toss_recording.txt: %s\n", error.c_str());
    return r;
  }();
  return rec;
}

}  // namespace

TEST(toss_recording_parses_every_flight) {
  const sh::TossRecording& rec = recording();
  CHECK(rec.scenes.size() == 1);
  CHECK(rec.flights.size() == 5);
  size_t frames = 0;
  for (const auto& f : rec.flights) frames += f.frames.size();
  CHECK(frames == 454);
  CHECK(rec.flights[0].movetype == sh::kMovetypeBounce);
  CHECK(rec.flights[4].frametime == 0.1);
}

TEST(a_malformed_toss_recording_names_its_line) {
  sh::TossRecording r;
  std::string error;
  CHECK(!sh::parse_toss_recording("scene a\nend\ntoss g movetype skate origin 0 0 0\n", r, error));
  CHECK_MSG(error.find("line 3") != std::string::npos, error);
  CHECK(!sh::parse_toss_recording("frame 0 -> origin 0 0 0\n", r, error));
}

TEST(every_recorded_frame_within_tolerance) {
  const sh::TossRecording& rec = recording();
  int frames = 0, passed = 0, exact = 0;
  for (const sh::TossFlight& f : rec.flights) {
    const sh::TossReplay r = sh::replay_toss(rec, f);
    frames += static_cast<int>(r.frames.size());
    passed += static_cast<int>(r.frames.size()) - r.failed;
    for (const sh::TossCheck& c : r.frames)
      if (c.ok && c.position_error == 0 && c.velocity_error == 0) exact++;
    std::printf(
        "%-20s %4zu frames, %3d out; worst position %.3g (frame %d), velocity %.3g "
        "(frame %d), trace %.3g\n",
        f.name.c_str(), r.frames.size(), r.failed, r.max_position_error, r.worst_position_frame,
        r.max_velocity_error, r.worst_velocity_frame, r.max_trace_error);
    for (const sh::TossCheck& c : r.frames) {
      if (c.ok) continue;
      const sh::TossFrame& w = f.frames[static_cast<size_t>(c.index)];
      CHECK_MSG(false, f.name + " frame " + std::to_string(c.index) + " out of tolerance (" +
                           c.mismatch + "): ours z " + std::to_string(c.ours.origin.z) + " vz " +
                           std::to_string(c.ours.velocity.z) + ", recorded z " +
                           std::to_string(w.origin.z) + " vz " + std::to_string(w.velocity.z) +
                           "; next: read the step in plugins/shooter/physics.cpp's physics_toss");
      break;
    }
  }
  std::printf("%d of %d frames within tolerance, %d exact\n", passed, frames, exact);
  CHECK(passed == frames);
}
