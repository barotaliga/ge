// LAB `pmove`: does the classic move agree with its recordings, scenario by scenario, and where
// does each run go? Every recorded scenario (tests/golden/shooter_recording.txt) is replayed
// through `player_move` (plugins/shooter/recording.hpp) and measured line by line: one table of
// each scenario's worst position and velocity error against the tolerance, a chart of those
// worsts, and for each drawn scenario its path from above and from the side, ours over the
// recording's, the scene's brushes cut at y = 0 behind the side view. Then the engine's generic
// mover on the classic rows (plugins/shooter/row.hpp) against the same recordings: its table
// and chart of each scenario's step and run errors.
//
// ge:summary Lab: the classic move against its recordings, worst errors and paths per scenario
// ge:keywords lab pmove parity recording tolerance trajectory top-view side-view strafe-jump
// ge:keywords stairs ramp water-jump scenario classic-row engine-mover row_recording
// ge:skill shooter labs
// ge:test tests/shooter_recording_test.cpp
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooter/recording.hpp"
#include "shooter/row_recording.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
using ge::core::num::to_fixed;

constexpr double kNan = std::numeric_limits<double>::quiet_NaN();

std::string upper(std::string s) {
  for (char& c : s)
    if (c >= 'a' && c <= 'z')
      c = static_cast<char>(c - 32);
    else if (c == '_')
      c = ' ';
  return s;
}

bool listed(const std::string& list, const std::string& name) {
  if (list == "all") return true;
  size_t i = 0;
  while (i <= list.size()) {
    size_t j = list.find(',', i);
    if (j == std::string::npos) j = list.size();
    if (list.compare(i, j - i, name) == 0) return true;
    i = j + 1;
  }
  return false;
}

// A brush cut by the plane y = 0, as a closed outline in (x, z): a big square clipped by each of
// the brush's planes that has an x or z part, then clamped to the window.
void brush_outline(const sh::Brush& b, double x0, double x1, double z0, double z1, gl::Trace& t) {
  std::vector<std::pair<double, double>> poly = {{x0, z0}, {x1, z0}, {x1, z1}, {x0, z1}};
  for (const sh::Plane& p : b.planes) {
    const double nx = p.normal.x, nz = p.normal.z, d = p.dist;
    if (nx == 0 && nz == 0) {
      if (d < 0) poly.clear();  // y = 0 is outside the brush
      continue;
    }
    std::vector<std::pair<double, double>> out;
    for (size_t i = 0; i < poly.size(); i++) {
      const auto& a = poly[i];
      const auto& c = poly[(i + 1) % poly.size()];
      const double da = nx * a.first + nz * a.second - d;
      const double dc = nx * c.first + nz * c.second - d;
      if (da <= 0) out.push_back(a);
      if ((da < 0 && dc > 0) || (da > 0 && dc < 0)) {
        const double f = da / (da - dc);
        out.emplace_back(a.first + f * (c.first - a.first), a.second + f * (c.second - a.second));
      }
    }
    poly = out;
  }
  if (poly.size() < 3) return;
  for (const auto& q : poly) {
    t.x.push_back(q.first);
    t.y.push_back(q.second);
  }
  t.x.push_back(poly.front().first);
  t.y.push_back(poly.front().second);
  t.x.push_back(poly.front().first);
  t.y.push_back(kNan);
}

void shoot(gl::Lab& lab, const std::string& id, const gl::Chart& chart, const std::string& caption,
           const std::string& look_for, const std::string& scenario) {
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({id,
            chart.title,
            caption,
            look_for,
            {{"scenario", scenario}, {"recording", lab.arg("recording")}}},
           gl::to_picture(chart.plot));
}

