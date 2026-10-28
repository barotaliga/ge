// LAB `snow-weather`: snow over time (engine/snow/time.hpp) charted, so a change to a law or a
// default shows as a curve that moved. Five charts: a ski groove filling in storms of three
// strengths; a fresh fall settling over days at three temperatures; a ploughed road filling with
// drifted snow under three winds and no fall; a crust's bearing load through a night of frost
// against a standing boot and a carving skier; and a churned slope's cross-section before and after
// a groomer's pass. The table gives each run's number; the lab fails when the groomer moves the
// mass by a unit, when any layer's mass ledger breaks, or when a 5 cm/h storm takes outside half an
// hour to three hours to bury a ski track.
//
// ge:summary Lab: snow over time charted: storms filling tracks, settling, drift, crust, grooming
// ge:keywords lab snow weather time storm fill settling drift wind crust groom groomer corduroy
// ge:keywords chart ledger
// ge:skill snow labs
// ge:test tests/snow_time_test.cpp
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "snow/layer.hpp"
#include "snow/press.hpp"
#include "snow/reference.hpp"
#include "snow/time.hpp"

namespace {

namespace gl = ge::lab;
namespace sn = ge::snow;

constexpr double kHour = 3600;
constexpr double kBoot = 785;      // N, a skier standing on one boot
constexpr double kCarving = 1570;  // N, the same skier at 2 g in a turn

std::string fix(double v, int digits) {
  char b[48];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

void chart(gl::Lab& lab, const std::string& id, gl::LineChart c, const std::string& caption,
           const std::string& look_for) {
  c.spikes = false;
  const gl::Chart drawn = gl::line_chart(c);
  const std::string svg = gl::to_svg(drawn.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({id, c.frame.title, caption, look_for, {}}, gl::to_picture(drawn.plot));
}

sn::LayerSpec flat(double depth, const sn::Weather* w, double cell) {
  sn::LayerSpec s;
  s.region = {-12, -12, 12, 12};
  s.cover.depth = depth;
  s.cell = cell;
  s.weather = w;
  return s;
}

void carve(sn::Layer& layer, double x, double t) {
  const sn::ReferenceContact rc = sn::reference_contact(0);
  for (int i = 0; i < 24; i++) sn::press(layer, sn::place(rc.imprint, x, -3 + 0.25 * i, 0), t);
}

void check_ledger(gl::Lab& lab, const sn::Layer& l, const std::string& what) {
  if (sn::mass_units(l) + l.spilled != l.mass_in) {
    lab.fail(what + ": the tiles hold " + std::to_string(sn::mass_units(l)) + " units and " +
                 std::to_string(l.spilled) + " spilled, but were given " +
                 std::to_string(l.mass_in),
             "Find the write that skips `bring_to` or miscounts `mass_in` in engine/snow/time.cpp "
             "or press.cpp; tests/snow_time_test.cpp's ledger test narrows it (skill: snow).");
  }
}

void run(gl::Lab& lab) {
  const int points = static_cast<int>(lab.arg_num("points"));
  const double cell = lab.arg_num("cell");
  gl::LabTable table{"snow over time", {"run", "number", "unit"}, {}};

  // A GROOVE FILLING: a carved track's depth under the cover at its axis over six hours of snow.
  gl::LineChart fill;
  fill.frame = {"A ski groove filling in a storm",
                "carved in 40 cm of powder at -8 C, then snow at 2, 5 and 10 cm/h", "hours",
                "groove depth (cm)"};
  const double rates[3] = {2, 5, 10};
  for (const double rate : rates) {
    sn::Weather w;
    w.fall = {{0}, {rate / 3600}};  // 1 cm/h at 100 kg/m3 is 1 kg/m2/h
    w.air = {{0}, {-8}};
    sn::SnowWorld world;
    sn::Layer& layer = sn::add_layer(world, flat(0.4, &w, cell));
    carve(layer, 0, 0);
    gl::Trace t{fix(rate, 0) + " cm/h", {}, {}};
    double gone = -1;
    for (int i = 0; i <= points; i++) {
      const double hours = 6.0 * i / points;
      const double drop = sn::sample(layer, 0, 0, hours * kHour).drop;
      t.x.push_back(hours);
      t.y.push_back(100 * drop);
      if (gone < 0 && drop < 0.005) gone = hours;
    }
    fill.traces.push_back(t);
    check_ledger(lab, layer, "the storm layer");
    lab.metric("fill." + fix(rate, 0) + "cmh.hours", gone, "h");
    table.rows.push_back({"track buried at " + fix(rate, 0) + " cm/h", fix(gone, 2), "h"});
    if (rate == 5 && (gone < 1 || gone > 3)) {
      lab.fail("a 5 cm/h storm buried the ski track in " + fix(gone, 2) + " h, outside 1-3 h",
               "Read `filled` and Weather::fill in engine/snow/time.hpp, then rerun `make lab "
               "LAB=snow-weather` (skill: snow).");
    }
  }
  chart(lab, "fill", fill,
        "How far a carved ski groove's axis stands under the cover beside it as snow falls into "
        "it: a hollow catches the fall and Weather::fill times its share more, until it is level.",
        "Every curve falling from about 20 cm to zero and staying there: the 10 cm/h storm in "
        "about an hour and a quarter, 5 cm/h in about two hours, 2 cm/h in about four and a half");

  // SETTLING: 60 kg/m2 of new snow over 20 cm of old, then five quiet days.
  gl::LineChart settle;
  settle.frame = {"A fresh fall settling", "60 kg/m2 fallen in 6 h onto 20 cm, then five days",
                  "days", "depth (cm)"};
  const double temps[3] = {-15, -5, 0};
  for (const double temp : temps) {
    sn::Weather w;
    w.fall = {{0, 6 * kHour, 6 * kHour + 1}, {10.0 / 3600, 10.0 / 3600, 0}};
    w.air = {{0}, {temp - 0.5}};
    sn::SnowWorld world;
    sn::Layer& layer = sn::add_layer(world, flat(0.2, &w, cell));
    gl::Trace t{fix(temp - 0.5, 1) + " C", {}, {}};
    for (int i = 0; i <= points; i++) {
      const double days = 5.0 * i / points;
      t.x.push_back(days);
      t.y.push_back(100 * sn::sample(layer, 0, 0, days * 86400).depth);
    }
    settle.traces.push_back(t);
    const sn::Column end = sn::sample(layer, 0, 0, 5 * 86400);
    table.rows.push_back(
        {"density after 5 days at " + fix(temp - 0.5, 1) + " C", fix(end.density, 0), "kg/m3"});
  }
  chart(lab, "settle", settle,
        "The column's depth as six hours of snow fall onto it and it settles for five days: the "
        "density relaxes toward Weather::settled_density, faster the warmer the air.",
        "Each curve rising for the first quarter day, then sinking and flattening; the warmest "
        "lowest and flattest by day five, the coldest still sinking");

  // DRIFT: a road ploughed to the ground across the wind, filling with no fall.
  gl::LineChart drift;
  drift.frame = {"A ploughed road drifting in",
                 "40 cm of powder cut to the ground, -10 C, no fall, the wind at 6, 10 and 14 m/s",
                 "hours", "road's depth under the cover (cm)"};
  const double winds[3] = {6, 10, 14};
  for (const double wind : winds) {
    sn::Weather w;
    w.air = {{0}, {-10}};
    w.wind = {{0}, {wind}};
    sn::SnowWorld world;
    sn::Layer& layer = sn::add_layer(world, flat(0.4, &w, cell));
    sn::Imprint plough{sn::box(0, 0, 0, 8, 1.5), 0, 0, 0.4, 1, 8, 1.0, sn::Motion::plough};
    sn::press(layer, plough, 0);
    gl::Trace t{fix(wind, 0) + " m/s", {}, {}};
    double filled_at = -1;
    for (int i = 0; i <= points; i++) {
      const double hours = 8.0 * i / points;
      const double drop = sn::sample(layer, 0, 0, hours * kHour).drop;
      t.x.push_back(hours);
      t.y.push_back(100 * drop);
      if (filled_at < 0 && drop < 0.005) filled_at = hours;
    }
    drift.traces.push_back(t);
    check_ledger(lab, layer, "the drift layer");
    table.rows.push_back({"road filled at " + fix(wind, 0) + " m/s", fix(filled_at, 2), "h"});
  }
  chart(lab, "drift", drift,
        "A road cut to the ground filling with snow the wind carries across it, nothing falling: "
        "the saltation flux past the friction velocity Weather::drift_threshold lifts.",
        "Each a straight fall from 40 cm that stops at zero: 14 m/s filled in about two and a "
        "quarter hours, 10 m/s in about three and a quarter, 6 m/s in under seven");

  // CRUST: a warm day, then a night's frost; what the lid bears.
  sn::Weather crust;
  crust.air = {{0, 12 * kHour, 13 * kHour}, {4, 4, -5}};
  gl::LineChart lid;
  lid.frame = {"A night's crust and what it bears", "+4 C until noon, then -5 C", "hour of the day",
               "crust bears (N)"};
  gl::Trace bears{"crust_load", {}, {}};
  for (int i = 0; i <= points; i++) {
    const double hour = 12 + 12.0 * i / points;
    bears.x.push_back(hour);
    bears.y.push_back(sn::crust_load(crust, hour * kHour));
  }
  lid.traces.push_back(bears);
  lid.rules = {kBoot, kCarving};
  chart(lab, "crust", lid,
        "The load a melt-freeze crust bears before it breaks, growing with the frost since the "
        "thaw (Stefan: thickness with the root of the degree-hours). The dashed lines are a "
        "skier standing on one boot (785 N) and carving at 2 g (1570 N).",
        "Zero until the air passes freezing, then rising and bending over: the boot's line "
        "crossed late in the evening, the carving line not before midnight");
  sn::SnowWorld cw;
  sn::Layer& cl = sn::add_layer(cw, flat(1.0, &crust, cell));
  sn::Imprint boot = sn::reference_contact(2).imprint;
  const sn::Pressed stood = sn::press(cl, sn::place(boot, -3, 0, 0), 23 * kHour);
  boot.load = kCarving;
  const sn::Pressed turned = sn::press(cl, sn::place(boot, 3, 0, 0), 23 * kHour);
  check_ledger(lab, cl, "the crust layer");
  table.rows.push_back({"crust bears at 23:00", fix(sn::crust_load(crust, 23 * kHour), 0), "N"});
  table.rows.push_back({"boot at 23:00 sinks", fix(100 * stood.sink, 1), "cm"});
  table.rows.push_back({"2 g turn at 23:00 sinks", fix(100 * turned.sink, 1), "cm"});
  lab.metric("crust.boot-sink", stood.sink, "m");
  lab.metric("crust.carve-sink", turned.sink, "m");

  // THE GROOMER: five carved tracks and a boot path churned, then a 4 m swath.
  sn::Weather cold;
  cold.air = {{0}, {-6}};
  sn::SnowWorld gw;
  sn::Layer& gl_ = sn::add_layer(gw, flat(0.4, &cold, cell));
  for (int k = 0; k < 5; k++) carve(gl_, -1.6 + 0.8 * k, 0);
  const sn::ReferenceContact walk = sn::reference_contact(2);
  for (int i = 0; i < 8; i++) sn::press(gl_, sn::place(walk.imprint, 0.3, -3 + 0.75 * i, 0), 0);
  gl::LineChart cross;
  cross.frame = {"A groomer's pass across a churned slope",
                 "the snow's surface across z = 0 before and after a 4 m swath", "x (m)",
                 "surface over the ground (cm)"};
  gl::Trace before{"before", {}, {}};
  for (int i = 0; i <= 4 * points; i++) {
    const double x = -3 + 6.0 * i / (4 * points);
    before.x.push_back(x);
    before.y.push_back(100 * sn::sample(gl_, x, 0, 600).depth);
  }
  const uint64_t units = sn::mass_units(gl_), given = gl_.mass_in;
  const sn::Groomed g = sn::groom(gl_, sn::capsule(0, -4, 0, 4, 2.0), 600);
  gl::Trace after{"after", {}, {}};
  for (const double x : before.x) {
    after.x.push_back(x);
    after.y.push_back(100 * sn::sample(gl_, x, 0, 600).depth);
  }
  cross.traces = {before, after};
  chart(lab, "groom", cross,
        "The surface across five carved tracks and a boot path before a groomer passes, and "
        "after: every sample in the swath levelled to the same mass at the groomed density.",
        "Before: six grooves 20-30 cm deep in the 40 cm cover; after: one flat floor 4 m wide "
        "about 6 cm deep (the powder packed to the groomed density), the cover either side "
        "unchanged");
  check_ledger(lab, gl_, "the groomed layer");
  // The tiles the swath allocated bring their cover with them; nothing else may change the mass.
  if (sn::mass_units(gl_) - units != gl_.mass_in - given) {
    lab.fail("the groomer moved the mass by " +
                 std::to_string(static_cast<int64_t>(sn::mass_units(gl_) - units) -
                                static_cast<int64_t>(gl_.mass_in - given)) +
                 " units",
             "Read `groom` in engine/snow/time.cpp (the units are shared out base + rest), then "
             "rerun tests/snow_time_test.cpp (skill: snow).");
  }
  table.rows.push_back({"groomed samples", std::to_string(g.samples), ""});
  table.rows.push_back({"mass through the swath", fix(g.mass, 1), "kg"});
  lab.metric("groom.mass", g.mass, "kg");
  lab.table(table);
}

const gl::LabSpec kSpec{
    "snow-weather",
    gl::LabKind::scenario,
    "engine",
    "engine/snow/time.hpp",
    "snow",
    "What does the weather do to a layer of snow over hours and days: fill, settle, drift, crust, "
    "groom?",
    {
        {"points", "48", "points along each curve"},
        {"cell", "0.1", "m between the layers' samples"},
    },
    "--points=12 --cell=0.2",
    run,
};
GE_LAB(kSpec);

}  // namespace
