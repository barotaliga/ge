// LAB `framemove`: does the original player move agree with its recordings, scenario by scenario,
// and where does each run go next to the predicted move's? Every recorded scenario
// (tests/golden/shooter_framemove.txt) is replayed through the original move
// (plugins/shooter/frame_recording.hpp) over its scene of the classic move's recording and
// measured step by step: one table of each scenario's worst position and velocity error against
// the tolerance and its first step out, a chart of those worsts, and for each drawn scenario its
// path from the side over the scene's brushes cut at y = 0, ours over the recording's.
//
// ge:summary Lab: the original move against its recordings, worst errors and paths per scenario
// ge:keywords lab framemove original-move parity recording tolerance trajectory side-view
// ge:keywords stairs ramp swim water-jump scenario sv_playermove
// ge:skill shooter labs
// ge:test tests/shooter_framemove_test.cpp
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "scene_chart.hpp"
#include "shooter/frame_recording.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
using ge::core::num::to_fixed;
using ge::shooter::labs::brush_outline;
using ge::shooter::labs::listed;
using ge::shooter::labs::shoot;
using ge::shooter::labs::upper;

bool read_text(gl::Lab& lab, const std::string& path, std::string& out) {
  std::ifstream in(path);
  if (!in) {
    lab.fail("framemove: cannot read " + path,
             "next: run the lab from the repository's root, or pass the file's path");
    return false;
  }
  std::stringstream ss;
  ss << in.rdbuf();
  out = ss.str();
  return true;
}

const sh::RecordedScene* scene_of(const sh::Recording& scenes, const std::string& name) {
  for (const sh::RecordedScene& s : scenes.scenes)
    if (s.name == name) return &s;
  return nullptr;
}

void draw(gl::Lab& lab, const sh::RecordedScene& scene, const sh::FrameScenario& sc,
          const sh::FrameReplay& r) {
  gl::Trace ours{"ours", {}, {}}, rec{"recorded", {}, {}};
  double x0 = sc.origin.x, x1 = x0, z0 = sc.origin.z, z1 = z0;
  auto add = [&](const sh::Vec3f& o, gl::Trace& t) {
    t.x.push_back(o.x);
    t.y.push_back(o.z);
    x0 = std::min(x0, static_cast<double>(o.x));
    x1 = std::max(x1, static_cast<double>(o.x));
    z0 = std::min(z0, static_cast<double>(o.z));
    z1 = std::max(z1, static_cast<double>(o.z));
  };
  add(sc.origin, ours);
  add(sc.origin, rec);
  for (size_t i = 0; i < r.lines.size(); i++) {
    add(r.lines[i].ours.origin, ours);
    add(sc.lines[i].after.origin, rec);
  }
  const double mx = std::max(16.0, (x1 - x0) * 0.08), mz = std::max(16.0, (z1 - z0) * 0.15);
  gl::Trace solid{"solid at y = 0", {}, {}}, liquid{"water, slime, lava at y = 0", {}, {}};
  for (const sh::Brush& b : scene.brushes)
    brush_outline(b, x0 - mx, x1 + mx, z0 - 24 - mz, z1 + mz,
                  b.contents == sh::kContentsSolid ? solid : liquid);
  const std::string worst = "worst position error " + to_fixed(r.max_position_error, 6) +
                            " u (step " + std::to_string(r.worst_position_line) + "), velocity " +
                            to_fixed(r.max_velocity_error, 6) + " u/s";
  gl::LineChart side;
  side.spikes = false;
  side.frame = {upper(sc.name) + " FROM THE SIDE", worst, "x (units)", "z, the origin (units)"};
  side.traces = {solid, rec, ours};
  if (!liquid.x.empty()) side.traces.insert(side.traces.begin() + 1, liquid);
  shoot(lab, sc.name + "-side", gl::line_chart(side),
        "From the side: the origin's height (24 above the feet) along x over " +
            std::to_string(r.lines.size()) + " steps of " + to_fixed(sc.dt, 4) +
            " s, ours and the recording's, over the scene's brushes cut at y = 0 (scene " +
            scene.name + "). " + worst + ".",
        "one path: ours over the recording's, so where they agree the recording's colour does "
        "not show; " +
            std::string(r.failed == 0 ? "every step is within tolerance"
                                      : std::to_string(r.failed) + " steps are OUT of tolerance"),
        sc.name);
}

