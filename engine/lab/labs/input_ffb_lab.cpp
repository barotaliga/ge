// LAB `input-ffb`: what force feedback does to a wheel (engine/input/feedback.hpp), played on
// the simulated rim (sim.hpp) so it is measured with no wheel attached. Four scenes: the hand
// turns the wheel and lets go under a spring and a damper (does it come back, and does it
// overshoot?); a spring alone (it rings); a constant push (a car pulling to one side) against a
// hand holding straight; and a road's rumble (a sine) under the hand. Each scene charts the rim's
// angle and the motor's drive over time; the metrics are the settle time, the overshoot and the
// hand's error, so a change to an effect's model shows as a number.
//
// ge:summary Lab: force-feedback effects on a simulated wheel rim, charted and measured
// ge:keywords lab input force-feedback ffb wheel spring damper constant rumble rim settle
// ge:keywords overshoot motor chart
// ge:skill input labs
// ge:test tests/input_feedback_test.cpp tests/input_sim_test.cpp
#include <cmath>
#include <string>
#include <vector>

#include "input/feedback.hpp"
#include "input/sim.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"

namespace {

namespace gl = ge::lab;
namespace in = ge::input;

struct Scene {
  const char* id;
  const char* title;
  const char* caption;
  const char* look_for;
  std::vector<in::Effect> effects;
  double hold_until_s;  // the hand holds `hold` until then, then lets go (or keeps holding)
  double hold;
  bool keeps_hold;
};

in::Effect effect(in::EffectKind k, float a, float b = 0) {
  in::Effect e;
  e.kind = k;
  if (k == in::EffectKind::spring || k == in::EffectKind::damper) {
    e.coeff = a;
  } else if (k == in::EffectKind::constant) {
    e.magnitude = a;
  } else {
    e.magnitude = a;
    e.period_s = b;
  }
  return e;
}

void run(gl::Lab& lab) {
  const double hz = lab.arg_num("hz");
  const double seconds = lab.arg_num("seconds");
  const auto k = static_cast<float>(lab.arg_num("spring"));
  const auto c = static_cast<float>(lab.arg_num("damper"));
  const Scene scenes[] = {
      {"return",
       "SPRING AND DAMPER: TURNED, THEN LET GO",
       "The hand turns the wheel to 0.8 and lets go at 1 s; a spring and a damper bring it back.",
       "the angle returns to 0 with at most a small overshoot and stays there; the motor's drive "
       "falls to 0 with it",
       {effect(in::EffectKind::spring, k), effect(in::EffectKind::damper, c)},
       1.0,
       0.8,
       false},
      {"ring",
       "A SPRING ALONE: TURNED, THEN LET GO",
       "The same turn and release with the spring and no damper: only the rim's own friction "
       "takes the energy out.",
       "the angle swings through centre several times, each swing a little smaller",
       {effect(in::EffectKind::spring, k)},
       1.0,
       0.8,
       false},
      {"pull",
       "A CONSTANT PULL HELD STRAIGHT",
       "A constant push to the left (a car pulling to one side) against a hand that holds the "
       "wheel straight all along.",
       "the angle settles a little left of 0, where the hand's grip balances the pull",
       {effect(in::EffectKind::constant, -0.3f)},
       seconds,
       0.0,
       true},
      {"road",
       "A ROAD'S RUMBLE UNDER THE HAND",
       "A 12 Hz sine (a rough road) under a hand holding the wheel straight.",
       "the angle trembles around 0 at the sine's rate, small, never drifting",
       {effect(in::EffectKind::sine, 0.25f, 1.0f / 12)},
       seconds,
       0.0,
       true},
  };
  gl::LabTable table{
      "each scene", {"scene", "settle s", "overshoot", "last angle", "max drive"}, {}};
  for (const Scene& sc : scenes) {
    in::SimInput sim;
    in::Feedback fb;
    in::SimWheel w;
    w.id = in::sim_plug(sim, in::sim_wheel());
    for (const in::Effect& e : sc.effects) in::play_effect(fb, w.id, e, 0);
    gl::Trace angle{"angle", {}, {}}, drive{"motor drive", {}, {}};
    double settle = -1, overshoot = 0, most = 0;
    const int steps = static_cast<int>(seconds * hz);
    for (int i = 0; i < steps; i++) {
      const double t = i / hz;
      w.hold = (t < sc.hold_until_s || sc.keeps_hold) ? sc.hold : -2;
      in::sim_wheel_step(sim, w, fb, 1.0 / hz);
      in::sim_advance(sim, 1.0 / hz);
      sim.queue.clear();
      angle.x.push_back(t);
      angle.y.push_back(w.pos);
      drive.x.push_back(t);
      drive.y.push_back(w.motor);
      most = std::fmax(most, std::fabs(w.motor));
      if (t > sc.hold_until_s) {
        if (-w.pos > overshoot) overshoot = -w.pos;  // past centre the other way
        if (std::fabs(w.pos) > 0.02)
          settle = -1;
        else if (settle < 0)
          settle = t - sc.hold_until_s;
      }
    }
    gl::LineChart chart;
    chart.frame = {sc.title, "simulated rim at " + std::to_string(static_cast<int>(hz)) + " Hz",
                   "t (s)", "angle, drive (-1..1)"};
    chart.spikes = false;
    chart.traces = {angle, drive};
    if (sc.hold_until_s < seconds) chart.marks.push_back({sc.hold_until_s, "LET GO"});
    const gl::Chart drawn = gl::line_chart(chart);
    const std::string svg = gl::to_svg(drawn.plot);
    lab.attach(std::string(sc.id) + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
    lab.shot(
        {sc.id,
         sc.title,
         sc.caption,
         sc.look_for,
         {{"spring", lab.arg("spring")}, {"damper", lab.arg("damper")}, {"hz", lab.arg("hz")}}},
        gl::to_picture(drawn.plot));
    lab.metric(std::string(sc.id) + ".settle", settle, "s");
    lab.metric(std::string(sc.id) + ".overshoot", overshoot, "");
    lab.metric(std::string(sc.id) + ".last", w.pos, "");
    table.rows.push_back({sc.id, std::to_string(settle).substr(0, 5),
                          std::to_string(overshoot).substr(0, 5),
                          std::to_string(w.pos).substr(0, 6), std::to_string(most).substr(0, 5)});
    if (std::string(sc.id) == "return" && (settle < 0 || overshoot > 0.2)) {
      lab.fail(
          "the spring and damper did not bring the wheel back within 0.02 of centre with "
          "under 0.2 overshoot",
          "Read engine/input/feedback.cpp's spring and damper and sim.cpp's rim, then rerun "
          "`make lab LAB=input-ffb` (skill: input).");
    }
  }
  lab.table(table);
}

const gl::LabSpec kSpec{
    "input-ffb",
    gl::LabKind::scenario,
    "engine",
    "engine/input/feedback.hpp, engine/input/sim.hpp",
    "input",
    "What do the force-feedback effects do to a wheel's rim under a hand?",
    {
        {"spring", "0.8", "the return spring's coefficient"},
        {"damper", "0.25", "the damper's coefficient (0.36 damps the default spring critically)"},
        {"seconds", "4", "each scene's length"},
        {"hz", "240", "the rim's step rate"},
    },
    "--seconds=3",
    run,
};
GE_LAB(kSpec);

}  // namespace