void draw(gl::Lab& lab, const sh::Recording& rec, const sh::RecordedScenario& sc,
          const sh::ReplayResult& r) {
  gl::Trace ours_top{"ours", {}, {}}, rec_top{"recorded", {}, {}};
  gl::Trace ours_side{"ours", {}, {}}, rec_side{"recorded", {}, {}};
  double x0 = sc.start.origin.x, x1 = x0, z0 = sc.start.origin.z, z1 = z0;
  auto add = [&](const sh::Vec3f& o, gl::Trace& top, gl::Trace& side) {
    top.x.push_back(o.x);
    top.y.push_back(o.y);
    side.x.push_back(o.x);
    side.y.push_back(o.z);
    x0 = std::min(x0, static_cast<double>(o.x));
    x1 = std::max(x1, static_cast<double>(o.x));
    z0 = std::min(z0, static_cast<double>(o.z));
    z1 = std::max(z1, static_cast<double>(o.z));
  };
  add(sc.start.origin, ours_top, ours_side);
  add(sc.start.origin, rec_top, rec_side);
  for (size_t i = 0; i < r.lines.size(); i++) {
    add(r.lines[i].ours.origin, ours_top, ours_side);
    add(sc.lines[i].after.origin, rec_top, rec_side);
  }

  const std::string title = upper(sc.name);
  const std::string worst = "worst position error " + to_fixed(r.max_position_error, 6) +
                            " u (cmd " + std::to_string(r.worst_position_line) + "), velocity " +
                            to_fixed(r.max_velocity_error, 6) + " u/s";
  const std::string look =
      "one path: ours is drawn over the recording's, so where the two agree the recording's "
      "colour does not show; " +
      std::string(r.failed == 0 ? "every line is within tolerance"
                                : std::to_string(r.failed) + " lines are OUT of tolerance");

  gl::LineChart top;
  top.spikes = false;
  top.frame = {title + " FROM ABOVE", worst, "x (units)", "y (units)"};
  top.traces = {rec_top, ours_top};
  shoot(lab, sc.name + "-top", gl::line_chart(top),
        "From above: the origin's path over " + std::to_string(r.lines.size()) +
            " commands, ours and the recording's. " + worst + ".",
        look, sc.name);

  const double mx = std::max(16.0, (x1 - x0) * 0.08), mz = std::max(16.0, (z1 - z0) * 0.15);
  gl::Trace solid{"solid at y = 0", {}, {}}, liquid{"water, slime, lava at y = 0", {}, {}};
  for (const sh::Brush& b : rec.scenes[static_cast<size_t>(sc.scene)].brushes)
    brush_outline(b, x0 - mx, x1 + mx, z0 - 24 - mz, z1 + mz,
                  b.contents == sh::kContentsSolid ? solid : liquid);
  gl::LineChart side;
  side.spikes = false;
  side.frame = {title + " FROM THE SIDE", worst, "x (units)", "z, the origin (units)"};
  side.traces = {solid, rec_side, ours_side};
  if (!liquid.x.empty()) side.traces.insert(side.traces.begin() + 1, liquid);
  shoot(lab, sc.name + "-side", gl::line_chart(side),
        "From the side: the origin's height (24 above the feet) along x, ours and the "
        "recording's, over the scene's brushes cut at y = 0, solid and liquid apart (scene " +
            rec.scenes[static_cast<size_t>(sc.scene)].name + "). " + worst + ".",
        look + "; the origin rides 24 above each floor and climbs stairs and ramps", sc.name);
}

void row_table(gl::Lab& lab, const sh::Recording& rec);  // below

