// LAB `skijump-hills`: THE SKI JUMPING HILL SET OVER A MAP (R43): what the hill does to the ground
// it is laid on, a row a map and a hill, read against the map's own ground under it:
//
//   tower   how far the start tower's deck stands over the map's ground at the top gate, m (the
//           in-run a structure the higher it is)
//   raised  the most the in-run stands over the map's ground between the top gate and the edge, m
//   dig     the deepest the map's ground is cut under the raised in-run to the slope it is laid
//           at (`venue_rules::ski_jump::raised`), m (a hillside the tower is sunk into)
//   fill    the most the landing hill (the edge to U) is built up over the map's ground, m, and
//   cut     the most it is dug into it
//   rms     the landing hill's and the out-run's root-mean-square cut and fill, m (the earth moved)
//   arena   the most the ground beside the out-run (where the stands go, 20 to 40 m off the
//           sides) lies off its level, m
//   drop    the drop from the top gate to the fall line, m
//
// with the hill's profile from the side at true scale (the map's ground under the line grey, the
// graded grid brown, the hill's surface black, the gates, the edge, P, K, L and U marked) and the
// map from above. A claim about "the tower on seed 38's large hill" is a claim about a row here.
//
// ge:summary Lab: the ski jumping hill set over a map, the earth it moves, its profile and its map
// ge:keywords lab ski-jump jumping-hill tower raised-in-run cut fill arena profile r43 picture
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
#include "lab/raster.hpp"
#include "mountain/labs/level_map.hpp"
#include "mountain/mapgen/digest.hpp"
#include "mountain/mapgen/ski_jump.hpp"
#include "mountain/mapgen/venue_rules.hpp"

