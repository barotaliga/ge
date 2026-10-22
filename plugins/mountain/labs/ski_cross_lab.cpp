// LAB `ski-cross`: A SKI-CROSS COURSE BUILT OVER A MAP (R35): the course close up from above (the
// graded face shaded by height, the course's edges and its fence, the berms banked, the features
// coloured along the line, the gates as flags, the four lanes, the finish line), its profile from
// the side (the ground under the line before it was built, the built line with its features),
// the map from above with the course on it, and the course's tables with its analysis. A claim
// about "the third berm on seed 38" is a claim about a row here.
//
// ge:summary Lab: a ski-cross course built over a map, close up, from the side and from above
// ge:keywords lab ski-cross berm rollers jump step-down corridor-gate profile r35 picture
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include "core/heightfield.hpp"
#include "core/libm/libm.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/analysis/index.hpp"
#include "mountain/labs/level_map.hpp"
#include "mountain/mapgen/digest.hpp"
#include "mountain/mapgen/ski_cross.hpp"

namespace {

namespace mg = ge::mountain::mapgen;
namespace an = ge::mountain::analysis;
namespace libm = ge::core::libm;
using ge::lab::Rgba;

std::string f1(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.1f", v);
  return b;
}

// Each feature's colour: a berm orange, rollers teal, a jump red, a step-down purple.
Rgba colour_of(mg::CrossFeature k) {
  switch (k) {
    case mg::CrossFeature::berm:
      return {235, 130, 20};
    case mg::CrossFeature::rollers:
      return {20, 160, 170};
    case mg::CrossFeature::jump:
      return {210, 30, 30};
    case mg::CrossFeature::step:
      return {130, 50, 170};
  }
  return {0, 0, 0};
}

// THE COURSE CLOSE UP: the box round the course and its finish area, `px` wide.
ge::lab::Picture draw_close(const mg::Level& level, int px) {
  const mg::RaceCourse& race = *level.race;
  const auto& pts = level.track.points;
  double x0 = std::numeric_limits<double>::infinity();
  double x1 = -x0;
  double z0 = x0;
  double z1 = -x0;
  for (const mg::TrackPoint& p : pts) {
    x0 = std::min(x0, p.x);
    x1 = std::max(x1, p.x);
    z0 = std::min(z0, p.z);
    z1 = std::max(z1, p.z);
  }
  constexpr double kPad = 50;
  x0 -= kPad;
  x1 += kPad;
  z0 -= kPad;
  z1 += kPad;
  const double k = px / (x1 - x0);
  const int ph = static_cast<int>(std::ceil((z1 - z0) * k));
  auto pic = ge::lab::create_picture(px, ph, {0, 0, 0});
  double lo = std::numeric_limits<double>::infinity();
  double hi = -lo;
  for (const mg::TrackPoint& p : pts) {
    lo = std::min(lo, p.y - 20);
    hi = std::max(hi, p.y + 20);
  }
  for (int py = 0; py < ph; py++) {
    for (int pxx = 0; pxx < px; pxx++) {
      const double x = x0 + (pxx + 0.5) / k;
      const double z = z0 + (py + 0.5) / k;
      const double t = std::clamp((level.ground_at(x, z) - lo) / (hi - lo), 0.0, 1.0);
      const auto s = ge::core::field_gradient(level.ground, x, z);
      const double nl = std::sqrt(s.gx * s.gx + 1 + s.gz * s.gz);
      const double lit = std::clamp((s.gx + 1.41421356 + s.gz) / (2 * nl), 0.0, 1.0);
      const double shade = 0.35 + 0.65 * lit;
      const double packed = std::clamp(level.packed_at(x, z), 0.0, 1.0);
      const double base = 150 + 100 * t;
      ge::lab::set_pixel(pic, pxx, py,
                         {static_cast<uint8_t>((base - 40 * packed) * shade),
                          static_cast<uint8_t>((base - 10 * packed) * shade),
                          static_cast<uint8_t>(std::min(255.0, base + 20 * packed) * shade)});
    }
  }
  const auto X = [&](double x) { return (x - x0) * k; };
  const auto Z = [&](double z) { return (z - z0) * k; };
  for (const mg::TreeDef& t : level.trees) {
    if (t.x < x0 || t.x > x1 || t.z < z0 || t.z > z1) continue;
    ge::lab::fill_disc(pic, X(t.x), Z(t.z), std::max(1.0, t.crown * k), {28, 72, 40, 230});
  }
  // The edges and the fence either side, the centreline coloured by the feature under it.
  const double fence = race.width / 2 + (race.nets ? race.nets->gap : 0);
  for (size_t i = 1; i < pts.size(); i++) {
    const mg::TrackPoint& a = pts[i - 1];
    const mg::TrackPoint& b = pts[i];
    for (const double side : {-1.0, 1.0}) {
      const auto edge = [&](const mg::TrackPoint& p, double off) {
        return std::pair<double, double>{X(p.x + libm::cos(p.heading) * off * side),
                                         Z(p.z - libm::sin(p.heading) * off * side)};
      };
      const auto ea = edge(a, a.width / 2);
      const auto eb = edge(b, b.width / 2);
      ge::lab::draw_line(pic, ea.first, ea.second, eb.first, eb.second, {40, 60, 120}, 1);
      if (b.s > race.from && b.s < race.to) {
        const auto fa = edge(a, fence);
        const auto fb = edge(b, fence);
        ge::lab::draw_line(pic, fa.first, fa.second, fb.first, fb.second, {60, 60, 60}, 1);
      }
    }
    Rgba c{90, 90, 110};
    for (const mg::RaceCourse::Feature& f : race.features) {
      if (b.s >= f.from && b.s <= f.to) c = colour_of(f.kind);
    }
    ge::lab::draw_line(pic, X(a.x), Z(a.z), X(b.x), Z(b.z), c, 3);
  }
  // The gates: a flag on the inside of a berm, a bar across before a feature, red and blue.
  const auto& cps = level.checkpoints;
  for (size_t i = 0; i < cps.size(); i++) {
    const mg::Checkpoint& g = cps[i];
    const double ax = libm::cos(g.heading);
    const double az = -libm::sin(g.heading);
    const Rgba c = i == 0                            ? Rgba{30, 160, 60}
                   : i + 1 == cps.size()             ? Rgba{0, 0, 0}
                   : g.colour == mg::GateColour::red ? Rgba{220, 20, 20}
                                                     : Rgba{20, 60, 220};
    if (g.pole) {
      const double off = g.turn * (g.width / 2 - 1);
      ge::lab::fill_disc(pic, X(g.x + ax * off), Z(g.z + az * off), 4, c);
    } else {
      const double h = g.width / 2;
      ge::lab::draw_line(pic, X(g.x - ax * h), Z(g.z - az * h), X(g.x + ax * h), Z(g.z + az * h), c,
                         2);
    }
    if (i > 0 && i + 1 < cps.size()) {
      ge::lab::draw_text(pic, X(g.x) + 8, Z(g.z) - 4, std::to_string(i), c);
    }
  }
  for (size_t i = 0; i < level.grid.size(); i++) {
    const mg::Spawn& l = level.grid[i];
    ge::lab::fill_disc(pic, X(l.x), Z(l.z), 2.5, {255, 255, 255});
  }
  ge::lab::draw_text(pic, X(level.grid[0].x) + 6, Z(level.grid[0].z) - 10, "1", {255, 255, 255});
  return pic;
}

// THE COURSE FROM THE SIDE, its mean fall taken out: arc across the page, height over the straight
// line from the top to the end up it (so a roller's half metre reads), the base map's ground under
// the line grey, the built line coloured by its features, black between them.
ge::lab::Picture draw_profile(const mg::Level& level, int w, int h) {
  const mg::RaceCourse& race = *level.race;
  const mg::Level& base = *race.base;
  const auto& pts = level.track.points;
  auto pic = ge::lab::create_picture(w, h, {246, 246, 242});
  const double length = level.track.length;
  const double fall = (pts.front().y - pts.back().y) / std::max(1.0, length);
  const auto over = [&](double s, double y) { return y - (pts.front().y - fall * s); };
  double lo = std::numeric_limits<double>::infinity();
  double hi = -lo;
  for (const mg::TrackPoint& p : pts) {
    lo = std::min({lo, over(p.s, p.y), over(p.s, base.ground_at(p.x, p.z))});
    hi = std::max({hi, over(p.s, p.y), over(p.s, base.ground_at(p.x, p.z))});
  }
  const auto X = [&](double s) { return 12 + (s / length) * (w - 24); };
  const auto Y = [&](double s, double y) {
    return 12 + (hi - over(s, y)) / std::max(1.0, hi - lo) * (h - 36);
  };
  for (size_t i = 1; i < pts.size(); i++) {
    const mg::TrackPoint& a = pts[i - 1];
    const mg::TrackPoint& b = pts[i];
    ge::lab::draw_line(pic, X(a.s), Y(a.s, base.ground_at(a.x, a.z)), X(b.s),
                       Y(b.s, base.ground_at(b.x, b.z)), {160, 160, 160}, 1);
    Rgba c{20, 20, 20};
    for (const mg::RaceCourse::Feature& f : race.features) {
      if (b.s >= f.from && b.s <= f.to) c = colour_of(f.kind);
    }
    ge::lab::draw_line(pic, X(a.s), Y(a.s, a.y), X(b.s), Y(b.s, b.y), c, 2);
  }
  const auto mark = [&](double s, Rgba c, const std::string& label) {
    ge::lab::draw_line(pic, X(s), 4, X(s), h - 18, c, 1);
    ge::lab::draw_text(pic, X(s) + 2, h - 14, label, c);
  };
  mark(race.from, {30, 160, 60}, "DOORS");
  mark(race.to, {0, 0, 0}, "FINISH");
  ge::lab::draw_text(pic, w - 150, 6, f1(hi - lo) + " M TOP TO BOTTOM", {80, 80, 80});
  return pic;
}

std::optional<mg::Level> course_map(ge::lab::Lab& lab, double seed, int version) {
  auto map = ge::mountain::labs::build_map(lab, seed, version);
  if (!map) return std::nullopt;
  if (const auto id = mg::ski_cross_course_of(*map)) {
    return ge::mountain::labs::build_map(lab, seed, version, std::nullopt, id);
  }
  return map;
}

void run(ge::lab::Lab& lab) {
  const int px = static_cast<int>(lab.arg_num("size"));
  for (const int version : ge::mountain::labs::map_versions(lab)) {
    for (const double seed : lab.arg_nums("seeds")) {
      const auto built = course_map(lab, seed, version);
      if (!built) continue;
      const mg::Level& map = *built;
      const std::string id = ge::mountain::labs::map_id(seed, version);
      const mg::Level course = mg::set_ski_cross(map);
      const mg::RaceCourse race = course.race.value_or(mg::RaceCourse{});
      const mg::RaceCourse::Stretch stretch = race.stretch.value_or(mg::RaceCourse::Stretch{0, 0});
      size_t errors = 0;
      for (const an::Finding& f : an::analyze_level(course).findings) {
        if (f.severity != an::Severity::error) continue;
        errors++;
        lab.fail(id + ": " + f.rule + " " + f.message,
                 "Read the course in this lab's pictures and fix the setter in "
                 "plugins/mountain/mapgen/ski_cross.cpp (skill: mountain).");
      }
      size_t counts[4] = {0, 0, 0, 0};
      for (const mg::RaceCourse::Feature& f : race.features) counts[static_cast<int>(f.kind)]++;
      const double length = race.to - race.from;
      lab.metric(id + ".length", length, "m");
      lab.metric(id + ".vertical", race.vertical, "m");
      lab.metric(id + ".grade", 100 * race.vertical / std::max(1.0, length), "%");
      lab.metric(id + ".berms", double(counts[0]), "count");
      lab.metric(id + ".rollers", double(counts[1]), "count");
      lab.metric(id + ".jumps", double(counts[2]), "count");
      lab.metric(id + ".step-downs", double(counts[3]), "count");
      lab.metric(id + ".gates", double(course.checkpoints.size()), "count");
      lab.metric(id + ".trees-cut", double(map.trees.size() - course.trees.size()), "count");
      lab.metric(id + ".errors", double(errors), "count");
      std::vector<std::vector<std::string>> rows;
      rows.reserve(race.features.size());
      for (const mg::RaceCourse::Feature& f : race.features) {
        rows.push_back({mg::cross_feature_name(f.kind), f1(f.from), f1(f.to),
                        f.kind == mg::CrossFeature::berm ? f1(f.angle) : f1(f.height),
                        f.kind == mg::CrossFeature::berm ? (f.side > 0 ? "right" : "left") : "-"});
      }
      lab.table({id + ": the features (arcs down the course, m)",
                 {"feature", "from", "to", "height m / turn deg", "inside"},
                 rows});
      lab.table(
          {id + ": the course",
           {"stretch", "doors", "finish", "drop", "gates", "digest"},
           {{f1(stretch.from) + "-" + f1(stretch.to), f1(race.from), f1(race.to), f1(race.vertical),
             std::to_string(course.checkpoints.size()), mg::level_digest(course)}}});
      auto inputs = ge::mountain::labs::map_inputs(lab, map);
      std::string told = "The course runs ";
      told.append(f1(length)).append(" m from the doors to the finish line and drops ");
      told.append(f1(race.vertical)).append(" m, with ").append(std::to_string(counts[0]));
      told.append(" berms, ").append(std::to_string(counts[1])).append(" sets of rollers, ");
      told.append(std::to_string(counts[2])).append(" jumps and ");
      told.append(std::to_string(counts[3])).append(" step-downs.");
      const std::string name = "SEED " + std::to_string(static_cast<long long>(seed));
      lab.shot({id + "-course", name + " SKI CROSS, CLOSE UP",
                "The course from above, the face as graded: its edges blue, its fence grey, the "
                "line coloured by the feature on it (a berm orange, rollers teal, a jump red, a "
                "step-down purple, else grey); a turning gate a dot on its berm's inside, a "
                "corridor gate a bar across, red and blue in turn and numbered; the start gate "
                "green with its four lanes white (1 the inside of the first berm), the finish "
                "black. " +
                    told,
                "the line weaving smoothly side to side, every berm with its gate on the inside, "
                "a gate before every feature, the course's fence and spill zones clear of trees",
                inputs},
               draw_close(course, px));
      lab.shot({id + "-profile", name + " SKI CROSS, SIDE",
                "The course from the side with its mean fall taken out (height over the straight "
                "line from its top to its end, stretched), the doors at the left: the ground "
                "under the line before the course was built grey, the built line coloured by its "
                "features (black between them).",
                "the line never rising between the features, the start ramp steep under the "
                "doors, each jump a ramp, a table and a long landing",
                inputs},
               draw_profile(course, px, std::max(200, px / 3)));
      lab.shot(
          {id + "-map", name + " SKI CROSS FROM ABOVE",
           "The map from above with the course built into it: the face shaded by height, "
           "the packed snow blue, the woods dark green, the course's line red and its gates "
           "numbered.",
           "the course on a gentle stretch low on the piste, finishing near the valley", inputs},
          ge::mountain::labs::draw_map(course, px));
    }
  }
}

const ge::lab::LabSpec kSpec{
    "ski-cross",
    ge::lab::LabKind::picture,
    "plugin:mountain",
    "plugins/mountain/mapgen",
    "mountain",
    "Where is a ski-cross course built on this map, and are its line, berms, features and gates "
    "to rule?",
    {
        {"seeds", "38", "seeds, comma-separated"},
        {"size", "1024", "the pictures' width, px"},
        {"version", "8", "generator versions, comma-separated: 1 the single piste, 8 the ski area"},
        ge::mountain::labs::kRegionFlag,
    },
    "--seeds=38 --size=384 --version=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
