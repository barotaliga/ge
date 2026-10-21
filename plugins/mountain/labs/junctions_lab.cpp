// LAB `junctions`: where a ski area's groomed snow BREAKS: a lip, a step or a wall a skier meets
// at speed where nothing was built to be jumped. Every run is read down its own line at its centre
// and a few metres in from either edge, a metre at a time: the pitch over the next 3 m against
// the pitch over the 3 m before, and a sudden steepening past `--threshold` (a quarter: from 10 %
// to 35 % in a stride) is a STEP. The kickers and drops laid on purpose (R9, R24) and the station
// pads (R26) are left out. What is left is gathered into one step a stretch and named by where it
// stands: a run MERGING into another (its last `course.merge` m, or the other's snow round the
// junction), a LANE, where a lane BRANCHES off a piste, or on a PISTE anywhere else.
//
// It exists because runs meet badly where nobody looks: a run graded under the surface of the
// one it leaves, or a wide run levelled to its last station above a narrower one, stands a lip
// or a wall exactly where two lines of skiers cross. The table lists every step (the run, the
// kind, the arc, the pitch before and after); the sheet draws the worst as relief, the runs over
// it and the step ringed.
//
// ge:summary Lab: where a ski area's groomed snow steps where runs meet: steps found, named, drawn
// ge:keywords lab junctions junction merge branch lane step lip wall pitch ski-area relief
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "core/heightfield.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "lab/lab.hpp"
#include "mountain/labs/level_map.hpp"
#include "mountain/labs/resort_map.hpp"
#include "mountain/mapgen/query.hpp"
#include "mountain/mapgen/resort_rules.hpp"

