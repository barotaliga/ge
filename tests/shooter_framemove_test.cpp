// THE ORIGINAL MOVE HELD TO ITS RECORDINGS: every scenario of tests/golden/shooter_framemove.txt
// (running, looking, jumping, walls, stairs, ramps, ledges, swimming, the water jump, slime and
// lava, at 72, 20 and 10 steps a second) replayed through `frame_think`, `frame_program_move` and
// `frame_move` step by step over its scene, our own state carried, each step within the tolerance
// (plugins/shooter/frame_recording.hpp) with the discrete fields equal. A failure prints the
// scenario's first step out of tolerance and what differs. Then the two hosts on the original
// move: the single-player clock moves a player once a frame by its newest command (the recorded
// run, step for step), the multiplayer clock once a command by the command's own length, the
// engine jumping for a multiplayer program and never for a single-player one.
//
// ge:summary Tests: the original player move against every recorded step, within tolerance
// ge:keywords test framemove original-move recording vectors parity walk-move swim water-jump
// ge:keywords sv_playermove single-player-clock latest-command program-jumps
// ge:skill shooter testing
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "check.hpp"
#include "shooter/frame.hpp"
#include "shooter/frame_recording.hpp"

namespace sh = ge::shooter;

namespace {

std::string read_file(const char* path) {
  std::ifstream f(ge::test::root_path(path));
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

const sh::Recording& scenes() {
  static const sh::Recording rec = [] {
    sh::Recording r;
    std::string error;
    if (!sh::parse_recording(read_file("tests/golden/shooter_recording.txt"), r, error))
      std::fprintf(stderr, "tests/golden/shooter_recording.txt: %s\n", error.c_str());
    return r;
  }();
  return rec;
}

const sh::FrameRecording& recording() {
  static const sh::FrameRecording rec = [] {
    sh::FrameRecording r;
    std::string error;
    if (!sh::parse_frame_recording(read_file("tests/golden/shooter_framemove.txt"), r, error))
      std::fprintf(stderr, "tests/golden/shooter_framemove.txt: %s\n", error.c_str());
    return r;
  }();
  return rec;
}

const sh::RecordedScene& flat() { return scenes().scenes[0]; }

// A server on the flat scene with one player stood as the recordings stand it.
struct Host {
  sh::ModelData world = sh::build_brush_model(flat().brushes);
  sh::Server sv;

  explicit Host(bool single_player_clock, bool program_jumps = false) {
    sh::ServerSettings set;
    set.maxclients = 1;
    set.max_edicts = 8;
    set.single_player_clock = single_player_clock;
    set.mintic = static_cast<double>(1.0f / 72);
    set.playermove = sh::PlayerMove::original;
    set.program_jumps = program_jumps;
    sh::create_server(sv, set, {world.model()}, sh::Program{});
    sh::connect_client(sv, 0, false, true);
    sh::stand_frame_player(sv, 1, {0, 0, 24}, {});
  }
  const sh::Edict& player() const { return sv.edicts[1]; }
};

sh::Packet packet(uint32_t sequence, int16_t forward, uint8_t buttons = 0, uint8_t msec = 14) {
  sh::Packet p;
  p.sequence = sequence;
  for (sh::MoveCmd& c : p.cmds) {
    c.msec = msec;
    c.forward = forward;
    c.buttons = buttons;
  }
  return p;
}

}  // namespace

TEST(framemove_recording_parses_every_scenario) {
  const sh::FrameRecording& rec = recording();
  CHECK(rec.scenarios.size() == 35);
  size_t lines = 0;
  for (const auto& s : rec.scenarios) lines += s.lines.size();
  CHECK_MSG(lines > 5000, std::to_string(lines));
  CHECK(rec.scenarios[0].name == "run_and_stop" && rec.scenarios[0].scene == "flat");
  CHECK(rec.scenarios[1].dt == static_cast<double>(0.05f));
}

TEST(a_malformed_framemove_recording_names_its_line) {
  sh::FrameRecording r;
  std::string error;
  CHECK(!sh::parse_frame_recording("scene a\nscenario b\nsettings skate=1\n", r, error));
  CHECK_MSG(error.find("line 3") != std::string::npos, error);
  CHECK(!sh::parse_frame_recording("frame 0 view 0 0\n", r, error));
}

TEST(every_recorded_step_within_tolerance) {
  const sh::FrameRecording& rec = recording();
  int lines = 0, passed = 0;
  for (const sh::FrameScenario& f : rec.scenarios) {
    const sh::FrameReplay r = sh::replay_frames(scenes(), f);
    CHECK_MSG(r.error.empty(), r.error);
    lines += static_cast<int>(f.lines.size());
    passed += static_cast<int>(r.lines.size()) - r.failed;
    std::printf(
        "%-18s %4zu steps, %3d out; worst position %.3g (step %d), velocity %.3g (step %d)\n",
        f.name.c_str(), r.lines.size(), r.failed, r.max_position_error, r.worst_position_line,
        r.max_velocity_error, r.worst_velocity_line);
    for (size_t i = 0; i < r.lines.size(); i++) {
      const sh::FrameCheck& c = r.lines[i];
      if (c.ok) continue;
      const sh::FrameState& w = f.lines[i].after;
      char buf[400];
      std::snprintf(buf, sizeof buf,
                    "%s step %d out of tolerance (%s): ours %g %g %g v %g %g %g clip %d, recorded "
                    "%g %g %g v %g %g %g clip %d; next: read plugins/shooter/framemove.cpp",
                    f.name.c_str(), c.index, c.mismatch.c_str(), c.ours.origin.x, c.ours.origin.y,
                    c.ours.origin.z, c.ours.velocity.x, c.ours.velocity.y, c.ours.velocity.z,
                    c.ours.clip, w.origin.x, w.origin.y, w.origin.z, w.velocity.x, w.velocity.y,
                    w.velocity.z, w.clip);
      CHECK_MSG(false, std::string(buf));
      break;
    }
  }
  std::printf("%d of %d steps within tolerance\n", passed, lines);
  CHECK(passed == lines);
}

TEST(the_single_player_clock_moves_a_player_once_a_frame_by_its_newest_command) {
  // run_and_stop's first 40 steps: one packet before the first frame, none after (the command
  // persists), the player moved in the physics frame exactly as the recording.
  const sh::FrameScenario& run = recording().scenarios[0];
  Host h(true);
  for (int i = 0; i < 40; i++) {
    const sh::Packet p = packet(1, 400);
    sh::server_frame(h.sv, &p, i == 0 ? 1 : 0);
    const sh::FrameState& want = run.lines[static_cast<size_t>(i)].after;
    const sh::Edict& ed = h.player();
    CHECK_MSG(std::fabs(ed.origin.x - want.origin.x) <= sh::kPositionTolerance &&
                  ed.origin.z == want.origin.z,
              "step " + std::to_string(i) + ": x " + std::to_string(ed.origin.x) + " recorded " +
                  std::to_string(want.origin.x));
    CHECK(std::fabs(ed.velocity.x - want.velocity.x) <= sh::kVelocityTolerance);
    CHECK(((ed.flags & sh::kFlagOnGround) != 0) == want.onground);
  }
  CHECK(h.sv.counts.commands == 0);  // no command moved it by itself
}

TEST(the_multiplayer_clock_steps_the_original_move_per_command) {
  // Each command is one original step of its own length: the same as stepping by hand.
  Host h(false);
  Host by_hand(false);
  for (uint32_t i = 1; i <= 30; i++) {
    sh::run_packet(h.sv, packet(i, 400));
    const sh::FrameMoveSettings set = sh::frame_settings(by_hand.sv.set);
    sh::FrameCommand c;
    c.forward = 400;
    sh::frame_think(by_hand.sv, 1, c, set, 0.014);
    sh::frame_program_move(by_hand.sv, 1, false, 0.014);
    sh::frame_move(by_hand.sv, 1, set, 0.014);
  }
  CHECK(h.player().origin.x == by_hand.player().origin.x);
  CHECK(h.player().velocity.x == by_hand.player().velocity.x);
  CHECK(h.player().origin.x > 100);
  CHECK(h.sv.counts.commands == 30);
}

TEST(the_engine_jumps_only_for_a_program_that_does_not) {
  for (const bool program_jumps : {false, true}) {
    Host h(false, program_jumps);
    for (uint32_t i = 1; i <= 10; i++) sh::run_packet(h.sv, packet(i, 0));  // land
    CHECK((h.player().flags & sh::kFlagOnGround) != 0);
    sh::run_packet(h.sv, packet(11, 0, sh::kButtonJump));
    CHECK_MSG((h.player().velocity.z > 0) == !program_jumps, std::to_string(h.player().velocity.z));
  }
}
