// LAB `heli`: the helicopter staged in exact situations (scenarios.hpp: a hand-built map, a
// moment, a scripted hand or the bot's) and measured: the lift-off and the torque's turn, the
// climb at full lever, the hover's drift, the cruise, the landing and how hard, the crash and why
// (a lever dropped, a face too steep, a crown, the rotor's tips), the disc over the top, the
// height kept over the snow, and the fuselage hung toward a rider. One picture a scenario: the
// line from above, and the height over the snow and the speed under it.
//
// ge:summary Lab: the helicopter scenarios run and measured; the line, the height and speed drawn
// ge:keywords lab heli helicopter scenario lift-off torque climb hover cruise land crash loop
// ge:keywords terrain rider trace
// ge:skill helicopter labs
// ge:test tests/helicopter_scenarios_test.cpp
#include <string>

#include "core/math.hpp"
#include "core/num.hpp"
#include "core/num_text.hpp"
#include "helicopter/scenarios.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"

namespace {

namespace hc = ge::helicopter;
namespace num = ge::core::num;
using ge::lab::Picture;
using ge::lab::Rgba;

constexpr Rgba kSnow{236, 240, 244};
constexpr Rgba kAirLine{40, 90, 200};
constexpr Rgba kGroundLine{120, 120, 130};
constexpr Rgba kHeightLine{150, 60, 170};
constexpr Rgba kCrashMark{210, 30, 30};
constexpr Rgba kAimMark{30, 160, 70};
constexpr Rgba kTrunk{30, 120, 50};
constexpr Rgba kInk{20, 20, 30};
constexpr Rgba kGrid{200, 200, 210};
constexpr double kDeg = 180 / ge::core::kPi;

std::string fixed(double v, int d) { return num::to_fixed(v, d); }

// The line from above (top) and the height over the snow and the speed against time (bottom).
Picture draw_run(const hc::HeliScenario& sc, const ge::mountain::mapgen::Level& level,
                 const hc::HeliRun& run) {
  constexpr int kW = 520, kPlanH = 300, kGraphH = 150, kPad = 16;
  Picture p = ge::lab::create_picture(kW, kPlanH + kGraphH, Rgba{250, 250, 252});
  double x0 = run.trace.front().x, x1 = x0, z0 = run.trace.front().z, z1 = z0;
  for (const auto& t : run.trace) {
    x0 = num::min(x0, t.x), x1 = num::max(x1, t.x);
    z0 = num::min(z0, t.z), z1 = num::max(z1, t.z);
  }
  if (!sc.hand) {
    x0 = num::min(x0, sc.aim.x), x1 = num::max(x1, sc.aim.x);
    z0 = num::min(z0, sc.aim.z), z1 = num::max(z1, sc.aim.z);
  }
  const double cx = (x0 + x1) / 2, cz = (z0 + z1) / 2;
  const double span = num::max(40.0, num::max(x1 - x0, z1 - z0) * 1.1);
  const double scale = num::min((kW - 2.0 * kPad) / span, (kPlanH - 2.0 * kPad) / span);
  const auto px = [&](double x) { return kW / 2.0 + (x - cx) * scale; };
  const auto pz = [&](double z) { return kPlanH / 2.0 + (z - cz) * scale; };
  ge::lab::fill_rect(p, kPad, kPad, kW - 2 * kPad, kPlanH - 2 * kPad, kSnow);
  for (const auto& tr : level.trees) {
    ge::lab::fill_disc(p, px(tr.x), pz(tr.z), num::max(2.0, tr.crown * scale), kTrunk);
  }
  for (size_t i = 1; i < run.trace.size(); i++) {
    const auto& a = run.trace[i - 1];
    const auto& b = run.trace[i];
    ge::lab::draw_line(p, px(a.x), pz(a.z), px(b.x), pz(b.z), kAirLine, 2);
  }
  ge::lab::fill_disc(p, px(run.trace.front().x), pz(run.trace.front().z), 4, kInk);
  if (!sc.hand) ge::lab::draw_ring(p, px(sc.aim.x), pz(sc.aim.z), 6, kAimMark);
  if (run.crash != hc::HeliCrash::none) {
    ge::lab::draw_ring(p, px(run.end.x), pz(run.end.z), 8, kCrashMark);
  }
  ge::lab::draw_text(p, kPad, 4, std::string(sc.id) + "  " + fixed(span, 0) + " M ACROSS", kInk);
  const int g0 = kPlanH + 8, gh = kGraphH - 24;
  double top_h = 10;
  for (const auto& t : run.trace) top_h = num::max(top_h, t.y - t.ground);
  const double top_kmh = num::max(10.0, run.top_speed * 3.6);
  const double tmax = num::max(1.0, run.trace.back().t);
  for (int k = 0; k <= 4; k++) {
    const double y = g0 + gh - k / 4.0 * gh;
    ge::lab::draw_line(p, kPad, y, kW - kPad, y, kGrid);
  }
  const auto gx = [&](double t) { return kPad + t / tmax * (kW - 2.0 * kPad); };
  const auto gh_of = [&](double over) { return g0 + gh - num::max(0.0, over) / top_h * gh; };
  const auto gv = [&](double v) { return g0 + gh - v * 3.6 / top_kmh * gh; };
  for (size_t i = 1; i < run.trace.size(); i++) {
    const auto& a = run.trace[i - 1];
    const auto& b = run.trace[i];
    ge::lab::draw_line(p, gx(a.t), gh_of(a.y - a.ground), gx(b.t), gh_of(b.y - b.ground),
                       kHeightLine, 2);
    ge::lab::draw_line(p, gx(a.t), gv(a.speed), gx(b.t), gv(b.speed), kAirLine, 1);
  }
  ge::lab::draw_line(p, kPad, g0 + gh, kW - kPad, g0 + gh, kGroundLine);
  ge::lab::draw_text(p, kPad, kPlanH + kGraphH - 12,
                     "HEIGHT 0-" + fixed(top_h, 0) + " M, SPEED 0-" + fixed(top_kmh, 0) +
                         " KM/H, OVER " + fixed(tmax, 1) + " S",
                     kInk);
  return p;
}

void run(ge::lab::Lab& lab) {
  const std::string want = lab.arg("scenario");
  const double hz = lab.arg_num("hz");
  ge::lab::LabTable table{"scenarios",
                          {"scenario", "top km/h", "metres", "climbed m", "climb m/s", "lift-off s",
                           "turned deg", "roll deg", "disc deg", "landings", "landing m/s", "crash",
                           "crash s", "fall m/s", "closest m", "lowest m", "end agl m"},
                          {}};
  int ran = 0;
  for (const hc::HeliScenario& sc : hc::heli_scenarios()) {
    if (want != "all" && want != sc.id) continue;
    ran++;
    const auto level = hc::heli_scenario_level(sc);
    const hc::HeliRun r = hc::run_heli_scenario(sc, hz);
    const std::string k = sc.id;
    const double end_agl = r.end.y - level.ground_at(r.end.x, r.end.z);
    lab.metric(k + ".top-speed", r.top_speed * 3.6, "km/h");
    lab.metric(k + ".distance", r.distance, "m");
    lab.metric(k + ".climbed", r.climbed, "m");
    lab.metric(k + ".top-climb", r.top_climb, "m/s");
    lab.metric(k + ".turned", r.turned * kDeg, "deg");
    lab.metric(k + ".roll", r.max_roll * kDeg, "deg");
    lab.metric(k + ".disc", r.max_disc * kDeg, "deg");
    lab.metric(k + ".landing", r.hardest_landing, "m/s");
    lab.metric(k + ".end-agl", end_agl, "m");
    if (r.closest >= 0) lab.metric(k + ".closest", r.closest, "m");
    if (r.lowest >= 0) lab.metric(k + ".lowest", r.lowest, "m");
    table.rows.push_back(
        {k, fixed(r.top_speed * 3.6, 1), fixed(r.distance, 1), fixed(r.climbed, 1),
         fixed(r.top_climb, 1), fixed(r.lift_off, 2), fixed(r.turned * kDeg, 1),
         fixed(r.max_roll * kDeg, 1), fixed(r.max_disc * kDeg, 1), std::to_string(r.landings),
         fixed(r.hardest_landing, 2), hc::heli_crash_name(r.crash), fixed(r.crash_time, 2),
         fixed(r.crash_fall, 1), fixed(r.closest, 1), fixed(r.lowest, 1), fixed(end_agl, 1)});
    lab.shot({k,
              "HELI " + k,
              std::string(sc.what) +
                  ". Top: the line from above at one scale, +x right and +z (down the face) "
                  "down; a trunk's crown green, the start a black dot, the bot's aim a green "
                  "ring, a red ring where it crashed. Bottom: the skids' height over the snow "
                  "(purple) and the speed (blue) against time.",
              "the line, the height and the speed do what the first sentence says; a red ring "
              "only where the scenario crashes it",
              {{"scenario", k}, {"hz", fixed(hz, 0)}}},
             draw_run(sc, level, r));
  }
  if (ran == 0) {
    lab.fail("no scenario is called '" + want + "'",
             "Run `make lab LAB=heli ARGS=--help` and pick one of "
             "plugins/helicopter/scenarios.cpp's ids, or `all`.");
  }
  lab.table(std::move(table));
}

const ge::lab::LabSpec kSpec{
    "heli",
    ge::lab::LabKind::scenario,
    "plugin:helicopter",
    "plugins/helicopter (step_heli, the rotor, the pilot)",
    "helicopter",
    "Staged in each exact situation, does the helicopter fly as a light helicopter does: the "
    "lift-off and the torque, the climb, the hover, the cruise, the landing, the crash on a "
    "dropped lever, a steep face, a crown, the disc over the top, the height kept over the snow "
    "and the fuselage hung toward a rider?",
    {
        {"scenario", "all", "one scenario's id (plugins/helicopter/scenarios.cpp), or all"},
        {"hz", "120", "steps a second"},
    },
    "--scenario=all",
    run,
};
GE_LAB(kSpec);

}  // namespace
