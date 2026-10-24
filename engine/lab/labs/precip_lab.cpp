// LAB `precip-air`: what falls through the air (engine/wind/precip.hpp) read the way its readers
// read it, so a change shows as numbers and pictures: the rate and the visual range over the dial
// for dry snow, a sleet and rain, with the tiers' edges; a raindrop's terminal speed by its size
// and the median drop by the rate; the rain share by height round a snow line; and the wet ground
// through an afternoon of showers (rain filling the film and the puddles, the sun and the wind
// drying them), with the steady state each rate would leave. Every number is a pure function, so
// the lab is deterministic on every row.
//
// ge:summary Lab: precipitation's rate, tiers, visibility, drop speed, phase and the wet ground
// ge:keywords lab precip rain snow sleet rate tier visibility drop terminal-speed phase snowline
// ge:keywords wet puddles shower drying
// ge:skill water render labs
// ge:test tests/wind_precip_test.cpp
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "wind/precip.hpp"

namespace {

namespace gl = ge::lab;
namespace gw = ge::wind;

std::string fixed(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

void chart_shot(gl::Lab& lab, const std::string& id, const gl::Chart& chart,
                const std::string& caption, const std::string& look_for) {
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({id, chart.title, caption, look_for, {{"wind", lab.arg("wind")}}},
           gl::to_picture(chart.plot));
}

struct Mix {
  const char* name;
  double rain;
};
constexpr Mix kMixes[] = {{"snow", 0}, {"sleet", 0.5}, {"rain", 1}};

void dial(gl::Lab& lab) {
  const double wind = lab.arg_num("wind");
  gl::LineChart rate, seen;
  rate.frame = {"THE RATE OVER THE DIAL: SNOW, SLEET AND RAIN",
                "liquid-equivalent mm/h; the dashed lines are the rain tiers' edges", "intensity",
                "rate (mm/h)"};
  rate.log_y = true;
  rate.spikes = false;
  rate.rules = {2.5, 7.6, 50};
  seen.frame = {"WHAT ONE SEES THROUGH IT", "the visual range; dashed: 1 km and 500 m, snow's tiers",
                "intensity", "visibility (m)"};
  seen.log_y = true;
  seen.spikes = false;
  seen.rules = {1000, 500};
  gl::LabTable table{"The dial's stops: rate, visibility and tier (wind " + fixed(wind, 0) +
                         " m/s), and the storm the same fall makes in a 18 m/s wind",
                     {"intensity", "fall", "mm/h", "visibility m", "fall speed m/s", "tier",
                      "in 18 m/s"},
                     {}};
  for (const Mix& m : kMixes) {
    gl::Trace r{m.name, {}, {}}, v{m.name, {}, {}};
    for (int k = 2; k <= 100; ++k) {
      const gw::Precip p{k / 100.0, m.rain};
      r.x.push_back(p.intensity);
      r.y.push_back(gw::rate_of(p));
      v.x.push_back(p.intensity);
      v.y.push_back(gw::visibility_of(p));
    }
    rate.traces.push_back(r);
    seen.traces.push_back(v);
    for (const double i : {0.1, 0.25, 0.5, 0.75, 1.0}) {
      const gw::Precip p{i, m.rain};
      table.rows.push_back({fixed(i, 2), m.name, fixed(gw::rate_of(p), 2),
                            fixed(gw::visibility_of(p), 0), fixed(gw::fall_speed(p), 2),
                            gw::tier_name(gw::tier_of(p, wind)), gw::tier_name(gw::tier_of(p, 18))});
      lab.metric(std::string("rate.") + m.name + "." + fixed(i, 2), gw::rate_of(p), "mm/h");
      lab.metric(std::string("visibility.") + m.name + "." + fixed(i, 2), gw::visibility_of(p), "m");
    }
  }
  chart_shot(lab, "rate", gl::line_chart(rate),
             "The liquid-equivalent rate (log scale) against the dial, for dry snow, a half-and-half "
             "sleet and rain. The dashed lines are rain's light/moderate (2.5), moderate/heavy "
             "(7.6) and heavy/storm (50 mm/h) edges.",
             "three straight lines on the log scale, rain steepest; rain crosses 2.5 near 0.25 and "
             "50 near 0.8; snow ends at 10 mm/h");
  chart_shot(lab, "visibility", gl::line_chart(seen),
             "The visual range (log scale) against the dial: snow hides about ten times what rain "
             "does at the same rate.",
             "every line falls with the dial; snow under 1 km past the middle, rain under 1 km only "
             "near the top");
  lab.table(table);
}

void drops(gl::Lab& lab) {
  gl::LineChart line;
  line.frame = {"A RAINDROP'S TERMINAL SPEED BY ITS SIZE",
                "marked: the median drop at 1, 10 and 50 mm/h", "diameter (mm)", "speed (m/s)"};
  line.spikes = false;
  gl::Trace t{"terminal speed", {}, {}};
  for (int k = 1; k <= 60; ++k) {
    const double d = k / 10.0;
    t.x.push_back(d);
    t.y.push_back(gw::drop_speed(d));
  }
  line.traces.push_back(t);
  gl::LabTable table{"The median drop and its pace by the rate", {"mm/h", "D0 mm", "m/s"}, {}};
  for (const double r : {0.5, 1.0, 2.5, 10.0, 50.0, 150.0}) {
    const double d = gw::median_drop(r);
    if (r == 1 || r == 10 || r == 50) line.marks.push_back({d, fixed(r, 0) + " MM/H"});
    table.rows.push_back({fixed(r, 1), fixed(d, 2), fixed(gw::drop_speed(d), 2)});
    lab.metric("drop." + fixed(r, 1), gw::drop_speed(d), "m/s");
  }
  chart_shot(lab, "drops", gl::line_chart(line),
             "A drop's terminal speed in still air against its diameter (the exponential fit), "
             "with the median drop of three rain rates marked.",
             "a curve rising steeply to 2 mm and levelling under 9.65 m/s; the marks move right "
             "with the rate");
  lab.table(table);
}

void phase(gl::Lab& lab) {
  gl::LineChart line;
  line.frame = {"SNOW OR RAIN BY HEIGHT", "the freezing level at 1500 m; the melting band under it",
                "rain share", "height (m)"};
  line.spikes = false;
  gl::Trace t{"rain share", {}, {}};
  for (int y = 900; y <= 1800; y += 10) {
    t.x.push_back(gw::rain_share_at(y, 1500));
    t.y.push_back(y);
  }
  line.traces.push_back(t);
  line.rules = {1500, 1500 - gw::kMeltBand};
  chart_shot(lab, "phase", gl::line_chart(line),
             "The share of the fall that is rain against height, the freezing level at 1500 m: "
             "snow above it, melting through the 300 m band under it, rain below.",
             "0 above the upper dashed line, 1 under the lower, a smooth S between");
}

void shower(gl::Lab& lab) {
  const double wind = lab.arg_num("wind");
  // An afternoon: a drizzle, a downpour, a dry spell in the sun, a shower, then dry in the shade.
  struct Spell {
    double minutes, rate, sun;
  };
  constexpr Spell kSpells[] = {{40, 1, 0}, {30, 25, 0}, {60, 0, 1}, {20, 6, 0.3}, {90, 0, 0}};
  gl::LineChart line;
  line.frame = {"THE WET GROUND THROUGH AN AFTERNOON OF SHOWERS",
                "wind " + fixed(wind, 0) + " m/s; the rain rate's spells as bands", "t (min)",
                "share"};
  line.spikes = false;
  gl::Trace wet{"film", {}, {}}, pools{"puddles", {}, {}};
  gw::Wet w;
  double t = 0;
  for (const Spell& s : kSpells) {
    line.bands.push_back({t, t + s.minutes, fixed(s.rate, 0) + " MM/H"});
    const double dry = gw::dry_rate(wind, s.sun);
    for (int k = 0; k < static_cast<int>(s.minutes); ++k) {
      w = gw::step_wet(w, s.rate, dry, 60);
      t += 1;
      wet.x.push_back(t);
      wet.y.push_back(w.wet);
      pools.x.push_back(t);
      pools.y.push_back(w.puddles);
    }
    lab.metric("wet.after." + fixed(t, 0), w.wet, "share");
    lab.metric("puddles.after." + fixed(t, 0), w.puddles, "share");
  }
  line.traces = {wet, pools};
  chart_shot(lab, "shower", gl::line_chart(line),
             "The water film's share and the puddles' over five spells: a drizzle, a downpour, an "
             "hour of sun, a shower, an hour and a half dry in the shade.",
             "the film soaks within the drizzle; the puddles fill in the downpour and drain within a quarter "
             "hour; the sun dries faster than the shade");
  gl::LabTable table{"The steady wet ground by the rain rate", {"mm/h", "water mm", "film",
                                                                "puddles"},
                     {}};
  for (const double r : {0.2, 1.0, 2.5, 7.6, 20.0, 50.0}) {
    const gw::Wet s = gw::wet_steady(r, gw::dry_rate(wind, 0));
    table.rows.push_back({fixed(r, 1), fixed(s.water, 2), fixed(s.wet, 2), fixed(s.puddles, 2)});
  }
  lab.table(table);
}

void run(gl::Lab& lab) {
  dial(lab);
  drops(lab);
  phase(lab);
  shower(lab);
}

const gl::LabSpec kSpec{
    "precip-air",
    gl::LabKind::picture,
    "engine",
    "engine/wind/precip.hpp",
    "water",
    "Does the dial give rates and visual ranges in each tier's band, do drops fall at their size's "
    "pace, does the snow turn to rain through the melting band, and does the ground wet and dry "
    "as rain and sun say?",
    {{"wind", "4", "the wind, m/s, for the tiers and the drying"}},
    "--wind=4",
    run};
GE_LAB(kSpec);

}  // namespace
