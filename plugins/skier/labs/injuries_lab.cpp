// LAB `injuries`: what the skier's body takes. Every ride scenario's body (the injuries by part
// and name, the severity score, when he was mortally hurt and when he died), and the body's
// curves swept over many runs' draws: a trunk met head-on and beside him by closing speed, and
// a landing by its load, each as the share hurt, the mean severity and the share killed. Two
// pictures: every scenario's parts shaded by their worst injury, and the sweeps' mean severity.
//
// ge:summary Lab: the body's injuries in every scenario, and its curves by speed and by load
// ge:keywords lab injuries body iss severity ais trunk landing sweep mortal death scenario
// ge:skill skier labs
// ge:test tests/skier_body_test.cpp
#include <string>
#include <vector>

#include "core/num.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/mapgen/types.hpp"
#include "skier/anatomy.hpp"
#include "skier/body.hpp"
#include "skier/scenarios.hpp"
#include "skier/skis.hpp"
#include "skier/tuning.hpp"

namespace {

namespace sk = ge::skier;
namespace gl = ge::lab;
using gl::Picture;
using gl::Rgba;

constexpr int32_t kTrials = 400;
constexpr Rgba kInk{20, 20, 30};
constexpr Rgba kAisColour[6] = {{240, 242, 246}, {250, 226, 120}, {245, 170, 60},
                                {230, 100, 40},  {190, 30, 30},   {90, 0, 20}};

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

// What `kTrials` runs' draws of one dose do: the share hurt, the mean score, the share killed,
// and the injury taken most.
struct Sweep {
  double hurt = 0;
  double iss = 0;
  double mortal = 0;
  std::string most;
};

// The step's events and the trunk where they stand, judged over `kTrials` seeds.
Sweep sweep(const sk::SkierEvents& ev, const sk::World& world) {
  Sweep out;
  std::vector<int32_t> taken(sk::injuries_catalog().size(), 0);
  for (int32_t k = 0; k < kTrials; k++) {
    sk::BodyState b;
    sk::SkierState s;  // at the origin, facing +z
    sk::BodyEvents be;
    sk::step_body(b, s, ev, world, sk::kReferenceSkis,
                  sk::BodyClock{0, 1.0 / 120, 0, static_cast<uint32_t>(k)}, be);
    out.hurt += b.count > 0 ? 1 : 0;
    out.iss += sk::severity_of(b);
    out.mortal += b.mortal >= 0 ? 1 : 0;
    for (int32_t j = 0; j < b.count && j < sk::kMostInjuries; j++) {
      taken[b.injuries[static_cast<size_t>(j)].kind]++;
    }
  }
  out.hurt /= kTrials;
  out.iss /= kTrials;
  out.mortal /= kTrials;
  int32_t best = 0;
  for (size_t j = 0; j < taken.size(); j++) {
    if (taken[j] > best) {
      best = taken[j];
      out.most = sk::injuries_catalog()[j].name;
    }
  }
  return out;
}

// Every scenario a row, every part a column, each cell its worst injury's AIS.
Picture draw_body(const std::vector<std::string>& ids, const std::vector<sk::BodyState>& bodies) {
  constexpr int kLeft = 110, kCell = 22, kTop = 70, kPad = 10;
  const int rows = static_cast<int>(ids.size());
  Picture p = gl::create_picture(kLeft + sk::kParts * kCell + kPad, kTop + rows * kCell + 40,
                                 Rgba{250, 250, 252});
  gl::draw_text(p, kPad, 6, "WORST INJURY BY PART, EVERY SCENARIO", kInk);
  for (int c = 0; c < sk::kParts; c++) {
    const std::string name = sk::part_name(static_cast<sk::Part>(c));
    // The column's name, written down it a letter at a time from its first five.
    for (size_t i = 0; i < name.size() && i < 5; i++) {
      gl::draw_text(p, kLeft + c * kCell + 7, 24 + static_cast<double>(i) * 9,
                    std::string(1, static_cast<char>(name[i] - 32 * (name[i] >= 'a'))), kInk);
    }
  }
  for (int r = 0; r < rows; r++) {
    gl::draw_text(p, kPad, kTop + r * kCell + 7, ids[static_cast<size_t>(r)], kInk);
    for (int c = 0; c < sk::kParts; c++) {
      const int ais = bodies[static_cast<size_t>(r)].worst[static_cast<size_t>(c)];
      gl::fill_rect(p, kLeft + c * kCell + 1, kTop + r * kCell + 1, kCell - 2, kCell - 2,
                    kAisColour[ais]);
    }
  }
  const int ky = kTop + rows * kCell + 12;
  for (int a = 0; a <= 5; a++) {
    gl::fill_rect(p, kLeft + a * 60, ky, 14, 14, kAisColour[a]);
    gl::draw_text(p, kLeft + a * 60 + 18, ky + 3, "AIS " + std::to_string(a), kInk);
  }
  return p;
}

void run(gl::Lab& lab) {
  // EVERY SCENARIO'S BODY.
  gl::LabTable scen{"scenarios",
                    {"scenario", "injuries", "worst AIS", "ISS", "peak g", "mortal s", "dead s",
                     "injuries by part"},
                    {}};
  std::vector<std::string> ids;
  std::vector<sk::BodyState> bodies;
  for (const sk::Scenario& sc : sk::scenarios()) {
    const sk::ScenarioRun r = sk::run_scenario(sc, sk::kReferenceSkis);
    const std::string k = sc.id;
    int worst = 0;
    for (uint8_t w : r.body.worst) worst = w > worst ? w : worst;
    std::string list;
    for (int32_t j = 0; j < r.body.count && j < sk::kMostInjuries; j++) {
      const sk::Injury& h = r.body.injuries[static_cast<size_t>(j)];
      list += (list.empty() ? "" : " ") + std::string(sk::part_name(h.part)) + ":" +
              sk::injury_def(h.kind).name;
    }
    const int32_t iss = sk::severity_of(r.body);
    lab.metric(k + ".injuries", r.body.count, "count");
    lab.metric(k + ".iss", iss, "score");
    lab.metric(k + ".peak-g", r.body.peak, "g");
    lab.metric(k + ".dead", r.death_time, "s");
    scen.rows.push_back({k, std::to_string(r.body.count), std::to_string(worst),
                         std::to_string(iss), fixed(r.body.peak, 1), fixed(r.mortal_time, 2),
                         fixed(r.death_time, 2), list.empty() ? "-" : list});
    ids.push_back(k);
    bodies.push_back(r.body);
  }
  lab.table(std::move(scen));
  lab.shot({"body",
            "INJURIES BY PART",
            "Every ride scenario a row and every part of the body a column (head, neck, chest, "
            "back, abdomen, pelvis, then the paired parts left and right), each cell shaded by "
            "the worst injury that part took: white none, yellow AIS 1 to dark red AIS 5.",
            "the clean rides are white across; the trunks and the high drop are dark where "
            "they struck (the front of him on a trunk, the legs and the back on a drop)",
            {{"skis", "reference"}, {"hz", "120"}}},
           draw_body(ids, bodies));

  // THE CURVES, over many runs' draws: a trunk by closing speed and a landing by its load.
  ge::mountain::mapgen::Level level;
  level.trees.push_back(ge::mountain::mapgen::TreeDef{0, 0, 0, 18, 0.2, 60, 2.5,
                                                      ge::mountain::mapgen::TreeKind::spruce, -1});
  const sk::World world{&level, nullptr};
  gl::LabTable trunk{"trunk",
                     {"km/h", "head-on hurt", "head-on ISS", "head-on mortal", "most", "side hurt",
                      "side ISS", "side mortal", "most"},
                     {}};
  gl::LineChart lc;
  lc.frame = {"MEAN SEVERITY BY THE TRUNK'S CLOSING SPEED AND BY A LANDING'S LOAD",
              std::to_string(kTrials) + " draws a point, the reference skier",
              "closing speed (m/s) / landing load (g)", "mean ISS"};
  gl::Trace head_on{"trunk head-on", {}, {}}, beside{"trunk beside him", {}, {}};
  gl::Trace landing_trace{"landing", {}, {}};
  for (int v = 2; v <= 24; v += 2) {
    sk::SkierEvents ev;
    ev.tree = 0;
    ev.closing = v;
    ev.wiped_out = v > sk::tuning::wipeout::kTreeSpeed;
    ev.cause = ev.wiped_out ? sk::Wipeout::tree : sk::Wipeout::none;
    level.trees[0].x = 0;  // square ahead of his tips
    level.trees[0].z = 0.55;
    const Sweep a = sweep(ev, world);
    level.trees[0].x = -0.55;  // beside him, on his left
    level.trees[0].z = 0;
    const Sweep b = sweep(ev, world);
    const std::string kmh = fixed(v * 3.6, 0);
    lab.metric("trunk.head-on." + kmh + ".iss", a.iss, "score");
    lab.metric("trunk.beside." + kmh + ".iss", b.iss, "score");
    trunk.rows.push_back({kmh, fixed(a.hurt, 2), fixed(a.iss, 1), fixed(a.mortal, 2),
                          a.most.empty() ? "-" : a.most, fixed(b.hurt, 2), fixed(b.iss, 1),
                          fixed(b.mortal, 2), b.most.empty() ? "-" : b.most});
    head_on.x.push_back(v), head_on.y.push_back(a.iss);
    beside.x.push_back(v), beside.y.push_back(b.iss);
  }
  lab.table(std::move(trunk));
  gl::LabTable land{"landing", {"load g", "hurt", "ISS", "mortal", "most"}, {}};
  for (int g = 4; g <= 30; g += 2) {
    sk::SkierEvents ev;
    ev.landed = true;
    ev.landing_g = g;
    ev.flight_s = 1;
    ev.wiped_out = g > sk::tuning::air::kHarshG;
    ev.cause = ev.wiped_out ? sk::Wipeout::landing : sk::Wipeout::none;
    const Sweep a = sweep(ev, world);
    lab.metric("landing." + std::to_string(g) + "g.iss", a.iss, "score");
    land.rows.push_back({std::to_string(g), fixed(a.hurt, 2), fixed(a.iss, 1), fixed(a.mortal, 2),
                         a.most.empty() ? "-" : a.most});
    landing_trace.x.push_back(g), landing_trace.y.push_back(a.iss);
  }
  lab.table(std::move(land));
  lc.traces = {head_on, beside, landing_trace};
  lc.rules = {16};  // major trauma
  lc.spikes = false;
  const gl::Chart ch = gl::line_chart(lc);
  lab.shot({"curves",
            "MEAN SEVERITY BY SPEED AND BY LOAD",
            "The mean injury severity score over " + std::to_string(kTrials) +
                " draws: a trunk met square ahead of the tips and beside him on his left, by "
                "closing speed in m/s, and a landing by its load in g, on one x axis. The dashed "
                "line is 16, major trauma.",
            "every curve rises with its dose and none falls back; a trunk head-on is worse than "
            "beside him at the same speed; a landing under 6 g scores 0",
            {{"draws", std::to_string(kTrials)}, {"skis", "reference"}}},
           gl::to_picture(ch.plot));
}

const gl::LabSpec kSpec{
    "injuries",
    gl::LabKind::scenario,
    "plugin:skier",
    "plugins/skier (body, anatomy)",
    "skier",
    "What does the skier's body take in each staged crash, and how do a trunk's speed and a "
    "landing's load turn into injuries over many draws?",
    {},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