void run(gl::Lab& lab) {
  const std::string& path = lab.arg("recording");
  std::ifstream in(path);
  if (!in) {
    lab.fail("pmove: cannot read the recording " + path,
             "next: run the lab from the repository's root, or pass --recording=<file>");
    return;
  }
  std::stringstream ss;
  ss << in.rdbuf();
  sh::Recording rec;
  std::string error;
  if (!sh::parse_recording(ss.str(), rec, error)) {
    lab.fail("pmove: " + path + ": " + error,
             "next: fix that line (format: "
             "plugins/shooter/recording.hpp)");
    return;
  }

  gl::LabTable table{"every recorded scenario against its recording",
                     {"scenario", "scene", "lines", "out", "exact", "worst position u", "at cmd",
                      "worst velocity u/s", "at cmd", "first out"},
                     {}};
  gl::Trace pos{"worst position error (u)", {}, {}}, vel{"worst velocity error / 5 (u/s)", {}, {}};
  int lines = 0, out = 0, exact = 0, scenarios_out = 0;
  for (size_t i = 0; i < rec.scenarios.size(); i++) {
    const sh::RecordedScenario& sc = rec.scenarios[i];
    const sh::ReplayResult r = sh::replay(rec, sc);
    lines += static_cast<int>(r.lines.size());
    out += r.failed;
    int same = 0;  // lines equal to the recording in every printed digit
    for (const sh::LineCheck& c : r.lines)
      if (c.ok && c.position_error == 0 && c.velocity_error == 0) same++;
    exact += same;
    std::string first;
    for (const sh::LineCheck& c : r.lines)
      if (!c.ok) {
        first = "cmd " + std::to_string(c.index) + (c.mismatch.empty() ? "" : ": " + c.mismatch);
        break;
      }
    if (r.failed > 0) {
      scenarios_out++;
      lab.fail("pmove: " + sc.name + " leaves the tolerance on " + std::to_string(r.failed) +
                   " lines (first " + first + ")",
               "next: run tests/shooter_recording_test.cpp and read the step it names in "
               "plugins/shooter/pmove.cpp or slide.cpp");
    }
    table.rows.push_back({sc.name, rec.scenes[static_cast<size_t>(sc.scene)].name,
                          std::to_string(r.lines.size()), std::to_string(r.failed),
                          std::to_string(same), to_fixed(r.max_position_error, 6),
                          std::to_string(r.worst_position_line), to_fixed(r.max_velocity_error, 6),
                          std::to_string(r.worst_velocity_line), first.empty() ? "-" : first});
    lab.metric(sc.name + ".worst-position", r.max_position_error, "u");
    lab.metric(sc.name + ".worst-velocity", r.max_velocity_error, "u/s");
    lab.metric(sc.name + ".lines-out", r.failed, "count");
    pos.x.push_back(static_cast<double>(i));
    pos.y.push_back(r.max_position_error);
    vel.x.push_back(static_cast<double>(i));
    vel.y.push_back(r.max_velocity_error / 5);
    if (listed(lab.arg("draw"), sc.name)) draw(lab, rec, sc, r);
  }
  lab.metric("lines", lines, "count");
  lab.metric("lines-out", out, "count");
  lab.metric("lines-exact", exact, "count");
  lab.metric("scenarios-out", scenarios_out, "count");
  lab.table(table);

  gl::LineChart worst;
  worst.points = true;
  worst.spikes = false;
  worst.frame = {"WORST ERROR BY SCENARIO",
                 std::to_string(lines - out) + " of " + std::to_string(lines) +
                     " recorded lines within tolerance, " + std::to_string(exact) + " exact",
                 "scenario (the table's order)", "error"};
  worst.traces = {pos, vel};
  worst.rules = {static_cast<double>(sh::kPositionTolerance)};
  shoot(lab, "worst", gl::line_chart(worst),
        "Each scenario's worst line: the position error in units and the velocity error in "
        "units/s divided by 5, so the one dashed rule at 0.01 is both tolerances (0.01 u and "
        "0.05 u/s). Scenarios in the table's order.",
        "every dot sits under the dashed rule; most sit on zero (bit-exact runs)", "all");
  row_table(lab, rec);
}

