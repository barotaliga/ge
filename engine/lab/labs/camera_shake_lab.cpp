// LAB `camera-shake`: what a blow's trauma (engine/camera/shake.hpp) does to the picture. A
// trauma of 1 (an explosion beside the subject) and of 0.4 (a hard landing) are added at 0.5 s
// and the shake is read at the comfort settings 1, 0.5 and 0 (the `motion` and `shake` cvars), at
// 60 Hz, and the full blow at 30 and 240 Hz too. Charts: the roll and the pitch through the
// blow at each comfort; the roll at each display rate. For each: the peak of every angle and
// the offset, the time until the shake is gone, and the wave's frequency (from its crossings of
// zero). A shake past its row's amplitude, one that outlives its trauma, a wave faster than its
// row's top frequency, a comfort of 0 that moves the picture, or a display rate that changes the
// path fails.
//
// ge:summary Lab: trauma shake by size, comfort and display rate: charts, peaks, calm, frequency
// ge:keywords lab camera shake trauma comfort motion-sickness explosion landing frequency decay
// ge:keywords frame-rate wave
// ge:skill camera labs
// ge:test tests/camera_shake_test.cpp
#include <cmath>
#include <string>
#include <vector>

#include "camera/shake.hpp"
#include "camera_sketch.hpp"
#include "core/math.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/plot.hpp"

