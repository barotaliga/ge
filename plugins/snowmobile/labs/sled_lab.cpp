// LAB `sled`: the snowmobile staged in exact situations (scenarios.hpp: a hand-built map, a
// moment, a scripted hand or the bot's) and measured: the top speed against the class's 140 km/h,
// the belt's sink at rest and as it planes, its slip, the height climbed and where it stalls, the
// circle full lock settles on, the most it rolled, the flights and the landings, the trunks, and
// whether and why the rider was thrown. One picture a scenario: the line from above over the
// snow (groomed white, powder blue, the trunk green), and the speed and the belt's sink under it.
// With --trace, every step as a series (`<id>.trace.tsv`, for `ge graph`) and charts of it.
//
// ge:summary Lab: the snowmobile scenarios run and measured; the line, speed and sink drawn
// ge:keywords lab sled snowmobile scenario top-speed sink float slip climb high-mark stall
// ge:keywords turn radius roll flight landing thrown trace per-step series charts trace-flag
// ge:skill snowmobile labs
// ge:test tests/snowmobile_scenarios_test.cpp
#include <string>

#include "core/num.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "lab/series_charts.hpp"
#include "snowmobile/scenarios.hpp"

namespace {

namespace sm = ge::snowmobile;
namespace num = ge::core::num;
using ge::lab::Picture;
using ge::lab::Rgba;

constexpr Rgba kGroomed{236, 240, 244};
constexpr Rgba kPowder{196, 214, 236};
constexpr Rgba kSnowLine{40, 90, 200};
constexpr Rgba kAirLine{235, 140, 20};
constexpr Rgba kSinkLine{150, 60, 170};
constexpr Rgba kThrownMark{210, 30, 30};
constexpr Rgba kTrunk{30, 120, 50};
constexpr Rgba kInk{20, 20, 30};
constexpr Rgba kGrid{200, 200, 210};
// The sink graph's full height, m.
constexpr double kSinkScale = 0.4;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

// The line from above (top) and the speed and the belt's sink against time (bottom).
Picture draw_run(const sm::SledScenario& sc, const ge::mountain::mapgen::Level& level,
                 const sm::SledRun& run) {
  constexpr int kW = 520, kPlanH = 300, kGraphH = 150, kPad = 16;
  Picture p = ge::lab::create_picture(kW, kPlanH + kGraphH, Rgba{250, 250, 252});
  double x0 = run.trace.front().x, x1 = x0, z0 = run.trace.front().z, z1 = z0;
  for (const auto& t : run.trace) {
    x0 = num::min(x0, t.x), x1 = num::max(x1, t.x);
    z0 = num::min(z0, t.z), z1 = num::max(z1, t.z);
  }
  x0 = num::min(x0, run.end.x), x1 = num::max(x1, run.end.x);
  z0 = num::min(z0, run.end.z), z1 = num::max(z1, run.end.z);
  const double cx = (x0 + x1) / 2, cz = (z0 + z1) / 2;
  const double span = num::max(40.0, num::max(x1 - x0, z1 - z0) * 1.1);
  const double scale = num::min((kW - 2.0 * kPad) / span, (kPlanH - 2.0 * kPad) / span);
  const auto px = [&](double x) { return kW / 2.0 + (x - cx) * scale; };
  const auto pz = [&](double z) { return kPlanH / 2.0 + (z - cz) * scale; };
  for (int y = kPad; y < kPlanH - kPad; y += 2) {
    for (int x = kPad; x < kW - kPad; x += 2) {
      const double wx = cx + (x - kW / 2.0) / scale;
      const double wz = cz + (y - kPlanH / 2.0) / scale;
      ge::lab::fill_rect(p, x, y, 2, 2, level.packed_at(wx, wz) > 0.5 ? kGroomed : kPowder);
    }
  }
  for (const auto& tr : level.trees) {
    ge::lab::fill_disc(p, px(tr.x), pz(tr.z), num::max(2.0, tr.radius * scale * 3), kTrunk);
  }
  for (size_t i = 1; i < run.trace.size(); i++) {
    const auto& a = run.trace[i - 1];
    const auto& b = run.trace[i];
    ge::lab::draw_line(p, px(a.x), pz(a.z), px(b.x), pz(b.z), b.airborne ? kAirLine : kSnowLine, 2);
  }
  ge::lab::fill_disc(p, px(run.trace.front().x), pz(run.trace.front().z), 4, kInk);
  if (run.thrown != sm::Thrown::none) {
    ge::lab::draw_ring(p, px(run.end.x), pz(run.end.z), 7, kThrownMark);
  }
  ge::lab::draw_text(p, kPad, 4, std::string(sc.id) + "  " + fixed(span, 0) + " M ACROSS", kInk);
  const int g0 = kPlanH + 8, gh = kGraphH - 24;
  const double top_kmh = num::max(10.0, run.top_speed * 3.6);
  const double tmax = num::max(1.0, run.trace.back().t);
  for (int v = 0; v <= static_cast<int>(top_kmh); v += 20) {
    const double y = g0 + gh - v / top_kmh * gh;
    ge::lab::draw_line(p, kPad, y, kW - kPad, y, kGrid);
  }
  const auto gx = [&](double t) { return kPad + t / tmax * (kW - 2.0 * kPad); };
  const auto gy = [&](double v) { return g0 + gh - v * 3.6 / top_kmh * gh; };
  const auto gs = [&](double sink) { return g0 + gh - num::min(1.0, sink / kSinkScale) * gh; };
  for (size_t i = 1; i < run.trace.size(); i++) {
    const auto& a = run.trace[i - 1];
    const auto& b = run.trace[i];
    ge::lab::draw_line(p, gx(a.t), gs(a.sink), gx(b.t), gs(b.sink), kSinkLine, 1);
    ge::lab::draw_line(p, gx(a.t), gy(a.speed), gx(b.t), gy(b.speed),
                       b.airborne ? kAirLine : kSnowLine, 2);
  }
  ge::lab::draw_text(p, kPad, kPlanH + kGraphH - 12,
                     "SPEED 0-" + fixed(top_kmh, 0) + " KM/H, SINK 0-" + fixed(kSinkScale, 1) +
                         " M, OVER " + fixed(tmax, 1) + " S",
                     kInk);
  return p;
}

// The run step by step under --trace: every step's speeds, the belt's sink and slip, the engine,
// the attitude and the controls as a series, its flights, landings, trunks and throw marked.
struct StepTrace {
  static constexpr double kDeg = 180 / ge::core::kPi;
  ge::lab::Series s =
      ge::lab::make_series({"t_s", "speed", "way", "vy", "sink", "slip", "rpm", "roll", "pitch",
                            "ski_angle", "throttle", "brake", "steer", "lean", "airborne"});
  std::vector<ge::lab::Mark> marks;
  std::vector<ge::lab::Band> bands;
  double flight_from = -1;

