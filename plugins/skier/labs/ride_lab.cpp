// LAB `ride`: the skier staged in exact situations (scenarios.hpp: a hand-built map, a moment, a
// scripted input) and measured: the top speed against the terminal `limits.hpp` expects, the
// distance, the hardest sideways load and the tightest path, the flights and their landings, the
// stop, the wipeout. One picture a scenario: the line from above over the snow (groomed white,
// powder blue, the trunk green) coloured by what he was doing, and his speed under it.
//
// ge:summary Lab: the ride scenarios run and measured; the line and the speed drawn
// ge:keywords lab ride scenario scenarios skier measure top-speed terminal carve stop flight
// ge:keywords landing wipeout trace
// ge:skill skier labs
// ge:test tests/skier_scenarios_test.cpp
#include <string>
#include <vector>

#include "core/math.hpp"
#include "core/num.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "skier/limits.hpp"
#include "skier/scenarios.hpp"
#include "skier/skis.hpp"

namespace {

namespace sk = ge::skier;
namespace num = ge::core::num;
using ge::lab::Picture;
using ge::lab::Rgba;

constexpr Rgba kGroomed{236, 240, 244};
constexpr Rgba kPowder{196, 214, 236};
constexpr Rgba kSnowLine{40, 90, 200};
constexpr Rgba kAirLine{235, 140, 20};
constexpr Rgba kDownLine{210, 30, 30};
constexpr Rgba kTrunk{30, 120, 50};
constexpr Rgba kInk{20, 20, 30};
constexpr Rgba kGrid{200, 200, 210};

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

Rgba line_colour(const sk::TraceSample& t) {
  return t.down ? kDownLine : t.on_snow ? kSnowLine : kAirLine;
}

// The line from above (top) and the speed against time (bottom).
Picture draw_run(const sk::Scenario& sc, const ge::mountain::mapgen::Level& level,
                 const sk::ScenarioRun& run) {
  constexpr int kW = 520, kPlanH = 300, kGraphH = 150, kPad = 16;
  Picture p = ge::lab::create_picture(kW, kPlanH + kGraphH, Rgba{250, 250, 252});
  // The plan: the run's bounding box (and at least 40 m) at one scale, +x right, +z down.
  double x0 = run.trace.front().x, x1 = x0, z0 = run.trace.front().z, z1 = z0;
  for (const auto& t : run.trace) {
    x0 = num::min(x0, t.x), x1 = num::max(x1, t.x);
    z0 = num::min(z0, t.z), z1 = num::max(z1, t.z);
  }
  const double cx = (x0 + x1) / 2, cz = (z0 + z1) / 2;
  const double span = num::max(40.0, num::max(x1 - x0, z1 - z0) * 1.1);
  const double scale = num::min((kW - 2.0 * kPad) / span, (kPlanH - 2.0 * kPad) / span);
  const auto px = [&](double x) { return kW / 2.0 + (x - cx) * scale; };
  const auto pz = [&](double z) { return kPlanH / 2.0 + (z - cz) * scale; };
  for (int y = kPad; y < kPlanH - kPad; y += 2) {
    for (int x = kPad; x < kW - kPad; x += 2) {
      const double wx = cx + (x - kW / 2.0) / scale;
      const double wz = cz + (y - kPlanH / 2.0) / scale;
      const double packed = level.packed_at(wx, wz);
      ge::lab::fill_rect(p, x, y, 2, 2, packed > 0.5 ? kGroomed : kPowder);
    }
  }
  for (const auto& tr : level.trees) {
    ge::lab::fill_disc(p, px(tr.x), pz(tr.z), num::max(2.0, tr.radius * scale * 3), kTrunk);
  }
  for (size_t i = 1; i < run.trace.size(); i++) {
    const auto& a = run.trace[i - 1];
    const auto& b = run.trace[i];
    ge::lab::draw_line(p, px(a.x), pz(a.z), px(b.x), pz(b.z), line_colour(b), 2);
  }
  ge::lab::fill_disc(p, px(run.trace.front().x), pz(run.trace.front().z), 4, kInk);
  ge::lab::draw_text(p, kPad, 4, std::string(sc.id) + "  " + fixed(span, 0) + " M ACROSS", kInk);
  // The speed: 0 at the bottom, the run's top at the top, a line every 10 km/h.
  const int g0 = kPlanH + 8, gh = kGraphH - 24;
  const double top_kmh = num::max(10.0, run.top_speed * 3.6);
  const double tmax = num::max(1.0, run.trace.back().t);
  for (int v = 0; v <= static_cast<int>(top_kmh); v += 10) {
    const double y = g0 + gh - v / top_kmh * gh;
    ge::lab::draw_line(p, kPad, y, kW - kPad, y, kGrid);
  }
  for (size_t i = 1; i < run.trace.size(); i++) {
    const auto& a = run.trace[i - 1];
    const auto& b = run.trace[i];
    const auto gx = [&](double t) { return kPad + t / tmax * (kW - 2.0 * kPad); };
    const auto gy = [&](double v) { return g0 + gh - v * 3.6 / top_kmh * gh; };
    ge::lab::draw_line(p, gx(a.t), gy(a.speed), gx(b.t), gy(b.speed), line_colour(b), 2);
  }
  ge::lab::draw_text(p, kPad, kPlanH + kGraphH - 12,
                     "SPEED 0-" + fixed(top_kmh, 0) + " KM/H OVER " + fixed(tmax, 1) + " S", kInk);
  return p;
}

void run(ge::lab::Lab& lab) {
  const std::string want = lab.arg("scenario");
  const double hz = lab.arg_num("hz");
  const sk::SkiSpec& spec = sk::kReferenceSkis;
  ge::lab::LabTable table{"scenarios",
                          {"scenario", "top km/h", "end km/h", "metres", "max g", "radius m",
                           "flights", "longest s", "landing g", "stop s", "wipeout"},
                          {}};
  int ran = 0;
  for (const sk::Scenario& sc : sk::scenarios()) {
    if (want != "all" && want != sc.id) continue;
    ran++;
    const auto level = sk::scenario_level(sc);
    const sk::ScenarioRun r = sk::run_scenario(sc, spec, hz);
    const std::string k = sc.id;
    lab.metric(k + ".top-speed", r.top_speed * 3.6, "km/h");
    lab.metric(k + ".end-speed", r.end.speed * 3.6, "km/h");
    lab.metric(k + ".distance", r.distance, "m");
    lab.metric(k + ".max-g", r.max_g_lat, "g");
    lab.metric(k + ".min-radius", r.min_radius, "m");
    lab.metric(k + ".flights", r.flights, "count");
    lab.metric(k + ".longest-flight", r.longest_flight, "s");
    lab.metric(k + ".landing-g", r.max_landing_g, "g");
    lab.metric(k + ".stop", r.stop_time, "s");
    if (sc.schuss) {
      const double cda = sc.input(0, r.end).tuck > 0 ? spec.cda_tuck : spec.cda_upright;
      lab.metric(k + ".terminal", sk::terminal_speed(spec, sc.pitch, cda, sc.packed) * 3.6, "km/h");
    }
    table.rows.push_back({k, fixed(r.top_speed * 3.6, 1), fixed(r.end.speed * 3.6, 1),
                          fixed(r.distance, 1), fixed(r.max_g_lat, 2), fixed(r.min_radius, 1),
                          std::to_string(r.flights), fixed(r.longest_flight, 2),
                          fixed(r.max_landing_g, 1), fixed(r.stop_time, 2),
                          sk::wipeout_name(r.cause)});
    lab.shot({k,
              "RIDE " + k,
              std::string(sc.what) +
                  ". Top: the line from above at one scale, +x right and +z (downhill) down; "
                  "groomed snow white, powder blue, a trunk green, the start a black dot; the "
                  "line blue on the snow, orange in the air, red down. Bottom: speed against "
                  "time, a grid line every 10 km/h.",
              "the line and the speed do what the first sentence says; no red unless the "
              "scenario is a wipeout",
              {{"scenario", k}, {"skis", "reference"}, {"hz", fixed(hz, 0)}}},
             draw_run(sc, level, r));
  }
  if (ran == 0) {
    lab.fail("no scenario is called '" + want + "'",
             "Run `make lab LAB=ride ARGS=--help` and pick one of plugins/skier/scenarios.cpp's "
             "ids, or `all`.");
  }
  lab.table(std::move(table));
}

const ge::lab::LabSpec kSpec{
    "ride",
    ge::lab::LabKind::scenario,
    "plugin:skier",
    "plugins/skier (step_skier, snow, limits)",
    "skier",
    "Staged in each exact situation, does the skier ski as the model says: the terminal speed, "
    "the carve held, the stop, the flight and its landing, the wipeout?",
    {
        {"scenario", "all", "one scenario's id (plugins/skier/scenarios.cpp), or all"},
        {"hz", "120", "steps a second"},
    },
    "--scenario=all",
    run,
};
GE_LAB(kSpec);

}  // namespace
