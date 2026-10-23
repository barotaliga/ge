// LAB `para`: the paramotor staged in exact situations (scenarios.hpp: a hand-built map, a moment,
// a wind, a scripted hand or the bot's) and measured: the launch off the face, the glide's speed
// and sink, the climb, the banked turn, the stall and the surge, the bot down the fall line and
// onto a spot, ridge lift, the lee's folds, a crown, the drop. One picture a scenario: the line
// from above, and the height over the snow and the speed under it.
//
// ge:summary Lab: the paramotor scenarios run and measured; the line, the height and speed drawn
// ge:keywords lab para paramotor scenario launch glide sink climb turn stall surge land ridge
// ge:keywords lee fold crown drop trace
// ge:skill paramotor labs
// ge:test tests/paramotor_scenarios_test.cpp
#include <string>

#include "core/math.hpp"
#include "core/num.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "paramotor/scenarios.hpp"

namespace {

namespace pm = ge::paramotor;
namespace num = ge::core::num;
using ge::lab::Picture;
using ge::lab::Rgba;

constexpr Rgba kSnow{236, 240, 244};
constexpr Rgba kAirLine{40, 90, 200};
constexpr Rgba kSnowLine{120, 120, 130};
constexpr Rgba kHeightLine{150, 60, 170};
constexpr Rgba kFoldLine{230, 140, 30};
constexpr Rgba kCutMark{210, 30, 30};
constexpr Rgba kAimMark{30, 160, 70};
constexpr Rgba kTrunk{30, 120, 50};
constexpr Rgba kInk{20, 20, 30};
constexpr Rgba kGrid{200, 200, 210};
constexpr double kDeg = 180 / ge::core::kPi;

std::string fixed(double v, int d) { return num::to_fixed(v, d); }

// The line from above (top: blue aloft, grey on the snow) and the height over the snow, the speed
// and the folds against time (bottom).
Picture draw_run(const pm::ParaScenario& sc, const ge::mountain::mapgen::Level& level,
                 const pm::ParaRun& run) {
  constexpr int kW = 520, kPlanH = 300, kGraphH = 150, kPad = 16;
  Picture p = ge::lab::create_picture(kW, kPlanH + kGraphH, Rgba{250, 250, 252});
  double x0 = run.trace.front().x, x1 = x0, z0 = run.trace.front().z, z1 = z0;
  for (const auto& t : run.trace) {
    x0 = num::min(x0, t.x), x1 = num::max(x1, t.x);
    z0 = num::min(z0, t.z), z1 = num::max(z1, t.z);
  }
  if (sc.aim.has_aim) {
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
    ge::lab::draw_line(p, px(a.x), pz(a.z), px(b.x), pz(b.z), b.flying ? kAirLine : kSnowLine, 2);
  }
  ge::lab::fill_disc(p, px(run.trace.front().x), pz(run.trace.front().z), 4, kInk);
  if (sc.aim.has_aim) ge::lab::draw_ring(p, px(sc.aim.x), pz(sc.aim.z), 6, kAimMark);
  if (run.cut != pm::ParaCut::none && run.cut != pm::ParaCut::drop) {
    ge::lab::draw_ring(p, px(run.end.canopy.x), pz(run.end.canopy.z), 8, kCutMark);
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
  const auto gf = [&](double f) { return g0 + gh - f * gh; };
  for (size_t i = 1; i < run.trace.size(); i++) {
    const auto& a = run.trace[i - 1];
    const auto& b = run.trace[i];
    ge::lab::draw_line(p, gx(a.t), gh_of(a.y - a.ground), gx(b.t), gh_of(b.y - b.ground),
                       kHeightLine, 2);
    ge::lab::draw_line(p, gx(a.t), gv(a.speed), gx(b.t), gv(b.speed), kAirLine, 1);
    if (a.fold > 0 || b.fold > 0) {
      ge::lab::draw_line(p, gx(a.t), gf(a.fold), gx(b.t), gf(b.fold), kFoldLine, 1);
    }
  }
  ge::lab::draw_line(p, kPad, g0 + gh, kW - kPad, g0 + gh, kSnowLine);
  ge::lab::draw_text(p, kPad, kPlanH + kGraphH - 12,
                     "HEIGHT 0-" + fixed(top_h, 0) + " M, SPEED 0-" + fixed(top_kmh, 0) +
                         " KM/H, FOLD 0-1, OVER " + fixed(tmax, 1) + " S",
                     kInk);
  return p;
}

void run(ge::lab::Lab& lab) {
  const std::string want = lab.arg("scenario");
  const double hz = lab.arg_num("hz");
  ge::lab::LabTable table{
      "scenarios",
      {"scenario", "launch s",  "take-off s", "top km/h",   "metres",  "climbed m", "climb m/s",
       "air km/h", "sink m/s",  "bank deg",   "turned deg", "stalled", "folds",     "fold",
       "touches",  "landing g", "wiped",      "cut",        "cut s",   "closest m"},
      {}};
  int ran = 0;
  for (const pm::ParaScenario& sc : pm::para_scenarios()) {
    if (want != "all" && want != sc.id) continue;
    ran++;
    const auto level = pm::para_scenario_level(sc);
    const pm::ParaRun r = pm::run_para_scenario(sc, hz);
    const std::string k = sc.id;
    lab.metric(k + ".top-speed", r.top_speed * 3.6, "km/h");
    lab.metric(k + ".distance", r.distance, "m");
    lab.metric(k + ".climbed", r.climbed, "m");
    lab.metric(k + ".top-climb", r.top_climb, "m/s");
    lab.metric(k + ".airspeed", r.airspeed * 3.6, "km/h");
    lab.metric(k + ".sink", r.sink, "m/s");
    lab.metric(k + ".bank", r.max_bank * kDeg, "deg");
    lab.metric(k + ".turned", r.turned * kDeg, "deg");
    lab.metric(k + ".folds", r.folds, "count");
    lab.metric(k + ".landing-g", r.landing_g, "g");
    if (r.closest >= 0) lab.metric(k + ".closest", r.closest, "m");
    table.rows.push_back({k,
                          fixed(r.launch, 2),
                          fixed(r.take_off, 2),
                          fixed(r.top_speed * 3.6, 1),
                          fixed(r.distance, 1),
                          fixed(r.climbed, 1),
                          fixed(r.top_climb, 2),
                          fixed(r.airspeed * 3.6, 1),
                          fixed(r.sink, 2),
                          fixed(r.max_bank * kDeg, 1),
                          fixed(r.turned * kDeg, 1),
                          r.stalled ? "yes" : "no",
                          std::to_string(r.folds),
                          fixed(r.deepest_fold, 2),
                          std::to_string(r.touches),
                          fixed(r.landing_g, 2),
                          r.wiped_out ? "yes" : "no",
                          pm::para_cut_name(r.cut),
                          fixed(r.cut_time, 2),
                          fixed(r.closest, 1)});
    lab.shot({k,
              "PARA " + k,
              std::string(sc.what) +
                  ". Top: the pilot's line from above at one scale, +x right and +z (down the "
                  "face) down, blue in the air and grey on the snow; a trunk's crown green, the "
                  "start a black dot, the bot's aim a green ring, a red ring where the wing came "
                  "down. Bottom: the skis' height over the snow (purple), the speed (blue) and a "
                  "fold's depth (orange) against time.",
              "the line, the height and the speed do what the first sentence says; a red ring "
              "only where the scenario brings the wing down",
              {{"scenario", k}, {"hz", fixed(hz, 0)}}},
             draw_run(sc, level, r));
  }
  if (ran == 0) {
    lab.fail("no scenario is called '" + want + "'",
             "Run `make lab LAB=para ARGS=--help` and pick one of "
             "plugins/paramotor/scenarios.cpp's ids, or `all`.");
  }
  lab.table(std::move(table));
}

const ge::lab::LabSpec kSpec{
    "para",
    ge::lab::LabKind::scenario,
    "plugin:paramotor",
    "plugins/paramotor (step_para, the wing, the air, the pilot)",
    "paramotor",
    "Staged in each exact situation, does the paramotor fly as a motorised speed wing does: the "
    "launch off a face, the glide's speed and sink, the climb, the banked turn, the stall and "
    "the surge, the bot down the fall line and onto a spot, ridge lift, the lee's folds, a crown "
    "and the drop?",
    {
        {"scenario", "all", "one scenario's id (plugins/paramotor/scenarios.cpp), or all"},
        {"hz", "120", "steps a second"},
    },
    "--scenario=all",
    run,
};
GE_LAB(kSpec);

}  // namespace