  void step(const sm::SledStep& st) {
    const sm::SledState& m = st.sled;
    const sm::SledControls& c = st.controls;
    double sink = 0;
    for (const double v : m.sinks) sink += v;
    ge::lab::add_row(
        s, {st.t, m.speed * 3.6, m.way * 3.6, m.vy, sink / static_cast<double>(m.sinks.size()),
            m.slip, m.rpm, m.roll * kDeg, m.pitch * kDeg, m.ski_angle * kDeg, c.throttle, c.brake,
            c.steer, c.lean, m.airborne ? 1.0 : 0.0});
    const sm::SledEvents& e = st.events;
    if (e.took_off) marks.push_back({st.t, "TAKE-OFF"});
    if (e.landed) marks.push_back({st.t, "LAND"});
    if (e.hit > 0) marks.push_back({st.t, "TRUNK"});
    if (e.thrown != sm::Thrown::none) {
      marks.push_back({st.t, std::string("THROWN ") + sm::thrown_name(e.thrown)});
    }
    if (m.airborne && flight_from < 0) flight_from = st.t;
    if (!m.airborne && flight_from >= 0) {
      bands.push_back({flight_from, st.t, "AIR"});
      flight_from = -1;
    }
  }
};

void chart_trace(ge::lab::Lab& lab, const sm::SledScenario& sc, StepTrace& tr, double hz) {
  const double end = tr.s.rows() > 0 ? tr.s.find("t_s")->num.back() : 0;
  if (tr.flight_from >= 0) tr.bands.push_back({tr.flight_from, end, "AIR"});
  tr.s.set("title", std::string("the snowmobile scenario ") + sc.id + ", step by step");
  tr.s.set("x", "t_s");
  for (const auto& [col, unit] :
       std::vector<std::pair<std::string, std::string>>{{"t_s", "s"},
                                                        {"speed", "km/h"},
                                                        {"way", "km/h"},
                                                        {"vy", "m/s"},
                                                        {"sink", "m"},
                                                        {"slip", "m/s"},
                                                        {"roll", "deg"},
                                                        {"pitch", "deg"},
                                                        {"ski_angle", "deg"}}) {
    tr.s.set("unit." + col, unit);
  }
  const std::string k = sc.id;
  const ge::lab::SeriesCharts out{
      k + ".",
      k,
      k + ", " + fixed(end, 1) + " s at " + fixed(hz, 0) + " Hz",
      {{"scenario", k}, {"hz", fixed(hz, 0)}, {"trace", "yes"}},
      tr.marks,
      tr.bands,
      "Grey lines are the run's events (take-off, landing, a trunk, the throw), grey bands "
      "flights."};
  ge::lab::chart_series(
      lab, tr.s, out,
      {{"speed",
        "SPEED AND WAY, " + k,
        "km/h",
        {"speed", "way"},
        "The machine's speed (vertical included) and its way along its nose.",
        "the two lines part only in a slide; smooth but at a landing or a trunk"},
       {"sink",
        "THE BELT'S SINK, " + k,
        "sink (m)",
        {"sink"},
        "The belt's mean sink into the snow over its probes.",
        "deep at rest in powder, rising as it planes; a ring only at a landing"},
       {"slip",
        "THE BELT'S SLIP, " + k,
        "slip (m/s)",
        {"slip"},
        "The belt's mean slip over the snow.",
        "a burst off the line or on a climb, then settling"},
       {"engine",
        "THE ENGINE, " + k,
        "rpm",
        {"rpm"},
        "The engine's revolutions a minute.",
        "rises with the throttle and holds; no one-step jumps"},
       {"attitude",
        "ATTITUDE, " + k,
        "angle (deg)",
        {"roll", "pitch", "ski_angle"},
        "The machine's roll and pitch and the skis' turn off the nose.",
        "the roll follows the steer and the rider's lean; no oscillation that grows"},
       {"controls",
        "THE HANDS, " + k,
        "control (-1..1)",
        {"throttle", "brake", "steer", "lean"},
        "The controls the scripted hand or the bot held each step.",
        "the script's steps where the scenario says; the bot's hands without one-step chatter"}});
}

void run(ge::lab::Lab& lab) {
  const std::string want = lab.arg("scenario");
  const double hz = lab.arg_num("hz");
  const bool trace = lab.arg_bool("trace");
  ge::lab::LabTable table{
      "scenarios",
      {"scenario", "top km/h", "end km/h", "metres", "climbed m", "sink m", "slip m/s", "roll deg",
       "radius m", "stall s", "flights", "landing m/s", "hits", "thrown", "closest m"},
      {}};
  int ran = 0;
  for (const sm::SledScenario& sc : sm::sled_scenarios()) {
    if (want != "all" && want != sc.id) continue;
    ran++;
    const auto level = sm::sled_scenario_level(sc);
    StepTrace tr;
    const sm::SledRun r =
        trace ? sm::run_sled_scenario(sc, hz, [&](const sm::SledStep& st) { tr.step(st); })
              : sm::run_sled_scenario(sc, hz);
    if (trace) chart_trace(lab, sc, tr, hz);
    const std::string k = sc.id;
    lab.metric(k + ".top-speed", r.top_speed * 3.6, "km/h");
    lab.metric(k + ".end-speed", r.end.speed * 3.6, "km/h");
    lab.metric(k + ".distance", r.distance, "m");
    lab.metric(k + ".climbed", r.climbed, "m");
    lab.metric(k + ".sink", r.rest_sink, "m");
    lab.metric(k + ".slip", r.top_slip, "m/s");
    lab.metric(k + ".roll", r.max_roll * 180 / ge::core::kPi, "deg");
    lab.metric(k + ".radius", r.radius, "m");
    lab.metric(k + ".stall", r.stall_time, "s");
    if (r.closest >= 0) lab.metric(k + ".closest", r.closest, "m");
    table.rows.push_back({k, fixed(r.top_speed * 3.6, 1), fixed(r.end.speed * 3.6, 1),
                          fixed(r.distance, 1), fixed(r.climbed, 1), fixed(r.rest_sink, 3),
                          fixed(r.top_slip, 1), fixed(r.max_roll * 180 / ge::core::kPi, 1),
                          fixed(r.radius, 1), fixed(r.stall_time, 2), std::to_string(r.flights),
                          fixed(r.hardest_landing, 1), std::to_string(r.hits),
                          sm::thrown_name(r.thrown), fixed(r.closest, 1)});
    lab.shot({k,
              "SLED " + k,
              std::string(sc.what) +
                  ". Top: the line from above at one scale, +x right and +z (downhill) down; "
                  "groomed snow white, powder blue, a trunk green, the start a black dot, a red "
                  "ring where the rider was thrown; the line blue on the snow, orange in the air. "
                  "Bottom: speed against time (blue, a grid line every 20 km/h) and the belt's "
                  "mean sink (purple, 0 to 0.4 m).",
              "the line and the speed do what the first sentence says; a red ring only where "
              "the scenario throws the rider",
              {{"scenario", k}, {"hz", fixed(hz, 0)}}},
             draw_run(sc, level, r));
  }
  if (ran == 0) {
    lab.fail("no scenario is called '" + want + "'",
             "Run `make lab LAB=sled ARGS=--help` and pick one of "
             "plugins/snowmobile/scenarios.cpp's ids, or `all`.");
  }
  lab.table(std::move(table));
}

const ge::lab::LabSpec kSpec{
    "sled",
    ge::lab::LabKind::scenario,
    "plugin:snowmobile",
    "plugins/snowmobile (step_sled, the body, the drive, the pilot)",
    "snowmobile",
    "Staged in each exact situation, does the snowmobile ride as a mountain sled does: its top "
    "speed, its sink and float, the climb and the high-mark, the turn, the carve's roll, the "
    "drop ridden out and the cliff, the trunk and the parking brake?",
    {
        {"scenario", "all", "one scenario's id (plugins/snowmobile/scenarios.cpp), or all"},
        {"hz", "120", "steps a second"},
        {"trace", "", "also write each scenario's every step (<id>.trace.tsv) and chart it", true},
    },
    "--scenario=all",
    run,
};
GE_LAB(kSpec);

}  // namespace