void run(gl::Lab& lab) {
  std::string scenes_text, text, error;
  if (!read_text(lab, lab.arg("scenes"), scenes_text) ||
      !read_text(lab, lab.arg("recording"), text))
    return;
  sh::Recording scenes;
  sh::FrameRecording rec;
  if (!sh::parse_recording(scenes_text, scenes, error) ||
      !sh::parse_frame_recording(text, rec, error)) {
    lab.fail("framemove: " + error,
             "next: fix that line (formats: plugins/shooter/recording.hpp, frame_recording.hpp)");
    return;
  }
  gl::LabTable table{"every recorded scenario against the original move",
                     {"scenario", "scene", "dt s", "steps", "out", "worst position u", "at step",
                      "worst velocity u/s", "at step", "first out"},
                     {}};
  gl::Trace pos{"worst position error (u)", {}, {}}, vel{"worst velocity error / 5 (u/s)", {}, {}};
  int steps = 0, out = 0, scenarios_out = 0;
  for (size_t i = 0; i < rec.scenarios.size(); i++) {
    const sh::FrameScenario& sc = rec.scenarios[i];
    const sh::FrameReplay r = sh::replay_frames(scenes, sc);
    if (!r.error.empty()) {
      lab.fail("framemove: " + sc.name + ": " + r.error,
               "next: pass --scenes=tests/golden/shooter_recording.txt");
      continue;
    }
    steps += static_cast<int>(r.lines.size());
    out += r.failed;
    std::string first;
    for (const sh::FrameCheck& c : r.lines)
      if (!c.ok) {
        first = "step " + std::to_string(c.index) + (c.mismatch.empty() ? "" : ": " + c.mismatch);
        break;
      }
    if (r.failed > 0) {
      scenarios_out++;
      lab.fail("framemove: " + sc.name + " leaves the tolerance on " + std::to_string(r.failed) +
                   " steps (first " + first + ")",
               "next: run tests/shooter_framemove_test.cpp and read the step it names in "
               "plugins/shooter/framemove.cpp");
    }
    table.rows.push_back({sc.name, sc.scene, to_fixed(sc.dt, 4), std::to_string(r.lines.size()),
                          std::to_string(r.failed), to_fixed(r.max_position_error, 6),
                          std::to_string(r.worst_position_line), to_fixed(r.max_velocity_error, 6),
                          std::to_string(r.worst_velocity_line), first.empty() ? "-" : first});
    lab.metric(sc.name + ".worst-position", r.max_position_error, "u");
    lab.metric(sc.name + ".worst-velocity", r.max_velocity_error, "u/s");
    lab.metric(sc.name + ".steps-out", r.failed, "count");
    pos.x.push_back(static_cast<double>(i));
    pos.y.push_back(r.max_position_error);
    vel.x.push_back(static_cast<double>(i));
    vel.y.push_back(r.max_velocity_error / 5);
    if (listed(lab.arg("draw"), sc.name)) draw(lab, *scene_of(scenes, sc.scene), sc, r);
  }
  lab.metric("steps", steps, "count");
  lab.metric("steps-out", out, "count");
  lab.metric("scenarios-out", scenarios_out, "count");
  lab.table(table);

  gl::LineChart worst;
  worst.points = true;
  worst.spikes = false;
  worst.frame = {"THE ORIGINAL MOVE: WORST ERROR BY SCENARIO",
                 std::to_string(steps - out) + " of " + std::to_string(steps) +
                     " recorded steps within tolerance",
                 "scenario (the table's order)", "error"};
  worst.traces = {pos, vel};
  worst.rules = {static_cast<double>(sh::kPositionTolerance)};
  shoot(lab, "worst", gl::line_chart(worst),
        "Each scenario's worst step: the position error in units and the velocity error in "
        "units/s divided by 5, so the one dashed rule at 0.01 is both tolerances (0.01 u and "
        "0.05 u/s). Scenarios in the table's order.",
        "every dot sits under the dashed rule; most sit on zero", "all");
}

const gl::LabSpec kSpec{
    "framemove",
    gl::LabKind::scenario,
    "plugin:shooter",
    "plugins/shooter/framemove.hpp (the original player move) over hull.hpp, against "
    "tests/golden/shooter_framemove.txt",
    "shooter",
    "Does the original player move agree with its recordings on every scenario, and where does "
    "each run go?",
    {
        {"recording", "tests/golden/shooter_framemove.txt", "the original move's recordings"},
        {"scenes", "tests/golden/shooter_recording.txt", "the recordings the scenes are read from"},
        {"draw", "all", "the scenarios to draw (comma-separated names), or all"},
    },
    "--draw=stairs_up,swim_up_and_jump,water_jump_out",
    run,
};
GE_LAB(kSpec);

}  // namespace
