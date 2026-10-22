// LAB `sled`: the snowmobile staged in exact situations (scenarios.hpp: a hand-built map, a
// moment, a scripted hand or the bot's) and measured: the top speed against the class's 140 km/h,
// the belt's sink at rest and as it planes, its slip, the height climbed and where it stalls, the
// circle full lock settles on, the most it rolled, the flights and the landings, the trunks, and
// whether and why the rider was thrown. One picture a scenario: the line from above over the
// snow (groomed white, powder blue, the trunk green), and the speed and the belt's sink under it.
//
// ge:summary Lab: the snowmobile scenarios run and measured; the line, speed and sink drawn
// ge:keywords lab sled snowmobile scenario top-speed sink float slip climb high-mark stall
// ge:keywords turn radius roll flight landing thrown trace
// ge:skill snowmobile labs
// ge:test tests/snowmobile_scenarios_test.cpp
#include <string>

#include "core/num.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
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

void run(ge::lab::Lab& lab) {
  const std::string want = lab.arg("scenario");
  const double hz = lab.arg_num("hz");
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
    const sm::SledRun r = sm::run_sled_scenario(sc, hz);
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
    },
    "--scenario=all",
    run,
};
GE_LAB(kSpec);

}  // namespace
