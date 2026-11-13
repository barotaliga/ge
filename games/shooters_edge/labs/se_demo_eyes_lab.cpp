// LAB `se-demo-eyes`: DOES A PLAYED DEMO BOB, ROLL AND KICK AS THE FIRST GAME'S VIEW DOES? A
// recording keeps the player's place and look, never the view he saw, so the window rebuilds the
// classic view on the recorded eye (client/demo_eyes.hpp over engine/camera's
// `classic_first_person`, the camera cvars at the classic defaults). The demo is played the
// window's way (timedemo.hpp's paced `playdemo_step`, drawn between frames) at `--fps` pictures a
// second, and every picture's view is worked out twice: as recorded (`demo_view`, what the window
// drew before) and with the classic view rebuilt (`demo_eyes_view`).
//
// By default the demo is a multiview match written in code (demo_write.hpp, 77 frames a second):
// the player stands, runs forward at 320 units a second (the bob), strafes left and then right
// (the roll), stands and takes a blow from his right and one from in front (handed to the reader
// as the damage message would: the kick toward each), then dies (the view on its side). Its
// velocity is the one the view estimates from the frames read, its ground the flat floor (no
// level is loaded). `--file=<path>` plays a real recording instead (`--track` its player), and
// `--game=<base>/<folder>` loads its levels from that game folder so the ground is the level's
// own player hull traced under his feet.
//
// What it shows: the eye's height over the origin, the roll and the pitch against time, as
// recorded and rebuilt, the fixture's phases named in the caption; a table of the numbers. On the
// fixture the Exit holds the strafe's roll to the classic 2 degrees each way, the run's bob to its
// formula's span (0.3 and 0.7 of 0.02 of the speed, clamped to -7..+4 units), the kicks above 0
// and the dead roll to 80 degrees. Everything is deterministic.
//
// ge:summary Lab: a played demo's eye with the classic bob, roll and kick rebuilt, against as read
// ge:keywords lab se-demo-eyes demo-eyes demo view bob strafe-roll damage-kick dead-roll playdemo
// ge:keywords multiview client-recording joined-server classic-view
// ge:skill camera shooter labs
// ge:test tests/shooters_edge_demo_eyes_test.cpp
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_eyes.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/timedemo.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace sh = ge::shooter;
using ge::core::num::to_fixed;

constexpr double kDeg = 3.14159265358979323846 / 180;
constexpr double kSpeed = 320;  // the fixture's run, units/s

// The fixture's phases, s.
constexpr double kRunFrom = 0.5, kLeftFrom = 2.5, kRightFrom = 3.5, kStandFrom = 4.5;
constexpr double kBlowRight = 4.6, kBlowFront = 5.0, kDeadFrom = 5.5, kEnd = 6.5;

cl::MatchScript fixture() {
  cl::MatchScript m;
  m.frame_s = 0.013f;  // 77 a second, whole milliseconds
  m.frames = static_cast<int>(kEnd / m.frame_s);
  m.players.resize(2);
  m.players[0].name = "runner";
  m.players[1].name = "stander";
  sh::Vec3f at{0, 0, 24};
  for (int f = 0; f < m.frames; f++) {
    const double t = f * 0.013;
    float vx = 0, vy = 0;
    if (t >= kRunFrom && t < kLeftFrom) vx = kSpeed;
    if (t >= kLeftFrom && t < kRightFrom) vy = kSpeed;    // looking +x, moving +y: left
    if (t >= kRightFrom && t < kStandFrom) vy = -kSpeed;  // right
    at.x += vx * m.frame_s;
    at.y += vy * m.frame_s;
    const auto eighth = [](float v) { return std::round(v * 8) / 8; };
    m.players[0].origins.push_back({eighth(at.x), eighth(at.y), 24});
    m.players[0].health.push_back(t >= kDeadFrom ? 0 : 100);
    m.players[0].armour.push_back(0);
    m.players[1].origins.push_back({-512, -512, 24});
  }
  return m;
}

