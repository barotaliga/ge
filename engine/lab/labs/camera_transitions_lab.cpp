// LAB `camera-transitions`: what each EDIT (engine/camera/transition.hpp: cut, blend,
// inertialize) does to a change of view, through the carve scenario (engine/camera/scenarios.hpp)
// at a display rate. For each pair of rungs of the reference ladder and each edit, the view is
// changed mid-turn and followed to rest: a strip of what the lens sees through the change (one
// row an edit), and charts of the eye's distance from the subject and its speed in the subject's
// frame (the motion a player reads as the camera's own), with the arriving rig's own line beside
// them. A retarget case changes view again half way. For each: the kick in the eye's velocity at
// the switch, the peak speed and acceleration in the subject's frame, the peak turn rate and the
// time until the picture is the arriving rig's. A blend or an inertialized change that kicks the
// eye, or that has not landed within its duration, fails.
//
// ge:summary Lab: cut, blend and inertialize between rungs mid-turn: strips, charts, jerk, landing
// ge:keywords lab camera transition edit cut blend inertialize handover view-change switch jerk
// ge:keywords retarget landing strip subject-frame
// ge:skill camera labs
// ge:test tests/camera_transition_test.cpp
#include <cmath>
#include <string>
#include <vector>

#include "camera/camera.hpp"
#include "camera/ladders.hpp"
#include "camera/scenarios.hpp"
#include "camera/transition.hpp"
#include "camera_sketch.hpp"
#include "core/math.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/plot.hpp"