namespace {

namespace mg = ge::mountain::mapgen;
namespace libm = ge::core::libm;
namespace R = mg::venue_rules::ski_jump;
using ge::lab::Rgba;

std::string f1(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.1f", v);
  return b;
}

// The hill's line on a map carrying one: a point `d` m of plan along it and `across` m to its
// right.
struct Line {
  double x, z, fx, fz;
  double px(double d, double across = 0) const { return x + fx * d + fz * across; }
  double pz(double d, double across = 0) const { return z + fz * d - fx * across; }
};

Line line_of(const mg::Level& level) {
  const mg::TrackPoint& p = level.track.points.front();
  return {p.x, p.z, libm::sin(p.heading), libm::cos(p.heading)};
}

// What the hill set over its map did to the ground (the header's columns).
struct Earth {
  double tower = 0, raised = 0, dig = 0, fill = 0, cut = 0, rms = 0, arena = 0;
  double above = 0;  // the most the cut grid stands over the hill's surface along the line, m
  size_t trees = 0;  // trees left standing on the hill
};

Earth measure(const mg::Level& level, const mg::SkiJumpHill& hill) {
  const mg::Level& base = *hill.base;
  const Line ln = line_of(level);
  const auto y = [&](double d) { return mg::ski_jump_height_at(level, d); };
  const auto over = [&](double d) { return y(d) - base.ground_at(ln.px(d), ln.pz(d)); };
  Earth e;
  e.tower = over(hill.from);
  const double rise = libm::tan(R::raised::slope * ge::core::kPi / 180);
  for (int i = 0; hill.from + i <= hill.edge; i++) {
    const double d = hill.from + i;
    e.raised = std::max(e.raised, over(d));
    const double under = y(hill.edge) - hill.lip + (hill.edge - d) * rise;
    e.dig = std::max(e.dig, base.ground_at(ln.px(d), ln.pz(d)) - under);
  }
  double sq = 0;
  int n = 0;
  for (int i = 1; hill.edge + i <= hill.outrun; i++) {
    const double d = hill.edge + i;
    const double v = over(d);
    if (d <= hill.u) {
      e.fill = std::max(e.fill, v);
      e.cut = std::max(e.cut, -v);
    }
    sq += v * v;
    n++;
  }
  e.rms = std::sqrt(sq / std::max(1, n));
  const double half = R::width / 2;
  for (int i = 0; hill.u + 4 * i <= hill.outrun; i++) {
    const double d = hill.u + 4 * i;
    for (const double off : {half + 20, half + 30, half + 40}) {
      for (const double side : {-1.0, 1.0}) {
        const double g = base.ground_at(ln.px(d, side * off), ln.pz(d, side * off));
        e.arena = std::max(e.arena, std::fabs(g - y(hill.outrun)));
      }
    }
  }
  for (int d = 0; d <= hill.outrun; d++) {
    e.above =
        std::max(e.above, double(ge::core::sample_field(level.ground, ln.px(d), ln.pz(d))) - y(d));
  }
  for (const mg::TreeDef& t : level.trees) {
    const double along = (t.x - ln.x) * ln.fx + (t.z - ln.z) * ln.fz;
    const double across = (t.x - ln.x) * ln.fz - (t.z - ln.z) * ln.fx;
    if (along > 0 && along < hill.outrun && std::fabs(across) < half) e.trees++;
  }
  return e;
}

// THE HILL FROM THE SIDE at true scale: plan distance across, height up; the map's ground under
// the line grey, the cut grid brown, the hill's surface black; the gates green ticks, the edge
// red, P, K, L and U blue.
ge::lab::Picture draw_profile(const mg::Level& level, const mg::SkiJumpHill& hill, int w) {
  const mg::Level& base = *hill.base;
  const Line ln = line_of(level);
  const double end = hill.outrun;
  double lo = mg::ski_jump_height_at(level, end);
  double hi = mg::ski_jump_height_at(level, 0);
  for (int d = 0; d <= end; d++) {
    lo = std::min(lo, base.ground_at(ln.px(d), ln.pz(d)));
    hi = std::max(hi, base.ground_at(ln.px(d), ln.pz(d)));
  }
  const double k = (w - 24) / end;
  const int h = std::max(160, static_cast<int>(std::ceil((hi - lo) * k)) + 40);
  auto pic = ge::lab::create_picture(w, h, {246, 246, 242});
  const auto X = [&](double d) { return 12 + d * k; };
  const auto Y = [&](double y) { return 12 + (hi - y) * k; };
  const auto trace = [&](auto&& height, Rgba c, int thick) {
    for (int d = 1; d <= end; d++) {
      ge::lab::draw_line(pic, X(d - 1), Y(height(d - 1)), X(d), Y(height(d)), c, thick);
    }
  };
  trace([&](double d) { return base.ground_at(ln.px(d), ln.pz(d)); }, {150, 150, 150}, 1);
  trace([&](double d) { return double(ge::core::sample_field(level.ground, ln.px(d), ln.pz(d))); },
        {160, 110, 60}, 1);
  trace([&](double d) { return mg::ski_jump_height_at(level, d); }, {20, 20, 20}, 2);
  for (const mg::SkiJumpHill::Gate& g : hill.gates) {
    ge::lab::draw_line(pic, X(g.x), Y(g.y) - 6, X(g.x), Y(g.y) + 2, {30, 160, 60}, 1);
  }
  const auto mark = [&](double d, Rgba c, const std::string& label) {
    const double yy = Y(mg::ski_jump_height_at(level, d));
    ge::lab::draw_line(pic, X(d), yy - 10, X(d), yy + 10, c, 2);
    ge::lab::draw_text(pic, X(d) + 3, yy + 6, label, c);
  };
  mark(hill.edge, {210, 30, 30}, "EDGE");
  mark(hill.p, {30, 70, 220}, "P");
  mark(hill.k, {30, 70, 220}, "K");
  mark(hill.l, {30, 70, 220}, "L");
  mark(hill.u, {30, 70, 220}, "U");
  return pic;
}

void run(ge::lab::Lab& lab) {
  const int px = static_cast<int>(lab.arg_num("size"));
  std::vector<mg::JumpHill> hills;
  const std::string& asked = lab.arg("hill");
  if (asked.find("large") != std::string::npos) hills.push_back(mg::JumpHill::large);
  if (asked.find("normal") != std::string::npos) hills.push_back(mg::JumpHill::normal);
  if (hills.empty()) {
    lab.fail("--hill=" + asked + " names no hill", "Ask for --hill=large, normal or large,normal.");
    return;
  }
  std::vector<std::vector<std::string>> rows;
  for (const int version : ge::mountain::labs::map_versions(lab)) {
    for (const double seed : lab.arg_nums("seeds")) {
      const auto built = ge::mountain::labs::build_map(lab, seed, version);
      if (!built) continue;
      const mg::Level& map = *built;
      const auto inputs = ge::mountain::labs::map_inputs(lab, map);
      for (const mg::JumpHill which : hills) {
        const std::string name = mg::jump_hill_name(which);
        const std::string id = ge::mountain::labs::map_id(seed, version) + "." + name;
        const mg::Level level = mg::set_ski_jump(map, which);
        if (!level.ski_jump) {
          lab.fail(id + ": set_ski_jump returned a map with no hill",
                   "Read set_ski_jump in plugins/mountain/mapgen/ski_jump.cpp (skill: mountain).");
          continue;
        }
        const mg::SkiJumpHill& hill = *level.ski_jump;
        const Earth e = measure(level, hill);
        if (e.above > 0.05 || e.trees > 0) {
          lab.fail(id + ": the grid stands " + f1(e.above) + " m over the hill's surface and " +
                       std::to_string(e.trees) + " trees stand on it",
                   "Read the hill in this lab's pictures and fix cut_under or the clearing in "
                   "plugins/mountain/mapgen/ski_jump.cpp (skill: mountain).");
        }
        const double heading = level.track.points.front().heading * 180 / ge::core::kPi;
        lab.metric(id + ".tower", e.tower, "m");
        lab.metric(id + ".raised", e.raised, "m");
        lab.metric(id + ".dig", e.dig, "m");
        lab.metric(id + ".fill", e.fill, "m");
        lab.metric(id + ".cut", e.cut, "m");
        lab.metric(id + ".rms", e.rms, "m");
        lab.metric(id + ".arena", e.arena, "m");
        lab.metric(id + ".drop", hill.vertical, "m");
        lab.metric(id + ".trees-cut", double(map.trees.size() - level.trees.size()), "count");
        rows.push_back({id, f1(heading), f1(e.tower), f1(e.raised), f1(e.dig), f1(e.fill),
                        f1(e.cut), f1(e.rms), f1(e.arena), f1(hill.vertical),
                        std::to_string(hill.design + 1) + "/" + std::to_string(hill.gates.size()),
                        mg::level_digest(level)});
        const std::string title = "SEED " + std::to_string(static_cast<long long>(seed)) + " " +
                                  (which == mg::JumpHill::large ? "LARGE" : "NORMAL") + " HILL";
        lab.shot({id + "-profile", title + ", SIDE",
                  "The hill from the side at true scale, the top at the left: the map's ground "
                  "under the line grey, the grid cut under the hill brown, the hill's surface "
                  "black; the start gates green ticks, the edge red, P, K, L and U blue. The tower "
                  "stands " +
                      f1(e.tower) + " m over the map's ground, the landing hill moves " +
                      f1(e.rms) + " m of earth (rms).",
                  "the in-run raised over a brown cut that never stands above it; the landing "
                  "hill hugging the grey ground; a level out-run",
                  inputs},
                 draw_profile(level, hill, px));
        lab.shot({id + "-map", title + " FROM ABOVE",
                  "The map from above with the hill cut into it: the face shaded by height, the "
                  "packed snow blue, the woods dark green, the hill's line red.",
                  "the hill straight down the face, its sides and finish arena cleared, clear of "
                  "the lifts and the village",
                  inputs},
                 ge::mountain::labs::draw_map(level, px));
      }
    }
  }
  std::sort(rows.begin(), rows.end(),
            [](const auto& a, const auto& b) { return std::stod(a[7]) < std::stod(b[7]); });
  lab.table({"the hills, least earth moved first (m)",
             {"map.hill", "heading", "tower", "raised", "dig", "fill", "cut", "rms", "arena",
              "drop", "design gate", "digest"},
             rows});
}

const ge::lab::LabSpec kSpec{
    "skijump-hills",
    ge::lab::LabKind::picture,
    "plugin:mountain",
    "plugins/mountain/mapgen",
    "mountain",
    "Where is a ski jumping hill cut on this map, how high its tower stands and how much earth "
    "its landing hill moves?",
    {
        {"seeds", "38", "seeds, comma-separated"},
        {"size", "1024", "the pictures' width, px"},
        {"version", "8", "generator versions, comma-separated: 1 the single piste, 8 the ski area"},
        {"hill", "large,normal", "the hills, comma-separated: large, normal"},
        ge::mountain::labs::kRegionFlag,
    },
    "--seeds=38 --size=384 --version=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
