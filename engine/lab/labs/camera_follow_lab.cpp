// LAB `camera-follow`: whether every follower a camera is built on (engine/camera/follow.hpp)
// answers the same at every display rate. Each case is a follower or a spring handed a quick
// rise (0 to 1 over 0.2 s from 1/3 s, a subject's swerve) (and a ramp from 1.5 s), sampled at 30,
// 60, 144 and 240 Hz and at 60 Hz with a HITCH (a 100 ms frame every half second, a slow device's
// stall): one chart a case with every rate's line over another. For each case and rate: the
// overshoot, the time to settle within 2% of the rise, and the furthest the line strays from the
// 240 Hz one. A rate whose line strays past the row's bound, or a hitch that makes a follower ring
// or blow up, fails.
//
// ge:summary Lab: the camera's followers and springs at 30..240 Hz and through hitches
// ge:keywords lab camera follower spring half-life rise-response frame-rate hitch stall overshoot
// ge:keywords settle frame-rate-independent poles zeta response
// ge:skill camera labs
// ge:test tests/camera_follow_test.cpp
#include <cmath>
#include <string>
#include <vector>

#include "camera/follow.hpp"
#include "camera/transition.hpp"
#include "camera_sketch.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/plot.hpp"

namespace {

namespace gl = ge::lab;
namespace cam = ge::camera;
namespace sk = ge::lab::camera_sketch;

constexpr double kSeconds = 3.0;
// A camera's targets move; they do not jump (a teleport snaps the camera instead), so the
// followers are handed a quick smooth rise and then a ramp, and a line's stray from the 240 Hz
// one is the integration's and not a step's sampling.
constexpr double kStepAt = 1.0 / 3, kRise = 0.2, kRampAt = 1.5, kRampRate = 0.5;  // units/s

// What the followers chase: a quick rise from 0 to 1, then a ramp.
double target_at(double t) {
  if (t < kStepAt) return 0;
  if (t < kStepAt + kRise) return ge::camera::smootherstep((t - kStepAt) / kRise);
  if (t < kRampAt) return 1;
  return 1 + (t - kRampAt) * kRampRate;
}

struct Case {
  const char* id;
  const char* what;
  bool spring;  // a half-life spring, else a second-order follower
  cam::Poles poles;
  double halflife_s;
  // The furthest any rate's line may stray from the 240 Hz one. A follower takes its target as
  // moving in a line across the frame; a spring holds it where it ends, half a frame ahead.
  double bound;
};

// The followers the rigs are built on, by the poles and half-lives their rows carry.
const Case kCases[] = {
    {"critical-2hz",
     "a critically damped follower at 2 Hz (a boom's lift)",
     false,
     {2, 1, 0},
     0,
     0.03},
    {"breathing",
     "an underdamped follower, 2.4 Hz zeta 0.7 (the landing's dip)",
     false,
     {2.4, 0.7, 0},
     0,
     0.03},
    {"punchy",
     "a quick ringing follower, 6 Hz zeta 0.55 (a shot's punch)",
     false,
     {6, 0.55, 0},
     0,
     0.03},
    {"leading",
     "a follower that leads a ramp, 1.5 Hz zeta 1 r 2 (a yaw that keeps up)",
     false,
     {1.5, 1, 2},
     0,
     0.03},
    {"spring-0.15",
     "a critically damped spring, half-life 0.15 s (a boom's pull)",
     true,
     {},
     0.15,
     0.06},
};

struct Rate {
  const char* label;
  double hz;
  bool hitch;
};
const Rate kRates[] = {
    {"240 Hz", 240, false}, {"144 Hz", 144, false},        {"60 Hz", 60, false},
    {"30 Hz", 30, false},   {"60 Hz + hitches", 60, true},
};

struct Line {
  std::vector<double> t, y;
};

Line run_case(const Case& c, const Rate& r) {
  Line line;
  cam::Follower f;
  cam::settle(f, 0);
  cam::Spring s;
  // Time is counted in whole frames of the rate (a hitch is six of them at 60 Hz), so every
  // moment is exact.
  const long hitch = std::lround(0.1 * r.hz), every = std::lround(0.5 * r.hz);
  const long last = std::lround(kSeconds * r.hz);
  long tick = 0, next_hitch = every;
  while (tick < last) {
    long n = 1;
    if (r.hitch && tick + 1 >= next_hitch) {
      n = hitch;
      next_hitch += every;
    }
    tick += n;
    const double t = static_cast<double>(tick) / r.hz, dt = static_cast<double>(n) / r.hz;
    const double y = c.spring ? cam::spring_to(s, target_at(t), c.halflife_s, dt)
                              : cam::follow(f, c.poles, target_at(t), dt);
    line.t.push_back(t);
    line.y.push_back(y);
  }
  return line;
}

// The reference line's value at `t`, linearly between its frames.
double at(const Line& l, double t) {
  if (t <= l.t.front()) return l.y.front();
  for (size_t i = 1; i < l.t.size(); ++i) {
    if (l.t[i] >= t) {
      const double k = (t - l.t[i - 1]) / (l.t[i] - l.t[i - 1]);
      return l.y[i - 1] + (l.y[i] - l.y[i - 1]) * k;
    }
  }
  return l.y.back();
}

void run(gl::Lab& lab) {
  std::vector<std::vector<std::string>> rows;
  for (const Case& c : kCases) {
    std::vector<Line> lines;
    for (const Rate& r : kRates) lines.push_back(run_case(c, r));
    const Line& ref = lines[0];
    gl::LineChart lc;
    lc.frame = {std::string("RISE AND RAMP: ") + c.id, c.what, "t (s)", "position"};
    lc.spikes = false;
    lc.marks = {{kStepAt, "rise"}, {kRampAt, "ramp"}};
    lc.rules = {1.0};
    for (size_t i = 0; i < lines.size(); ++i) {
      const Rate& r = kRates[i];
      lc.traces.push_back({r.label, lines[i].t, lines[i].y});
      double most = 0, stray = 0, settled = -1;
      for (size_t k = 0; k < lines[i].t.size(); ++k) {
        const double t = lines[i].t[k], y = lines[i].y[k];
        if (t < kRampAt) {
          most = y > most ? y : most;
          if (t > kStepAt && std::fabs(y - 1) > 0.02)
            settled = -1;
          else if (t > kStepAt && settled < 0)
            settled = t - kStepAt;
        }
        const double d = std::fabs(y - at(ref, t));
        stray = d > stray ? d : stray;
        if (!std::isfinite(y) || std::fabs(y) > 10) stray = 1e9;
      }
      const std::string key = std::string(c.id) + "." + (r.hitch ? "hitch" : sk::num(r.hz, 0));
      lab.metric(key + ".overshoot", most - 1, "units");
      lab.metric(key + ".settle", settled, "s");
      lab.metric(key + ".stray", stray, "units");
      rows.push_back({c.id, r.label, sk::num(most - 1, 3), sk::num(settled, 3), sk::num(stray, 4)});
      // A hitch is a frame of 0.1 s, over which the ramp moves its rate's tenth.
      const double bound = r.hitch ? c.bound + kRampRate * 0.1 : c.bound;
      if (i > 0 && stray > bound) {
        lab.fail(std::string(c.id) + " at " + r.label + " strays " + sk::num(stray, 4) +
                     " from the 240 Hz line, past " + sk::num(bound, 3),
                 "Read the integration in engine/camera/follow.cpp, then run "
                 "tests/camera_follow_test.cpp (skill: camera).");
      }
    }
    const gl::Chart ch = gl::line_chart(lc);
    lab.shot({std::string(c.id) + "-rise",
              ch.title,
              std::string(c.what) +
                  ". The target rises from 0 to 1 over 0.2 s from 1/3 s and ramps from 1.5 s; one "
                  "line a "
                  "display rate, and one at 60 Hz with a 100 ms frame every half second.",
              "every rate's line on the 240 Hz one; the hitch line kinked but on the same path, "
              "never ringing or running away",
              {{"case", c.id}}},
             gl::to_picture(ch.plot));
  }
  lab.table({"every follower at every display rate",
             {"case", "rate", "overshoot", "settle-s", "stray-from-240hz"},
             rows});
  lab.note(
      "overshoot is how far past the rise the line goes (a critically damped follower's is 0); "
      "settle-s how long after the rise starts it stays within 2%, -1 when it never does before "
      "the "
      "ramp; stray the furthest the line is from the 240 Hz one at the same moment.");
}

const gl::LabSpec kSpec{
    "camera-follow",
    gl::LabKind::scenario,
    "engine",
    "engine/camera/follow.hpp (the second-order follower, the half-life spring)",
    "camera",
    "Does every camera follower answer the same at every display rate and through a hitch?",
    {},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
