// LAB `injuries`: what the skier's body takes. Every ride scenario's body (the injuries by part
// and name, the severity score, when he was mortally hurt and when he died), and the body's
// curves swept over many runs' draws: a trunk skied into head-on (the body thrown on into it) and
// met beside him by closing speed, and
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
#include "skier/synthetic.hpp"
#include "skier/trees.hpp"
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

// ONE MEETING, its physics run once: every step's skier and events, on its own map. The body
// draws nothing the skier's step reads, so every seed's body is judged off the same steps.
struct Tape {
  ge::mountain::mapgen::Level level;
  sk::TreeIndex trees;
  std::vector<sk::SkierState> states;
  std::vector<sk::SkierEvents> events;
};

// Tallied over `kTrials` seeds, the body judged over every step of `t`.
Sweep sweep(const Tape& t) {
  Sweep out;
  std::vector<int32_t> taken(sk::injuries_catalog().size(), 0);
  const sk::World world{&t.level, &t.trees};
  constexpr double dt = 1.0 / 120;
  for (int32_t k = 0; k < kTrials; k++) {
    sk::BodyState b;
    sk::BodyEvents be;
    for (size_t i = 0; i < t.states.size(); i++) {
      sk::SkierState s = t.states[i];
      sk::step_body(b, s, t.events[i], world, sk::kReferenceSkis,
                    sk::BodyClock{static_cast<double>(i + 1) * dt, dt, static_cast<int64_t>(i),
                                  static_cast<uint32_t>(k)},
                    be);
    }
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

// A packed flat with one trunk at its middle.
Tape flat_with_a_trunk() {
  Tape t;
  t.level = sk::flat_strip(0, 1);
  const double m = t.level.size / 2;
  t.level.trees.push_back(ge::mountain::mapgen::TreeDef{
      m, m, t.level.ground_at(m, m), 18, 0.2, 60, 2.5, ge::mountain::mapgen::TreeKind::spruce, -1});
  t.trees = sk::build_tree_index(t.level);
  return t;
}

// A TRUNK MET HEAD-ON at `v` m/s: skied straight into it from 2.5 m short, `across` m to its
// right of the trunk's centre (0 dead on), the skis stopped on it or turned aside by it and the
// body thrown on into it or past it, three seconds of it.
Tape into_a_trunk(double v, double across = 0) {
  Tape t = flat_with_a_trunk();
  const sk::World world{&t.level, &t.trees};
  const double m = t.level.size / 2;
  sk::SkierState s = sk::stand_skier(world, m + across, m - 2.5, 0, v);
  sk::SkierEvents ev;
  for (int i = 0; i < 3 * 120; i++) {
    sk::step_skier(s, sk::SkierInput{}, world, sk::kReferenceSkis, 1.0 / 120, ev);
    t.states.push_back(s);
    t.events.push_back(ev);
  }
  return t;
}

// A TRUNK IN HIS LINE: the head-on run swept across the trunk, his line from `kAcrossMost` m to
// its left to as far to its right, every line's draws tallied together (the injury most taken is
// the dead-on line's).
constexpr double kAcrossMost = 0.6;
constexpr int32_t kLines = 13;
Sweep in_his_line(double v) {
  Sweep out;
  for (int32_t j = 0; j < kLines; j++) {
    const double across = -kAcrossMost + 2 * kAcrossMost * j / (kLines - 1);
    const Sweep a = sweep(into_a_trunk(v, across));
    out.hurt += a.hurt / kLines;
    out.iss += a.iss / kLines;
    out.mortal += a.mortal / kLines;
    if (j == kLines / 2) out.most = a.most;
  }
  return out;
}

// ONE STEP'S EVENTS with nothing thrown after them: the trunk beside him, or a landing (one step
// more, for what a folded landing owes the trunk).
Tape events_alone(const sk::SkierEvents& ev, double tree_x, double tree_z) {
  Tape t;
  t.level.trees.push_back(ge::mountain::mapgen::TreeDef{
      tree_x, tree_z, 0, 18, 0.2, 60, 2.5, ge::mountain::mapgen::TreeKind::spruce, -1});
  t.states.push_back(sk::SkierState{});  // at the origin, facing +z
  t.events.push_back(ev);
  t.states.push_back(sk::SkierState{});
  t.events.push_back(sk::SkierEvents{});
  return t;
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
  gl::LabTable trunk{"trunk",
                     {"km/h", "head-on hurt", "head-on ISS", "head-on mortal", "most", "line ISS",
                      "line mortal", "side hurt", "side ISS", "side mortal", "most"},
                     {}};
  gl::LineChart lc;
  lc.frame = {"MEAN SEVERITY BY THE TRUNK'S CLOSING SPEED AND BY A LANDING'S LOAD",
              std::to_string(kTrials) + " draws a point, the reference skier",
              "closing speed (m/s) / landing load (g)", "mean ISS"};
  gl::Trace head_on{"trunk head-on", {}, {}}, line{"trunk in his line", {}, {}};
  gl::Trace beside{"trunk beside him", {}, {}};
  gl::Trace landing_trace{"landing", {}, {}};
  for (int v = 2; v <= 24; v += 2) {
    sk::SkierEvents ev;
    ev.tree = 0;
    ev.closing = v;
    ev.wiped_out = v > sk::tuning::wipeout::kTreeSpeed;
    ev.cause = ev.wiped_out ? sk::Wipeout::tree : sk::Wipeout::none;
    const Sweep a = sweep(into_a_trunk(v));
    const Sweep l = in_his_line(v);
    const Sweep b = sweep(events_alone(ev, -0.55, 0));  // beside him, on his left
    const std::string kmh = fixed(v * 3.6, 0);
    lab.metric("trunk.head-on." + kmh + ".iss", a.iss, "score");
    lab.metric("trunk.line." + kmh + ".iss", l.iss, "score");
    lab.metric("trunk.beside." + kmh + ".iss", b.iss, "score");
    trunk.rows.push_back({kmh, fixed(a.hurt, 2), fixed(a.iss, 1), fixed(a.mortal, 2),
                          a.most.empty() ? "-" : a.most, fixed(l.iss, 1), fixed(l.mortal, 2),
                          fixed(b.hurt, 2), fixed(b.iss, 1), fixed(b.mortal, 2),
                          b.most.empty() ? "-" : b.most});
    head_on.x.push_back(v), head_on.y.push_back(a.iss);
    line.x.push_back(v), line.y.push_back(l.iss);
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
    const Sweep a = sweep(events_alone(ev, 0, 100));
    lab.metric("landing." + std::to_string(g) + "g.iss", a.iss, "score");
    land.rows.push_back({std::to_string(g), fixed(a.hurt, 2), fixed(a.iss, 1), fixed(a.mortal, 2),
                         a.most.empty() ? "-" : a.most});
    landing_trace.x.push_back(g), landing_trace.y.push_back(a.iss);
  }
  lab.table(std::move(land));
  lc.traces = {head_on, line, beside, landing_trace};
  lc.rules = {16};  // major trauma
  lc.spikes = false;
  const gl::Chart ch = gl::line_chart(lc);
  lab.shot({"curves",
            "MEAN SEVERITY BY SPEED AND BY LOAD",
            "The mean injury severity score over " + std::to_string(kTrials) +
                " draws: a trunk skied into square ahead of the tips (the body thrown on into it) "
                "and met "
                "beside him on his left, by "
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