struct Run {
  std::vector<double> t, height_read, height_rig, roll_read, roll_rig, pitch_read, pitch_rig;
  int64_t blows = 0, levels = 0;
};

bool play(std::vector<uint8_t> bytes, bool scripted, int track, double fps, double from,
          double seconds, const cl::SearchPath* files, Run& out, std::string& error) {
  cl::Timedemo t;
  if (!cl::open_demo(std::move(bytes), t.demo, error)) return false;
  t.start = t.demo;
  t.active = true;
  t.paced = true;
  t.smooth = true;
  t.demo.track = track;
  const ge::camera::FirstPersonRow row = ge::camera::classic_first_person(ge::brush::kUnitM);
  const double fov = ge::camera::FirstPersonRow{}.fov_y;
  cl::DemoEyes eyes;
  std::unique_ptr<cl::LoadedMap> map;
  bool right = false, front = false;
  for (int64_t i = 0; i < 4'000'000; i++) {
    const cl::TimedemoStep st = cl::playdemo_step(t, 1.0 / fps, 1, error);
    if (st == cl::TimedemoStep::error) return false;
    if (st == cl::TimedemoStep::end) break;
    if (st == cl::TimedemoStep::new_level) {
      out.levels++;
      map.reset();
      if (files != nullptr) map = cl::load_named_map(*files, t.demo.state.map, error);
      if (files != nullptr && !map) return false;
    }
    cl::DemoState& read = t.demo.state;
    if (scripted && !right && read.time >= kBlowRight) {  // a rocket's splash from his right
      right = true;
      cl::take_blow(read, {20, 40, {read.eye.x, read.eye.y - 200, 24}});
    }
    if (scripted && !front && read.time >= kBlowFront) {  // a shotgun from in front
      front = true;
      cl::take_blow(read, {0, 24, {read.eye.x + 300, read.eye.y, 24}});
    }
    const cl::DemoState& d = cl::demo_drawn(t);
    if (d.map.empty() || !cl::demo_eyes_drawn(d)) continue;
    const sh::BrushModel* ground =
        map && !map->spec.models.empty() ? &map->spec.models[0] : nullptr;
    const double clock = t.clock;
    const ge::render::FrameView rig =
        cl::demo_eyes_view(d, read, eyes, row, ground, {}, fov, clock, clock);
    if (seconds > 0 && (clock < from || clock >= from + seconds)) continue;  // outside the charts
    const ge::render::FrameView as_read = cl::demo_view(d, fov, clock);
    // The eye over the origin: the recorded eye is 22 (its view height) over it.
    const double over = static_cast<double>(d.view_height);
    out.t.push_back(clock);
    out.height_read.push_back(over);
    out.height_rig.push_back(over + (rig.eye[1] - as_read.eye[1]) / ge::brush::kUnitM);
    out.roll_read.push_back(as_read.roll / kDeg);
    out.roll_rig.push_back(rig.roll / kDeg);
    out.pitch_read.push_back(as_read.pitch / kDeg);
    out.pitch_rig.push_back(rig.pitch / kDeg);
  }
  out.blows = t.demo.state.blows_taken;
  return true;
}

// The extremes of `v` over pictures with `from <= t < to`.
std::pair<double, double> span(const Run& r, const std::vector<double>& v, double from, double to) {
  double lo = 1e300, hi = -1e300;
  for (size_t i = 0; i < r.t.size(); i++)
    if (r.t[i] >= from && r.t[i] < to) {
      lo = std::min(lo, v[i]);
      hi = std::max(hi, v[i]);
    }
  return {lo, hi};
}

gl::Trace trace(const Run& r, const std::vector<double>& v, const std::string& label) {
  return {label, r.t, v};
}