namespace {

namespace gl = ge::lab;
namespace cam = ge::camera;
namespace sk = ge::lab::camera_sketch;

constexpr double kSwitchAt = 2.5;  // s into the carve, mid-turn
constexpr double kEnd = 4.5;
constexpr double kLanded = 0.01;  // m: the picture is the arriving rig's
constexpr double kKickMps = 2.0;  // the most a smooth edit may change the eye's velocity at once

struct Pair {
  const char* from;
  const char* to;
  const char* retarget;  // a third rung changed to half way, or nullptr
  bool strip;            // whether the pair gets a strip of sketches
};
const Pair kPairs[] = {
    {"chase", "far", nullptr, true},   {"chase", "helmet", nullptr, true},
    {"eyes", "chase", nullptr, false}, {"high", "chase", nullptr, false},
    {"chase", "helmet", "far", false},
};
struct EditRow {
  const char* name;
  cam::Edit edit;
};
const EditRow kEdits[] = {
    {"cut", cam::Edit::cut}, {"blend", cam::Edit::blend}, {"inertialize", cam::Edit::inertialize}};

struct Run {
  std::vector<double> t, dist, speed, turn;
  std::vector<cam::Pose> poses;
  std::vector<cam::Subject> subjects;
  std::vector<cam::Local> local;
};

// The carve run with the view changed at kSwitchAt by `edit` (and again half way when the pair
// retargets); `edit` null: the arriving rig alone, from the start.
Run run_pair(const cam::Scenario& sc, const Pair& p, const EditRow* edit, double hz,
             double duration) {
  const auto ladder = cam::reference_ladder();
  cam::Camera c = cam::make_camera(ladder.data(), static_cast<int>(ladder.size()), 1);
  const char* last = p.retarget ? p.retarget : p.to;
  c.rung = cam::find_rung(c, edit ? p.from : last);
  const cam::World w = cam::scenario_world(sc);
  cam::ScriptRun script = cam::start_script(sc);
  Run r;
  bool switched = false, retargeted = false;
  const int frames = static_cast<int>(kEnd * hz);
  for (int f = 1; f <= frames; ++f) {
    const double t = f / hz;
    if (edit && !switched && t >= kSwitchAt) {
      cam::set_rung(c, cam::find_rung(c, p.to), edit->edit, duration);
      switched = true;
    }
    if (edit && p.retarget && !retargeted && t >= kSwitchAt + duration / 2) {
      cam::set_rung(c, cam::find_rung(c, p.retarget), edit->edit, duration);
      retargeted = true;
    }
    const cam::Subject s = cam::advance_to(script, t);
    const cam::Pose pose = cam::update(c, s, w, 1 / hz);
    const cam::Local l = cam::to_local(pose, s);
    r.t.push_back(t);
    r.dist.push_back(ge::core::hypot3(l.off.x, l.off.y, l.off.z));
    if (r.local.empty()) {
      r.speed.push_back(0);
      r.turn.push_back(0);
    } else {
      const cam::Local rate = cam::local_rate(r.local.back(), l, 1 / hz);
      r.speed.push_back(ge::core::hypot3(rate.off.x, rate.off.y, rate.off.z));
      r.turn.push_back(std::fabs(rate.yaw) * 180 / ge::core::kPi);
    }
    r.poses.push_back(pose);
    r.subjects.push_back(s);
    r.local.push_back(l);
  }
  return r;
}

void run(gl::Lab& lab) {
  const double hz = lab.arg_num("hz");
  const double duration = lab.arg_num("duration");
  const cam::Scenario& sc = *cam::find_camera_scenario("carve");
  // The last frame before the switch (frame f is at f / hz and sits at index f - 1).
  const size_t at = static_cast<size_t>(std::lround(kSwitchAt * hz)) - 2;
  std::vector<std::vector<std::string>> rows;
  for (const Pair& p : kPairs) {
    std::string key = std::string(p.from) + "-to-" + p.to;
    if (p.retarget) key += "-to-" + std::string(p.retarget);
    const Run arriving = run_pair(sc, p, nullptr, hz, duration);
    std::vector<Run> runs;
    gl::LineChart dist, speed;
    dist.frame = {"EYE TO SUBJECT: " + key, "carve, the view changed at 2.5 s", "t (s)", "m"};
    speed.frame = {"EYE SPEED IN THE SUBJECT'S FRAME: " + key, "carve, the view changed at 2.5 s",
                   "t (s)", "m/s"};
    for (gl::LineChart* lc : {&dist, &speed}) {
      lc->spikes = false;
      lc->marks = {{kSwitchAt, "switch"}};
      if (p.retarget) lc->marks.push_back({kSwitchAt + duration / 2, "retarget"});
    }
    for (const EditRow& e : kEdits) {
      runs.push_back(run_pair(sc, p, &e, hz, duration));
      const Run& r = runs.back();
      // Plot the window round the change.
      std::vector<double> t, d, v;
      double peak_v = 0, peak_a = 0, peak_turn = 0, landed = -1;
      for (size_t k = at - static_cast<size_t>(0.3 * hz); k < r.t.size(); ++k) {
        t.push_back(r.t[k]);
        d.push_back(r.dist[k]);
        v.push_back(r.speed[k]);
        if (k > at + 1) {
          peak_v = r.speed[k] > peak_v ? r.speed[k] : peak_v;
          const double a = std::fabs(r.speed[k] - r.speed[k - 1]) * hz;
          peak_a = a > peak_a ? a : peak_a;
          peak_turn = r.turn[k] > peak_turn ? r.turn[k] : peak_turn;
        }
        const cam::Pose& a1 = r.poses[k];
        const cam::Pose& a2 = arriving.poses[k];
        const double off =
            ge::core::hypot3(a1.eye.x - a2.eye.x, a1.eye.y - a2.eye.y, a1.eye.z - a2.eye.z);
        if (k > at && off > kLanded)
          landed = -1;
        else if (k > at && landed < 0)
          landed = r.t[k] - kSwitchAt;
      }
      dist.traces.push_back({e.name, t, d});
      // The cut's one-frame spike (in the table) would flatten every other line.
      if (e.edit != cam::Edit::cut) speed.traces.push_back({e.name, t, v});
      // The kick: how much the eye's velocity in the subject's frame changes across the switch
      // (a lagging rig orbits in that frame as the subject turns, so a move alone is no jump;
      // a smooth edit leaves the old rig with its own velocity).
      auto vel = [&](size_t k, int axis) {
        const cam::Local& a = r.local[k - 1];
        const cam::Local& b = r.local[k];
        return (axis == 0   ? b.off.x - a.off.x
                : axis == 1 ? b.off.y - a.off.y
                            : b.off.z - a.off.z) *
               hz;
      };
      const double kick = ge::core::hypot3(vel(at + 1, 0) - vel(at, 0), vel(at + 1, 1) - vel(at, 1),
                                           vel(at + 1, 2) - vel(at, 2));
      const std::string mk = key + "." + e.name;
      lab.metric(mk + ".switch-kick", kick, "m/s");
      lab.metric(mk + ".peak-speed", peak_v, "m/s");
      lab.metric(mk + ".peak-accel", peak_a, "m/s2");
      lab.metric(mk + ".peak-turn", peak_turn, "deg/s");
      lab.metric(mk + ".landed", landed, "s");
      rows.push_back({key, e.name, sk::num(kick, 2), sk::num(peak_v, 1), sk::num(peak_a, 0),
                      sk::num(peak_turn, 0), sk::num(landed, 2)});
      if (e.edit == cam::Edit::cut) continue;
      // A smooth edit carries the eye's velocity through the switch.
      if (kick > kKickMps) {
        lab.fail(key + " by " + e.name + ": the eye's velocity jumped by " + sk::num(kick, 2) +
                     " m/s in the subject's frame at the switch",
                 "Read set_rung and update in engine/camera/camera.cpp, then run "
                 "tests/camera_transition_test.cpp (skill: camera).");
      }
      const double due = p.retarget ? duration * 1.5 : duration;
      if (landed < 0 || landed > due + 2 / hz) {
        lab.fail(key + " by " + e.name + ": the picture is not the arriving rig's " +
                     sk::num(due, 2) + " s after the switch (landed " + sk::num(landed, 2) + ")",
                 "Read the transition's end in engine/camera/camera.cpp (skill: camera).");
      }
    }
    {
      std::vector<double> t, d, v;
      for (size_t k = at - static_cast<size_t>(0.3 * hz); k < arriving.t.size(); ++k) {
        t.push_back(arriving.t[k]);
        d.push_back(arriving.dist[k]);
        v.push_back(arriving.speed[k]);
      }
      dist.traces.push_back(
          {std::string("the ") + (p.retarget ? p.retarget : p.to) + " alone", t, d});
      speed.traces.push_back(
          {std::string("the ") + (p.retarget ? p.retarget : p.to) + " alone", t, v});
    }
    const gl::Chart dc = gl::line_chart(dist);
    lab.shot({key + "-distance",
              dc.title,
              "The eye's distance from the subject through the change, by each edit, beside the "
              "arriving rig's own line (framed from the start).",
              "the cut a step; the blend an S onto the arriving line; the inertialized change "
              "leaving the old line with its slope and settling onto the arriving one",
              {{"pair", key}}},
             gl::to_picture(dc.plot));
    const gl::Chart vc = gl::line_chart(speed);
    lab.shot({key + "-speed",
              vc.title,
              "How fast the eye moves in the subject's frame (the camera's own motion, what a "
              "player reads as a move) through a blend and an inertialized change; the cut's "
              "one-frame spike is in the table.",
              "no spike on either line; both leave the old rig's speed without a step and are "
              "back on the arriving rig's line by the end of the change",
              {{"pair", key}}},
             gl::to_picture(vc.plot));
    if (!p.strip) continue;
    // The strip: what the lens sees through the change, one row an edit.
    const int cw = 200, ch = 112;
    std::vector<gl::Picture> cells;
    const double moments[] = {-1 / hz, 0.05, 0.15, 0.3, 0.45, duration + 0.1};
    for (size_t e = 0; e < runs.size(); ++e) {
      for (const double m : moments) {
        const size_t k = at + 1 + static_cast<size_t>(std::lround(m * hz));
        cells.push_back(sk::sketch_view(runs[e].poses[k], sc, runs[e].subjects[k], cw, ch,
                                        std::string(kEdits[e].name) + " " +
                                            (runs[e].t[k] >= kSwitchAt ? "+" : "") +
                                            sk::num(runs[e].t[k] - kSwitchAt, 2) + " S"));
      }
    }
    lab.shot({key + "-strip",
              "THROUGH THE CHANGE: " + key,
              "What the lens sees from the frame before the switch to just after the change's "
              "end (" +
                  sk::num(duration, 2) + " s); rows: cut, blend, inertialize.",
              "the cut's second frame already the arriving view; the blend and the inertialized "
              "change keeping the subject in frame all the way",
              {{"pair", key}, {"hz", sk::num(hz, 0)}}},
             sk::sheet(cells, 6, cw, ch));
  }
  lab.table({"every pair by every edit at " + sk::num(hz, 0) + " Hz, " + sk::num(duration, 2) +
                 " s changes",
             {"pair", "edit", "switch-kick-m/s", "peak-speed-m/s", "peak-accel-m/s2",
              "peak-turn-deg/s", "landed-s"},
             rows});
  lab.note(
      "Speeds and accelerations are the eye's in the subject's frame (to_local: right, up, "
      "ahead of the subject along its heading), measured from the frame after the switch. "
      "landed is how long after the switch the eye stays within 1 cm of the arriving rig kept "
      "warm from the start (-1: never by the end). A cut starts the arriving rig fresh, settled "
      "on the subject, so its landed is how long that fresh rig takes to forget it was "
      "restarted.");
}

const gl::LabSpec kSpec{
    "camera-transitions",
    gl::LabKind::scenario,
    "engine",
    "engine/camera/transition.hpp and the stack's set_rung (engine/camera/camera.hpp)",
    "camera",
    "What do a cut, a blend and an inertialized change do to a change of view mid-turn?",
    {
        {"hz", "60", "the display rate the camera runs at"},
        {"duration", "0.6", "each change's length, s"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