namespace {

namespace gl = ge::lab;
namespace cam = ge::camera;
namespace sk = ge::lab::camera_sketch;

constexpr double kBlowAt = 0.5, kSeconds = 2.5;
constexpr double kDeg = 180 / ge::core::kPi;

struct Line {
  std::vector<double> t, yaw, pitch, roll, offset;
};

Line run_shake(const cam::ShakeRow& row, double trauma, double scale, double hz) {
  Line l;
  cam::ShakeState s = cam::make_shake(1);
  bool hit = false;
  const int frames = static_cast<int>(kSeconds * hz);
  for (int f = 1; f <= frames; ++f) {
    const double t = f / hz;
    if (!hit && t >= kBlowAt) {
      cam::add_trauma(s, trauma);
      hit = true;
    }
    const cam::ShakeOut o = cam::shake_at(row, s, 1 / hz, scale);
    l.t.push_back(t);
    l.yaw.push_back(o.yaw * kDeg);
    l.pitch.push_back(o.pitch * kDeg);
    l.roll.push_back(o.roll * kDeg);
    l.offset.push_back(ge::core::hypot3(o.right, o.up, o.forward));
  }
  return l;
}

double peak(const std::vector<double>& v) {
  double m = 0;
  for (const double x : v) m = std::fabs(x) > m ? std::fabs(x) : m;
  return m;
}

// The last moment anything moves, s after the blow (-1: never moved).
double calm_after(const Line& l) {
  double last = -1;
  for (size_t k = 0; k < l.t.size(); ++k) {
    if (std::fabs(l.roll[k]) + std::fabs(l.pitch[k]) + std::fabs(l.yaw[k]) > 1e-9)
      last = l.t[k] - kBlowAt;
  }
  return last;
}

// The roll's frequency, Hz, from its crossings of zero while the trauma is over a half.
double frequency(const Line& l, double until_s) {
  int crossings = 0;
  for (size_t k = 1; k < l.t.size(); ++k) {
    if (l.t[k] <= kBlowAt || l.t[k] > kBlowAt + until_s) continue;
    if ((l.roll[k - 1] < 0) != (l.roll[k] < 0)) ++crossings;
  }
  return until_s > 0 ? crossings / (2 * until_s) : 0;
}

void run(gl::Lab& lab) {
  const cam::ShakeRow row{};
  std::vector<std::vector<std::string>> rows;
  const double hz = 60;
  struct Blow {
    const char* id;
    double trauma;
  };
  const Blow blows[] = {{"explosion", 1.0}, {"landing", 0.4}};
  for (const Blow& b : blows) {
    gl::LineChart roll, pitch;
    roll.frame = {std::string("ROLL THROUGH A BLOW: ") + b.id,
                  "trauma " + sk::num(b.trauma, 1) + " at 0.5 s, 60 Hz", "t (s)", "deg"};
    pitch.frame = {std::string("PITCH THROUGH A BLOW: ") + b.id, roll.frame.subtitle, "t (s)",
                   "deg"};
    for (gl::LineChart* lc : {&roll, &pitch}) {
      lc->spikes = false;
      lc->marks = {{kBlowAt, "blow"}};
    }
    for (const double comfort : {1.0, 0.5, 0.0}) {
      const Line l = run_shake(row, b.trauma, comfort, hz);
      const std::string label = "comfort " + sk::num(comfort, 1);
      roll.traces.push_back({label, l.t, l.roll});
      pitch.traces.push_back({label, l.t, l.pitch});
      const double full = 1.0;  // trauma^exponent at its most, times the comfort
      const double most = full * comfort * kDeg;
      const double p_roll = peak(l.roll), p_pitch = peak(l.pitch), p_yaw = peak(l.yaw);
      const double p_off = peak(l.offset);
      const double calm = calm_after(l);
      // The trauma is over a half for (trauma - 0.5) / decay s.
      const double strong = (b.trauma - 0.5) / row.decay_per_s;
      const double freq = frequency(l, strong > 0.2 ? strong : 0.6);
      const std::string key = std::string(b.id) + ".comfort-" + sk::num(comfort, 1);
      lab.metric(key + ".peak-roll", p_roll, "deg");
      lab.metric(key + ".peak-pitch", p_pitch, "deg");
      lab.metric(key + ".peak-yaw", p_yaw, "deg");
      lab.metric(key + ".peak-offset", p_off, "m");
      lab.metric(key + ".calm-after", calm, "s");
      lab.metric(key + ".frequency", freq, "Hz");
      rows.push_back({b.id, sk::num(comfort, 1), sk::num(p_roll, 2), sk::num(p_pitch, 2),
                      sk::num(p_yaw, 2), sk::num(p_off, 3), sk::num(calm, 2), sk::num(freq, 1)});
      if (p_roll > row.roll * most + 1e-9 || p_pitch > row.pitch * most + 1e-9 ||
          p_yaw > row.yaw * most + 1e-9) {
        lab.fail(key + ": the shake passes its row's amplitude",
                 "Read shake_at in engine/camera/shake.cpp (skill: camera).");
      }
      if (comfort == 0 && calm >= 0) {
        lab.fail(key + ": a comfort of 0 still shakes the picture",
                 "Read shake_at's scale in engine/camera/shake.cpp (skill: camera).");
      }
      if (calm > b.trauma / row.decay_per_s + 2 / hz) {
        lab.fail(key + ": the shake outlives its trauma (" + sk::num(calm, 2) + " s)",
                 "Read the decay in engine/camera/shake.cpp (skill: camera).");
      }
      if (comfort > 0 && freq > row.freq_hz * 1.1) {
        lab.fail(key + ": the wave runs at " + sk::num(freq, 1) + " Hz, past the row's " +
                     sk::num(row.freq_hz, 1),
                 "Read wave in engine/camera/shake.cpp (skill: camera).");
      }
    }
    for (gl::LineChart* lc : {&roll, &pitch}) {
      const gl::Chart c = gl::line_chart(*lc);
      const std::string id = std::string(b.id) + (lc == &roll ? "-roll" : "-pitch");
      lab.shot({id,
                c.title,
                "The shake's " + std::string(lc == &roll ? "roll" : "pitch") +
                    " after the blow at each comfort setting (the product of the motion and "
                    "shake cvars).",
                "a violent start that falls off fast (trauma squared), smooth waves, never a "
                "frame-to-frame flicker; half the swing at comfort 0.5, a flat line at 0",
                {{"blow", b.id}}},
               gl::to_picture(c.plot));
    }
  }
  // The full blow at every display rate: the same path.
  {
    gl::LineChart lc;
    lc.frame = {"ROLL AT EVERY DISPLAY RATE", "trauma 1 at 0.5 s, comfort 1", "t (s)", "deg"};
    lc.spikes = false;
    const Line ref = run_shake(row, 1.0, 1.0, 240);
    for (const double r : {240.0, 60.0, 30.0}) {
      const Line l = r == 240 ? ref : run_shake(row, 1.0, 1.0, r);
      lc.traces.push_back({sk::num(r, 0) + " Hz", l.t, l.roll});
      double stray = 0;
      for (size_t k = 0; k < l.t.size(); ++k) {
        // The 240 Hz frame at the same moment.
        const size_t j = static_cast<size_t>(std::lround(l.t[k] * 240)) - 1;
        if (j < ref.t.size()) {
          const double d = std::fabs(l.roll[k] - ref.roll[j]);
          stray = d > stray ? d : stray;
        }
      }
      lab.metric("rates." + sk::num(r, 0) + ".stray-roll", stray, "deg");
      rows.push_back({"rates", sk::num(r, 0) + " Hz", "", "", "", "", "", sk::num(stray, 3)});
      if (stray > 0.25 * row.roll * kDeg) {
        lab.fail("the shake at " + sk::num(r, 0) + " Hz strays " + sk::num(stray, 2) +
                     " deg from the 240 Hz path",
                 "Read how shake_at advances its clock in engine/camera/shake.cpp (skill: "
                 "camera).");
      }
    }
    const gl::Chart c = gl::line_chart(lc);
    lab.shot({"rates-roll",
              c.title,
              "The same blow's roll sampled at 240, 60 and 30 Hz: the shake is a function of its "
              "own clock, so the frames fall on one path.",
              "every rate's frames on the 240 Hz line; the 30 Hz one coarser, never a different "
              "shape",
              {}},
             gl::to_picture(c.plot));
  }
  lab.table({"every blow at every comfort, and the display rates",
             {"blow", "comfort", "peak-roll-deg", "peak-pitch-deg", "peak-yaw-deg", "peak-offset-m",
              "calm-after-s", "frequency-hz (rates: stray deg)"},
             rows});
  lab.note(
      "The reference shake row: yaw and pitch 0.035 rad, roll 0.05 rad and 0.04 m at full "
      "shake, trauma lost at 1.2 a second, shake = trauma squared, the fastest wave 7 Hz. "
      "frequency is the roll's, from its zero crossings while the trauma is strong.");
}

const gl::LabSpec kSpec{
    "camera-shake",
    gl::LabKind::scenario,
    "engine",
    "engine/camera/shake.hpp (trauma, its decay, the waves, the comfort scale)",
    "camera",
    "What does a blow's trauma do to the picture at each comfort setting and display rate?",
    {},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