// THE ENGINE MOVER'S CLASSIC ROW (plugins/shooter/row.hpp) on the same scenarios: each one's
// worst step error (one command from the recorded state) and its run's, the covered ones held to
// the recordings' own tolerance with on-ground, the ground and the water equal.
void row_table(gl::Lab& lab, const sh::Recording& rec) {
  gl::LabTable table{
      "the engine mover's classic row against every recording",
      {"scenario", "held", "step out", "run out", "step position u", "step velocity u/s",
       "run position u", "at cmd", "run velocity u/s", "at cmd"},
      {}};
  gl::Trace step{"step: worst position error (u)", {}, {}};
  gl::Trace run{"run: worst position error (u)", {}, {}};
  gl::Trace runv{"run: worst velocity error / 5 (u/s)", {}, {}};
  int held = 0, held_out = 0, lines = 0;
  for (size_t i = 0; i < rec.scenarios.size(); i++) {
    const sh::RecordedScenario& sc = rec.scenarios[i];
    const sh::RowReplay r = sh::replay_row(rec, sc);
    const bool covered = sh::row_covers(sc);
    if (covered) {
      held++;
      lines += static_cast<int>(r.run.lines.size());
      if (r.step.failed + r.run.failed > 0) {
        held_out++;
        lab.fail("pmove: the classic row leaves the tolerance on " + sc.name + " (" +
                     std::to_string(r.step.failed) + " step lines, " +
                     std::to_string(r.run.failed) + " run lines)",
                 "next: run tests/shooter_row_recording_test.cpp and read the scenario's first "
                 "line out; the row is plugins/shooter/row.cpp over engine/move/step.cpp");
      }
    }
    table.rows.push_back(
        {sc.name, covered ? "yes" : "no", std::to_string(r.step.failed),
         std::to_string(r.run.failed), to_fixed(r.step.max_position_error, 6),
         to_fixed(r.step.max_velocity_error, 6), to_fixed(r.run.max_position_error, 6),
         std::to_string(r.run.worst_position_line), to_fixed(r.run.max_velocity_error, 6),
         std::to_string(r.run.worst_velocity_line)});
    lab.metric("row." + sc.name + ".step-position", r.step.max_position_error, "u");
    lab.metric("row." + sc.name + ".run-position", r.run.max_position_error, "u");
    lab.metric("row." + sc.name + ".run-velocity", r.run.max_velocity_error, "u/s");
    const double x = static_cast<double>(i);
    step.x.push_back(x);
    step.y.push_back(covered ? r.step.max_position_error : kNan);
    run.x.push_back(x);
    run.y.push_back(covered ? r.run.max_position_error : kNan);
    runv.x.push_back(x);
    runv.y.push_back(covered ? r.run.max_velocity_error / 5 : kNan);
  }
  lab.metric("row.held-scenarios", held, "count");
  lab.metric("row.held-scenarios-out", held_out, "count");
  lab.metric("row.held-lines", lines, "count");
  lab.table(table);

  gl::LineChart chart;
  chart.points = true;
  chart.spikes = false;
  chart.frame = {"THE ENGINE MOVER'S CLASSIC ROW",
                 std::to_string(held) + " scenarios held (" + std::to_string(lines) + " lines), " +
                     std::to_string(held_out) + " out of the tolerance",
                 "scenario (the table's order)", "error"};
  chart.traces = {step, run, runv};
  chart.rules = {static_cast<double>(sh::kPositionTolerance)};
  shoot(lab, "row", gl::line_chart(chart),
        "The engine's generic mover (engine/move, engine/water/swim) on the classic rows "
        "(plugins/shooter/row.hpp), over the same brush worlds: each covered scenario's worst "
        "one-command position error from the recorded state, and its whole run's worst position "
        "and velocity (divided by 5) errors. The dashed rule at 0.01 is both tolerances, the "
        "recordings' own. Uncovered scenarios (a move type or setting the row does not carry) "
        "are left out.",
        "every dot far under the dashed rule: the generic mover carries the classic feel", "all");
}

const gl::LabSpec kSpec{
    "pmove",
    gl::LabKind::scenario,
    "plugin:shooter",
    "plugins/shooter/pmove.hpp over hull.hpp, against tests/golden/shooter_recording.txt",
    "shooter",
    "Does the classic player move agree with its recordings on every scenario, and where does "
    "each run go?",
    {
        {"recording", "tests/golden/shooter_recording.txt", "the recordings' file"},
        {"draw", "all", "the scenarios to draw (comma-separated names), or all"},
    },
    "--draw=strafe_jump,stairs_jump_airstep,water_jump_out",
    run,
};
GE_LAB(kSpec);

}  // namespace
