// THE ENGINE MOVER'S CLASSIC ROW HELD TO THE RECORDINGS: every recorded scenario
// (tests/golden/shooter_recording.txt) run through `engine/move` and `engine/water/swim` on the
// classic rows over the scene's brush model (plugins/shooter/row_recording.hpp). Every line of
// every covered scenario within the recordings' own tolerance, one command at a time from the
// recorded state and as a whole run, with on-ground, the ground and the water equal; the
// uncovered scenarios each left out for a reason the header names.
//
// ge:summary Tests: the engine mover's classic row against every recorded line, step and run
// ge:keywords test row recording parity tolerance engine-mover replay_row row_covers step run
// ge:skill shooter move testing
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "check.hpp"
#include "shooter/row_recording.hpp"

namespace sh = ge::shooter;

namespace {

std::string read_file(const char* rel) {
  std::ifstream f(ge::test::root_path(rel));
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

const sh::Recording& recording() {
  static const sh::Recording rec = [] {
    sh::Recording r;
    std::string error;
    if (!sh::parse_recording(read_file("tests/golden/shooter_recording.txt"), r, error))
      std::fprintf(stderr, "tests/golden/shooter_recording.txt: %s\n", error.c_str());
    return r;
  }();
  return rec;
}

void first_out(const sh::ReplayResult& r, const char* what) {
  for (const sh::LineCheck& c : r.lines) {
    if (c.ok) continue;
    std::printf(
        "    %s first out: cmd %d, position off %.4g, velocity off %.4g; ours %.6g %.6g %.6g"
        " v %.6g %.6g %.6g; %s\n",
        what, c.index, c.position_error, c.velocity_error, c.ours.origin.x, c.ours.origin.y,
        c.ours.origin.z, c.ours.velocity.x, c.ours.velocity.y, c.ours.velocity.z,
        c.mismatch.c_str());
    return;
  }
}

}  // namespace

TEST(every_covered_line_within_the_recordings_tolerance) {
  const sh::Recording& rec = recording();
  CHECK(rec.scenarios.size() == 35);
  int covered = 0, lines = 0, failed = 0;
  for (const sh::RecordedScenario& sc : rec.scenarios) {
    const sh::RowReplay r = sh::replay_row(rec, sc);
    const bool held = sh::row_covers(sc);
    std::printf(
        "%-26s %s step: %3d out, worst %.3g u, %.3g u/s; run: %3d out, worst %.3g u, "
        "%.3g u/s\n",
        sc.name.c_str(), held ? "held" : "    ", r.step.failed, r.step.max_position_error,
        r.step.max_velocity_error, r.run.failed, r.run.max_position_error,
        r.run.max_velocity_error);
    if (!held) continue;
    covered++;
    lines += static_cast<int>(r.run.lines.size());
    if (r.step.failed + r.run.failed == 0) continue;
    failed++;
    first_out(r.step, "step");
    first_out(r.run, "run");
  }
  // Every scenario but the old spectator, the two unsticks and the four settings the row does
  // not carry (row_recording.hpp).
  CHECK(covered == 28);
  CHECK(lines > 3000);
  CHECK_MSG(failed == 0, std::to_string(failed) +
                             " covered scenarios leave the recordings' tolerance on the row: read "
                             "each one's first line out above, then plugins/shooter/row.cpp and "
                             "the knob of engine/move/step.cpp it reaches (`make lab LAB=pmove` "
                             "charts them)");
}

TEST(the_uncovered_settings_are_named) {
  const sh::Recording& rec = recording();
  for (const sh::RecordedScenario& sc : rec.scenarios) {
    if (sh::row_covers(sc)) continue;
    const bool known = sc.start.type == sh::MoveType::old_spectator ||
                       sc.name.rfind("nudge", 0) == 0 || sc.settings.ktjump != 1 ||
                       sc.settings.slidefix || sc.settings.airstep;
    CHECK_MSG(known, sc.name + " is left out of the row with no reason row_recording.hpp names");
  }
}
