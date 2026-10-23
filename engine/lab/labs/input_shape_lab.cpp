// LAB `input-shape`: what each dead zone, curve and key ramp hands a game (engine/input/
// shape.hpp), drawn so a tuning is chosen by looking. Four charts: the output's length against
// the push's along a diagonal (where a radial zone steps and a scaled one does not); the output
// along a push near a cardinal (where the axial and sloped zones snap and the round ones keep the
// angle); the response curves with an anti-dead zone; and the key ramps over time (a key held,
// then let go). Every number is a pure function of the shape, so the digest holds the shapes.
//
// ge:summary Lab: dead zones, curves and key ramps charted (output against push, against time)
// ge:keywords lab input dead-zone curve ramp chart stick shape tuning axial radial sloped
// ge:skill input labs
// ge:test tests/input_shape_test.cpp
#include <cmath>
#include <string>
#include <vector>

#include "input/shape.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"

namespace {

namespace gl = ge::lab;
namespace in = ge::input;

void shoot(gl::Lab& lab, const std::string& id, const gl::Chart& chart, const std::string& caption,
           const std::string& look_for) {
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({id,
            chart.title,
            caption,
            look_for,
            {{"inner", lab.arg("inner")}, {"outer", lab.arg("outer")}}},
           gl::to_picture(chart.plot));
  lab.note(gl::chart_text(chart, 0));
}

void run(gl::Lab& lab) {
  const auto inner = static_cast<float>(lab.arg_num("inner"));
  const auto outer = static_cast<float>(lab.arg_num("outer"));
  const int n = static_cast<int>(lab.arg_num("points"));
  const in::Zone zones[] = {in::Zone::axial, in::Zone::radial, in::Zone::scaled_radial,
                            in::Zone::sloped};

  gl::LineChart length;
  length.frame = {"OUTPUT AGAINST PUSH, ON A DIAGONAL", "the stick pushed out along 30 degrees",
                  "push length", "output length"};
  length.spikes = false;
  gl::LineChart cardinal;
  cardinal.frame = {"SIDEWAYS OUTPUT NEAR A CARDINAL", "pushed full up, swept 0..0.4 sideways",
                    "sideways push", "sideways output"};
  cardinal.spikes = false;
  gl::LabTable table{
      "each zone at a few pushes",
      {"zone", "len 0.10", "len 0.20", "len 0.50", "len 0.90", "up+0.10 x", "up+0.30 x"},
      {}};
  for (const in::Zone z : zones) {
    const in::StickShape shape{z, inner, outer, {}, 0};
    gl::Trace t{std::string(in::zone_name(z)), {}, {}};
    gl::Trace c{std::string(in::zone_name(z)), {}, {}};
    for (int i = 0; i <= n; i++) {
      const double m = static_cast<double>(i) / n;
      float x = static_cast<float>(m * 0.8660254037844386), y = static_cast<float>(m * 0.5);
      in::shape_stick(x, y, shape);
      t.x.push_back(m);
      t.y.push_back(std::sqrt(static_cast<double>(x) * x + static_cast<double>(y) * y));
      float sx = static_cast<float>(0.4 * m), sy = -1;
      in::shape_stick(sx, sy, shape);
      c.x.push_back(0.4 * m);
      c.y.push_back(sx);
    }
    length.traces.push_back(t);
    cardinal.traces.push_back(c);
    std::vector<std::string> row{std::string(in::zone_name(z))};
    for (const double m : {0.1, 0.2, 0.5, 0.9}) {
      float x = static_cast<float>(m), y = 0;
      in::shape_stick(x, y, shape);
      row.push_back(std::to_string(x).substr(0, 6));
      lab.metric(std::string(in::zone_name(z)) + ".len-" + std::to_string(m).substr(0, 4), x, "");
    }
    for (const double side : {0.1, 0.3}) {
      float x = static_cast<float>(side), y = -0.95f;
      in::shape_stick(x, y, shape);
      row.push_back(std::to_string(x).substr(0, 6));
    }
    table.rows.push_back(row);
  }
  lab.table(table);
  length.marks = {{inner, "INNER"}, {outer, "OUTER"}};
  shoot(lab, "length", gl::line_chart(length),
        "Each zone's output length (0..1) against how far the stick is pushed, along a diagonal. "
        "The grey lines mark the inner and outer edges.",
        "radial jumps from 0 to the push at the inner edge; scaled_radial and sloped rise from 0 "
        "without a step and reach 1 at the outer edge; axial on a diagonal starts later");
  shoot(lab, "cardinal", gl::line_chart(cardinal),
        "The sideways output while the stick is held full up and pushed sideways: how a zone "
        "treats a push that is nearly straight.",
        "axial and sloped hold the sideways output at 0 for a small sideways push (they snap to "
        "the cardinal), radial and scaled_radial pass it through");

  gl::LineChart curves;
  curves.frame = {"RESPONSE CURVES", "a trigger through its shape: exponent and anti-dead zone",
                  "push", "output"};
  curves.spikes = false;
  for (const float e : {1.0f, 1.5f, 2.0f, 3.0f}) {
    for (const float anti : {0.0f, 0.2f}) {
      if (anti > 0 && e != 1.0f && e != 2.0f) continue;
      gl::Trace t{
          "exponent " + std::to_string(e).substr(0, 3) + (anti > 0 ? " anti 0.2" : ""), {}, {}};
      for (int i = 0; i <= n; i++) {
        const double v = static_cast<double>(i) / n;
        t.x.push_back(v);
        t.y.push_back(in::shape_axis(static_cast<float>(v), {inner, outer, {e}, anti}));
      }
      curves.traces.push_back(t);
    }
  }
  shoot(lab, "curves", gl::line_chart(curves),
        "A one-way axis (a trigger, a pedal) through its dead zone and outer edge with each "
        "response curve; the anti-dead zone lifts the first output.",
        "every curve is 0 up to the inner edge and 1 from the outer one; a higher exponent sags "
        "lower in the middle; the anti lines start at 0.2");

  gl::LineChart ramps;
  ramps.frame = {"A KEY HELD AS AN AXIS", "held for 0.5 s, then let go; 120 Hz", "t (s)", "value"};
  ramps.spikes = false;
  ramps.bands.push_back({0, 0.5, "HELD"});
  struct Named {
    const char* name;
    in::Ramp r;
  };
  const Named named[] = {{"steer 6/9", {6, 9, 0.02f}},
                         {"tuck 4/12", {4, 12, 0.02f}},
                         {"brake 20/30", {20, 30, 0.02f}},
                         {"no ramp", {}}};
  for (const Named& r : named) {
    gl::Trace t{r.name, {}, {}};
    float v = 0;
    for (int i = 0; i <= 120; i++) {
      const double ts = i / 120.0;
      t.x.push_back(ts);
      t.y.push_back(v);
      v = in::ramp_toward(v, ts < 0.5 ? 1.0f : 0.0f, 1.0 / 120, r.r);
    }
    ramps.traces.push_back(t);
    lab.metric(std::string("ramp.") + r.name + ".at-0.1s", t.y[12], "");
  }
  shoot(lab, "ramps", gl::line_chart(ramps),
        "A key's push eased toward full while held and back to centre after (attack/release in "
        "1/s), as a game's key steering and tuck are.",
        "each line rises quickly then settles below 1 while held, falls faster than it rose, and "
        "reaches exactly 0; no ramp is a square step");
}

const gl::LabSpec kSpec{
    "input-shape",
    gl::LabKind::picture,
    "engine",
    "engine/input/shape.hpp",
    "input",
    "What does each dead zone, response curve and key ramp hand the game?",
    {
        {"inner", "0.15", "the dead zone, a share of the push"},
        {"outer", "0.95", "the push that already reads full"},
        {"points", "200", "points a line"},
    },
    "--points=60",
    run,
};
GE_LAB(kSpec);

}  // namespace