void run(gl::Lab& lab) {
  const std::string file = lab.arg("file"), folder = lab.arg("game");
  const double fps = std::max(1.0, lab.arg_num("fps"));
  const int track = static_cast<int>(lab.arg_num("track"));
  const bool scripted = file.empty();
  const double from = scripted ? 0 : lab.arg_num("from");
  const double seconds = scripted ? 0 : lab.arg_num("seconds");
  std::vector<uint8_t> bytes;
  if (scripted) {
    bytes = cl::write_multiview_match(fixture());
  } else {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
      lab.fail("se-demo-eyes: " + file + " does not open", "next: pass --file=<a demo's path>");
      return;
    }
    bytes.assign(std::istreambuf_iterator<char>(in), {});
  }
  std::string error;
  cl::SearchPath files;
  if (!folder.empty()) {
    const size_t cut = folder.find_last_of('/');
    const std::string base = cut == std::string::npos ? "." : folder.substr(0, cut);
    const std::string game = cut == std::string::npos ? folder : folder.substr(cut + 1);
    if (!cl::open_game_dir(base, game, files, error)) {
      lab.fail("se-demo-eyes: " + error, "next: pass --game=<base>/<game folder>; skill: shooter");
      return;
    }
  }
  Run r;
  if (!play(bytes, scripted, track, fps, from, seconds, folder.empty() ? nullptr : &files, r,
            error)) {
    lab.fail("se-demo-eyes: " + error, "next: check the file with the se-demo lab; skill: shooter");
    return;
  }
  const std::vector<std::pair<std::string, std::string>> in = {
      {"file", scripted ? "(a run written in code)" : file},
      {"game", folder.empty() ? "(none: the ground by the climb)" : folder},
      {"fps", to_fixed(fps, 0)},
      {"window", seconds > 0 ? to_fixed(from, 1) + " s for " + to_fixed(seconds, 1) + " s"
                             : std::string("the whole demo")}};
  const double end = r.t.empty() ? 0 : r.t.back() + 1;
  const auto [bob_lo, bob_hi] =
      span(r, r.height_rig, scripted ? kRunFrom + 0.3 : 0, scripted ? kLeftFrom : end);
  const auto [left_lo, left_hi] = span(r, r.roll_rig, kLeftFrom + 0.2, kRightFrom);
  const auto [right_lo, right_hi] = span(r, r.roll_rig, kRightFrom + 0.2, kStandFrom);
  const auto [roll_lo, roll_hi] = span(r, r.roll_rig, 0, scripted ? kDeadFrom - 0.2 : end);
  double kick = 0;
  for (size_t i = 0; i < r.t.size(); i++)
    if (!scripted || (r.t[i] >= kStandFrom && r.t[i] < kDeadFrom))
      kick = std::max(kick, std::fabs(r.pitch_rig[i] - r.pitch_read[i]));
  const double dead = r.roll_rig.empty() ? 0 : r.roll_rig.back();
  lab.metric("pictures", static_cast<double>(r.t.size()), "count");
  lab.metric("blows", static_cast<double>(r.blows), "count");
  lab.metric("bob.span", bob_hi - bob_lo, "u");
  lab.metric("roll.most", std::max(std::fabs(roll_lo), std::fabs(roll_hi)), "deg");
  lab.metric("kick.pitch-most", kick, "deg");
  if (scripted) {
    lab.metric("roll.left", left_lo, "deg");
    lab.metric("roll.right", right_hi, "deg");
    lab.metric("roll.dead", dead, "deg");
  }

  gl::LabTable tab{"the view rebuilt on the recorded eye",
                   {"", "bob span u", "roll most deg", "pitch kick most deg", "blows"},
                   {}};
  tab.rows.push_back({"as recorded", "0.00", "0.00", "0.00", std::to_string(r.blows)});
  tab.rows.push_back({"classic view", to_fixed(bob_hi - bob_lo, 2),
                      to_fixed(std::max(std::fabs(roll_lo), std::fabs(roll_hi)), 2),
                      to_fixed(kick, 2), std::to_string(r.blows)});
  lab.table(tab);

  const std::string name = file.substr(file.find_last_of('/') + 1);
  const std::string sub =
      (scripted ? std::string("a run written in code: stand, run, strafe left, strafe right, two "
                              "blows, dead")
                : name) +
      ", drawn at " + to_fixed(fps, 0) + " pictures a second";
  const auto chart = [&](const char* id, const char* title, const char* y,
                         const std::vector<double>& a, const std::vector<double>& b,
                         const std::string& what, const std::string& look_for) {
    gl::LineChart c;
    c.spikes = false;
    c.frame = {title, sub, "t (s)", y, 960, 360};
    c.traces = {trace(r, a, "as recorded"), trace(r, b, "classic view")};
    const gl::Chart drawn = gl::line_chart(c);
    lab.shot({id, drawn.title, what, look_for, in}, gl::to_picture(drawn.plot));
  };
  chart("height", "THE EYE OVER THE ORIGIN", "units", r.height_read, r.height_rig,
        "The eye's height over the player's origin: as recorded it stands at the view height; "
        "rebuilt, the stride's bob rides on it while he runs and holds in the air, and dead it "
        "drops to 16 under the origin.",
        "a steady line as recorded; the bob's lopsided wave while running, up to 4 over");
  chart("roll", "THE VIEW'S ROLL", "degrees (right side down positive)", r.roll_read, r.roll_rig,
        "The roll: none as recorded; rebuilt, strafing leans the view into the strafe by the side "
        "speed's share of 200 units a second, a blow from the side tips it, and dead it lies at "
        "80 degrees.",
        "-2 strafing left, +2 strafing right, a spike at each blow, 80 at the end");
  chart("pitch", "THE VIEW'S PITCH", "degrees (up positive)", r.pitch_read, r.pitch_rig,
        "The pitch: the recorded look; rebuilt, a blow from in front kicks the view toward it "
        "and the kick fades over half a second.",
        "the two lines together except a fading step after each blow");

  if (!scripted) return;
  const auto near = [](double a, double b, double tol) { return std::fabs(a - b) <= tol; };
  // The bob at 320 u/s: from 0.3 - 0.7 to 0.3 + 0.7 of 6.4 units, clamped to -7..+4.
  const double amp = 0.02 * kSpeed, want = std::min(4.0, amp) - std::max(-7.0, -0.4 * amp);
  if (!near(bob_hi - bob_lo, want, 0.35))
    lab.fail("se-demo-eyes: the run's bob spans " + to_fixed(bob_hi - bob_lo, 2) +
                 " units, not the formula's " + to_fixed(want, 2),
             "next: read client/demo_eyes.cpp's velocity estimate; skill: camera");
  if (!near(left_lo, -2, 0.05) || !near(right_hi, 2, 0.05))
    lab.fail("se-demo-eyes: the strafes roll " + to_fixed(left_lo, 2) + " and " +
                 to_fixed(right_hi, 2) + " degrees, not -2 and +2",
             "next: read client/demo_eyes.cpp; skill: camera");
  if (!(kick > 0.5))
    lab.fail("se-demo-eyes: the blows kicked the pitch " + to_fixed(kick, 2) + " degrees",
             "next: read client/demo_eyes.cpp's blows; skill: camera");
  if (!near(dead, 80, 1e-6))
    lab.fail("se-demo-eyes: dead, the view rolls " + to_fixed(dead, 2) + " degrees, not 80",
             "next: read client/demo_eyes.cpp; skill: camera");
}

const gl::LabSpec kSpec{
    "se-demo-eyes",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "the classic first-person view rebuilt on a played demo's and a joined server's eye "
    "(client/demo_eyes.hpp over engine/camera classic_first_person)",
    "camera",
    "Does a played demo bob, roll and kick as the first game's view does, and by how much does "
    "it differ from the eye as recorded?",
    {
        {"file", "", "a demo's path; empty: a run written in code"},
        {"game", "",
         "a game folder (<base>/<folder>) the demo's levels load from: the ground is traced in "
         "them; empty: none"},
        {"track", "-1",
         "the slot the view rides in a multiview recording (-1: the lowest playing)"},
        {"fps", "60", "pictures a second the demo is drawn at"},
        {"from", "30", "a real demo's charts start here, s of play (the fixture: all of it)"},
        {"seconds", "12", "how long a real demo's charts run, s (0: the whole demo)"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