namespace {

namespace mg = ge::mountain::mapgen;
using ge::core::hypot;
using ge::core::sample_field;

// The stride the pitch is read over either side of a point, m.
constexpr double kStride = 3;
// How far in from a run's edge its edge lines are read, m.
constexpr double kEdgeIn = 3;
// Steps closer than this down the same run are one step, m.
constexpr double kGather = 10;
// How far past a kicker or a drop (and round a pad) the snow is the feature's, m.
constexpr double kFeatureMargin = 5;
// The packed share over which snow is groomed (R10).
constexpr double kGroomed = 0.9;
// How far down a piste from where a lane leaves it the snow is the branch's, m.
constexpr double kBranchReach = 60;

struct Step {
  std::string run;
  const char* kind;
  double s;
  double before;
  double after;
  double x;
  double z;
};

std::string pct(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.0f", 100 * v);
  return b;
}

std::string f1(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.1f", v);
  return b;
}

// Every step down every run of the area.
std::vector<Step> find_steps(const mg::ResortAttempt& a, double threshold) {
  const double merge = mg::resort_rules::course::merge;
  // Where lanes leave, and where runs join, by the run they touch.
  std::vector<std::vector<double>> joined(a.built.size());
  std::vector<mg::PlanPoint> lane_tops;
  for (const mg::BuiltRun& b : a.built) {
    if (b.walked.into) joined[size_t(b.walked.into->run)].push_back(b.walked.into->s);
    if (b.walked.spec.kind == mg::RunKind::road) {
      const mg::TrackPoint& p = b.walked.track.points.front();
      lane_tops.push_back({p.x, p.z});
    }
  }
  std::vector<Step> steps;
  for (size_t r = 0; r < a.built.size(); r++) {
    const mg::BuiltRun& b = a.built[r];
    const mg::WalkedRun& w = b.walked;
    const mg::Track& track = w.track;
    const bool lane = w.spec.kind == mg::RunKind::road;
    // The stretches the features laid on purpose own.
    std::vector<std::pair<double, double>> owned;
    for (const mg::Kicker& k : b.kickers) {
      const double s = k.s.value_or(0);
      owned.emplace_back(s - k.ramp - kFeatureMargin, s + k.landing + kFeatureMargin);
    }
    for (const mg::Cliff& d : b.drops) {
      const double s = d.s.value_or(0);
      owned.emplace_back(s - d.shelf - kFeatureMargin, s + d.face + d.landing + kFeatureMargin);
    }
    // The stretches down this piste where a lane leaves it.
    std::vector<double> branches;
    if (!lane) {
      for (const mg::PlanPoint& t : lane_tops) {
        const mg::TrackHit hit = mg::nearest_track_point(track, t.x, t.z);
        if (hit.distance < track.points[size_t(hit.index)].width / 2 + 10) {
          branches.push_back(track.points[size_t(hit.index)].s);
        }
      }
    }
    Step* last = nullptr;
    for (int at_m = int(kStride); at_m <= int(track.length - kStride); at_m++) {
      const double s = at_m;
      bool skip = false;
      for (const auto& [from, to] : owned) skip = skip || (s > from && s < to);
      if (skip) continue;
      const mg::TrackPoint here = mg::track_point_at(track, s);
      const double ax = ge::core::libm::cos(here.heading);  // the way across, to the right
      const double az = -ge::core::libm::sin(here.heading);
      const double edge = std::max(0.0, here.width / 2 - kEdgeIn);
      for (const double off : {-edge, 0.0, edge}) {
        // The ground `off` m across the line at arc `u`, and whether it is groomed snow there:
        // an edge line cut across a tight turn reads the woods, which are no step a skier meets.
        bool groomed = true;
        const auto at = [&](double u) {
          const mg::TrackPoint p = mg::track_point_at(track, u);
          const double px = p.x + ge::core::libm::cos(p.heading) * off;
          const double pz = p.z - ge::core::libm::sin(p.heading) * off;
          groomed = groomed && sample_field(a.packed, px, pz) > kGroomed;
          return sample_field(a.ground, px, pz);
        };
        const double x = here.x + ax * off;
        const double z = here.z + az * off;
        bool on_pad = false;
        for (const mg::StationPad& p : a.pads) {
          on_pad = on_pad || hypot(x - p.x, z - p.z) < p.r + kFeatureMargin;
        }
        if (on_pad) continue;
        const double y = at(s);
        const double before = (at(s - kStride) - y) / kStride;
        const double after = (y - at(s + kStride)) / kStride;
        if (after - before < threshold || !groomed) continue;
        if (last && last->run == w.spec.id && s - last->s < kGather) {
          if (after - before > last->after - last->before) {
            *last = {last->run, last->kind, s, before, after, x, z};
          }
          continue;
        }
        const char* kind = "piste";
        if (lane) {
          kind = "lane";
        } else if ((w.into && s > track.length - merge) ||
                   std::any_of(joined[r].begin(), joined[r].end(),
                               [&](double j) { return std::abs(s - j) < merge; })) {
          kind = "merge";
        } else if (std::any_of(branches.begin(), branches.end(),
                               [&](double t) { return s >= t - 10 && s < t + kBranchReach; })) {
          kind = "branch";
        }
        steps.push_back({w.spec.id, kind, s, before, after, x, z});
        last = &steps.back();
      }
    }
  }
  return steps;
}

// One step's patch of ground as relief, `side` px for `span` m round it, the runs over it and the
// step ringed.
ge::lab::Picture draw_patch(const mg::ResortAttempt& a, const Step& st, int side, double span) {
  auto pic = ge::lab::create_picture(side, side, {0, 0, 0});
  const double k = side / span;
  const double x0 = st.x - span / 2;
  const double z0 = st.z - span / 2;
  // The patch's mean slope toward the light, off a coarse grid over it.
  double mean = 0;
  for (int j = 0; j < 9; j++) {
    for (int i = 0; i < 9; i++) {
      const auto g =
          ge::core::field_gradient(a.ground, x0 + span * (i + 0.5) / 9, z0 + span * (j + 0.5) / 9);
      mean += (g.gx + g.gz) / 81;
    }
  }
  for (int py = 0; py < side; py++) {
    for (int px = 0; px < side; px++) {
      const double x = x0 + (px + 0.5) / k;
      const double z = z0 + (py + 0.5) / k;
      const auto g = ge::core::field_gradient(a.ground, x, z);
      // Lit from the upper left against the patch's own mean slope and exaggerated, so a change
      // of pitch a skier feels shows as a band of light or dark.
      const double lit = std::clamp(0.5 + 1.2 * (g.gx + g.gz - mean), 0.0, 1.0);
      const double packed = std::clamp(sample_field(a.packed, x, z), 0.0, 1.0);
      const auto v = static_cast<uint8_t>(40 + 200 * lit);
      ge::lab::set_pixel(pic, px, py,
                         {static_cast<uint8_t>(v * (1 - 0.15 * packed)),
                          static_cast<uint8_t>(v * (1 - 0.05 * packed)), v});
    }
  }
  for (const mg::BuiltRun& b : a.built) {
    const auto& pts = b.walked.track.points;
    const ge::lab::Rgba c = b.walked.spec.kind == mg::RunKind::road
                                ? ge::lab::Rgba{110, 110, 110}
                                : ge::mountain::labs::grade_colour(b.grade);
    for (size_t i = 1; i < pts.size(); i++) {
      ge::lab::draw_line(pic, (pts[i - 1].x - x0) * k, (pts[i - 1].z - z0) * k, (pts[i].x - x0) * k,
                         (pts[i].z - z0) * k, c, 1);
    }
  }
  ge::lab::draw_ring(pic, side / 2.0, side / 2.0, 8, {250, 120, 0});
  const std::string label = st.run + " " + st.kind + " " + pct(st.before) + "-" + pct(st.after);
  ge::lab::fill_rect(pic, 0, 0, ge::lab::text_width(label) + 6, ge::lab::kGlyphH + 4,
                     {0, 0, 0, 180});
  ge::lab::draw_text(pic, 3, 2, label, {255, 255, 255});
  return pic;
}

void run(ge::lab::Lab& lab) {
  const double threshold = lab.arg_num("threshold");
  const int tile = static_cast<int>(lab.arg_num("tile"));
  const auto worst_n = static_cast<size_t>(lab.arg_num("worst"));
  for (const double seed : lab.arg_nums("seeds")) {
    const std::string id = "seed-" + std::to_string(static_cast<long long>(seed));
    mg::ResortAttempt a;
    const auto tried = ge::mountain::labs::build_area(lab, seed, a);
    if (tried.empty() || !tried.back().why.empty()) continue;
    std::vector<Step> steps = find_steps(a, threshold);
    int merges = 0, lanes = 0, branches = 0, pistes = 0;
    for (const Step& st : steps) {
      const std::string kind = st.kind;
      merges += kind == "merge";
      lanes += kind == "lane";
      branches += kind == "branch";
      pistes += kind == "piste";
    }
    lab.metric(id + ".attempt", a.attempt, "count");
    lab.metric(id + ".steps", static_cast<double>(steps.size()), "count");
    lab.metric(id + ".merge-steps", merges, "count");
    lab.metric(id + ".branch-steps", branches, "count");
    lab.metric(id + ".lane-steps", lanes, "count");
    lab.metric(id + ".piste-steps", pistes, "count");
    std::stable_sort(steps.begin(), steps.end(), [](const Step& x, const Step& y) {
      return x.after - x.before > y.after - y.before;
    });
    lab.metric(id + ".worst", steps.empty() ? 0 : steps.front().after - steps.front().before,
               "m/m");
    ge::lab::LabTable table{id + ": steps, the worst first",
                            {"run", "kind", "s m", "before %", "after %", "x m", "z m"},
                            {}};
    for (const Step& st : steps) {
      table.rows.push_back(
          {st.run, st.kind, f1(st.s), pct(st.before), pct(st.after), f1(st.x), f1(st.z)});
    }
    lab.table(std::move(table));
    if (steps.empty()) {
      lab.note(id + ": no step over " + pct(threshold) + " % on any run");
      continue;
    }
    const size_t n = std::min(worst_n, steps.size());
    const int cols = static_cast<int>(std::min<size_t>(3, n));
    const int rows = static_cast<int>((n + 2) / 3);
    auto sheet = ge::lab::create_picture(cols * tile, rows * tile, {0, 0, 0});
    for (size_t i = 0; i < n; i++) {
      ge::lab::blit(sheet, draw_patch(a, steps[i], tile - 4, 120), int(i % 3) * tile + 2,
                    int(i / 3) * tile + 2);
    }
    const std::string seed_word = std::to_string(static_cast<long long>(seed));
    lab.shot({id + "-steps", "SEED " + seed_word + " WORST STEPS IN THE SNOW",
              "The " + std::to_string(n) + " worst of " + std::to_string(steps.size()) +
                  " steps found down the ski area's runs (the pitch over the next " + f1(kStride) +
                  " m steeper than over the " + f1(kStride) + " m before by " + pct(threshold) +
                  " % or more), each 120 m of ground as relief lit low from the "
                  "upper left, the runs over it in their colours, the lanes grey, the step ringed "
                  "and labelled with its run, where it stands and its pitch before and after, %.",
              "a ringed band of light and dark across a run where two runs meet is a lip a skier "
              "meets; none should stand in the snow two runs share",
              ge::mountain::labs::area_inputs(lab, a)},
             sheet);
  }
}

const ge::lab::LabSpec kSpec{
    "junctions",
    ge::lab::LabKind::picture,
    "plugin:mountain",
    "plugins/mountain/mapgen/network_stamp.cpp",
    "mountain",
    "Where does a ski area's groomed snow step where nothing was built to be jumped?",
    {
        {"seeds", "38", "seeds, comma-separated"},
        {"threshold", "0.25", "the steepening that counts as a step, m/m over a stride"},
        {"worst", "6", "how many of the worst steps the sheet draws"},
        {"tile", "320", "each patch's side on the sheet, px"},
        ge::mountain::labs::kRegionFlag,
        ge::mountain::labs::kAttemptsFlag,
    },
    "--seeds=38 --attempts=1 --worst=3 --tile=160",
    run,
};
GE_LAB(kSpec);

}  // namespace
