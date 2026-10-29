// THE CLASSIC MOVE HELD TO ITS RECORDINGS: every scenario of tests/golden/shooter_recording.txt
// (the classic move measured over hand-built worlds) replayed through `player_move` from its
// start, our own state carried from line to line, each command's result within the recordings'
// tolerance (plugins/shooter/recording.hpp) with every discrete field equal. A failure prints the
// scenario's first line out of tolerance and what differs.
//
// ge:summary Tests: the classic player move against every recorded line, within tolerance
// ge:keywords test pmove parity recording vectors tolerance replay parse_recording
// ge:skill shooter testing
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "check.hpp"
#include "shooter/recording.hpp"

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

}  // namespace

TEST(recording_parses_every_scenario) {
  const sh::Recording& rec = recording();
  CHECK(rec.scenes.size() == 10);
  CHECK(rec.scenarios.size() == 35);
  size_t lines = 0;
  for (const auto& sc : rec.scenarios) lines += sc.lines.size();
  CHECK(lines == 4033);
  // The settings the scenarios name reach the move.
  for (const auto& sc : rec.scenarios) {
    if (sc.name == "strafe_jump_speedcap") CHECK(sc.settings.bunnyspeedcap == 1);
    if (sc.name == "stairs_jump_airstep") CHECK(sc.settings.airstep && sc.settings.pground);
    if (sc.name == "ramp_down_jump_ktjump0") CHECK(sc.settings.ktjump == 0);
    if (sc.name == "player_box") CHECK(sc.boxes.size() == 1);
  }
}

TEST(a_malformed_recording_names_its_line) {
  sh::Recording r;
  std::string error;
  CHECK(!sh::parse_recording("scene a\nbrush solid\nplane 1 0 0\nend\n", r, error));
  CHECK_MSG(error.find("line 3") != std::string::npos, error);
  CHECK(!sh::parse_recording("scenario s\nend\n", r, error));
  CHECK(sh::parse_recording("# nothing\n\n", r, error));
}

TEST(every_recorded_line_within_tolerance) {
  const sh::Recording& rec = recording();
  int failed_scenarios = 0, lines = 0, passed = 0, exact = 0;
  for (const sh::RecordedScenario& sc : rec.scenarios) {
    const sh::ReplayResult r = sh::replay(rec, sc);
    lines += static_cast<int>(r.lines.size());
    passed += static_cast<int>(r.lines.size()) - r.failed;
    for (const sh::LineCheck& c : r.lines)
      if (c.ok && c.position_error == 0 && c.velocity_error == 0) exact++;
    std::printf("%-26s %4zu lines, %3d out; worst position %.3g (cmd %d), velocity %.3g (cmd %d)\n",
                sc.name.c_str(), r.lines.size(), r.failed, r.max_position_error,
                r.worst_position_line, r.max_velocity_error, r.worst_velocity_line);
    if (r.failed == 0) continue;
    failed_scenarios++;
    for (const sh::LineCheck& c : r.lines) {
      if (c.ok) continue;
      std::printf(
          "  first out: cmd %d, position off %.6g, velocity off %.6g; ours origin %.9g "
          "%.9g %.9g velocity %.9g %.9g %.9g; %s\n",
          c.index, c.position_error, c.velocity_error, c.ours.origin.x, c.ours.origin.y,
          c.ours.origin.z, c.ours.velocity.x, c.ours.velocity.y, c.ours.velocity.z,
          c.mismatch.c_str());
      break;
    }
  }
  std::printf("%d of %d recorded lines within tolerance, %d equal in every printed digit\n", passed,
              lines, exact);
  CHECK(lines == 4033);
  CHECK_MSG(failed_scenarios == 0,
            std::to_string(failed_scenarios) +
                " scenarios leave the recordings' tolerance: read each one's first line out "
                "above, then the step it names in plugins/shooter/pmove.cpp or slide.cpp "
                "(run `make lab LAB=pmove` for its charts)");
}
