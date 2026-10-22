// LAB `park`: THE TERRAIN PARK (R20) on a map built for a tricks run: the park close up from above
// (the stamped face shaded by height, the line coloured by the kicker on it, each lip and its
// landing marked, the gates numbered), the park from the side (the line's height with its mean
// fall taken out, each kicker's ramp, deck, landing and run-out coloured by its size, the gates as
// ticks), the map from above, and the kickers' table with the map's analysis. A claim about "the
// third high kicker on seed 38" is a claim about a row here.
//
// ge:summary Lab: the terrain park on a tricks map, close up, from the side and from above
// ge:keywords lab terrain-park park trick-kicker built-landing run-in profile r20 picture
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
#include "mountain/mapgen/query.hpp"

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

// Each size's colour: a low kicker green, a medium blue, a high red.
Rgba colour_of(mg::TrickSize z) {
  switch (z) {
    case mg::TrickSize::low:
      return {30, 160, 60};
    case mg::TrickSize::medium:
      return {30, 80, 220};
    case mg::TrickSize::high:
      return {210, 30, 30};
  }
  return {0, 0, 0};
}

// The park's kickers in the order skied.
std::vector<const mg::Kicker*> park_of(const mg::Level& level) {
  std::vector<const mg::Kicker*> out;
  for (const mg::Kicker& k : level.kickers) {
    if (k.trick) out.push_back(&k);
  }
  std::stable_sort(out.begin(), out.end(), [](const mg::Kicker* a, const mg::Kicker* b) {
    return a->s.value_or(0) < b->s.value_or(0);
  });
  return out;
}

// The park kicker over arc `s` of the line, if any.
const mg::Kicker* kicker_at(const std::vector<const mg::Kicker*>& park, double s) {
  for (const mg::Kicker* k : park) {
    const double at = k->s.value_or(0);
    if (s >= at - k->ramp && s <= at + k->landing) return k;
  }
  return nullptr;
}

// THE PARK CLOSE UP: the box round the line from the first ramp's foot to the last run-out, `px`
// wide.
ge::lab::Picture draw_close(const mg::Level& level, const std::vector<const mg::Kicker*>& park,
                            int px) {
  const double from = park.front()->s.value_or(0) - park.front()->ramp - 40;
  const double to = park.back()->s.value_or(0) + park.back()->landing + 40;
  double x0 = std::numeric_limits<double>::infinity();
  double x1 = -x0;
  double z0 = x0;
  double z1 = -x0;
  for (const mg::TrackPoint& p : level.track.points) {
    if (p.s < from || p.s > to) continue;
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
  const int ph = std::min(4 * px, static_cast<int>(std::ceil((z1 - z0) * k)));
  auto pic = ge::lab::create_picture(px, ph, {0, 0, 0});
  for (int py = 0; py < ph; py++) {
    for (int pxx = 0; pxx < px; pxx++) {
      const double x = x0 + (pxx + 0.5) / k;
      const double z = z0 + (py + 0.5) / k;
      const auto g = ge::core::field_gradient(level.ground, x, z);
      const double nl = std::sqrt(g.gx * g.gx + 1 + g.gz * g.gz);
      const double lit = std::clamp((g.gx + 1.41421356 + g.gz) / (2 * nl), 0.0, 1.0);
      const double shade = 0.3 + 0.7 * lit;
      const double packed = std::clamp(level.packed_at(x, z), 0.0, 1.0);
      const double base = 230;
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
  const auto& pts = level.track.points;
  for (size_t i = 1; i < pts.size(); i++) {
    const mg::TrackPoint& a = pts[i - 1];
    const mg::TrackPoint& b = pts[i];
    const mg::Kicker* on = kicker_at(park, b.s);
    ge::lab::draw_line(pic, X(a.x), Z(a.z), X(b.x), Z(b.z),
                       on ? colour_of(*on->trick) : Rgba{90, 90, 110}, on ? 3 : 1);
  }
  for (const mg::Kicker* kk : park) {
    const double ax = libm::cos(kk->heading);
    const double az = -libm::sin(kk->heading);
    const double h = kk->width / 2;
    ge::lab::draw_line(pic, X(kk->x - ax * h), Z(kk->z - az * h), X(kk->x + ax * h),
                       Z(kk->z + az * h), {0, 0, 0}, 2);
    ge::lab::draw_text(pic, X(kk->x + ax * h) + 4, Z(kk->z + az * h) - 4, kk->id, {0, 0, 0});
  }
  const auto& cps = level.checkpoints;
  for (size_t i = 0; i < cps.size(); i++) {
    const mg::Checkpoint& g = cps[i];
    if (g.s < from || g.s > to) continue;
    const double ax = libm::cos(g.heading);
    const double az = -libm::sin(g.heading);
    const double h = g.width / 2;
    ge::lab::draw_line(pic, X(g.x - ax * h), Z(g.z - az * h), X(g.x + ax * h), Z(g.z + az * h),
                       {230, 120, 20}, 1);
    ge::lab::draw_text(pic, X(g.x - ax * h) - 22, Z(g.z - az * h) - 4, std::to_string(i),
                       {230, 120, 20});
  }
  return pic;
}

// THE PARK FROM THE SIDE, its mean fall taken out: arc across the page from the first ramp's foot
// to the last run-out, height over the straight line between them up it (so a lip's metre reads),
// each kicker coloured by its size and the line between black, the gates orange ticks.
ge::lab::Picture draw_profile(const mg::Level& level, const std::vector<const mg::Kicker*>& park,
                              int w, int h) {
  const double from = park.front()->s.value_or(0) - park.front()->ramp - 30;
  const double to = park.back()->s.value_or(0) + park.back()->landing + 30;
  auto pic = ge::lab::create_picture(w, h, {246, 246, 242});
  const auto y_at = [&](double s) {
    const mg::TrackPoint p = mg::track_point_at(level.track, s);
    return level.ground_at(p.x, p.z);
  };
  const double y0 = y_at(from);
  const double fall = (y0 - y_at(to)) / std::max(1.0, to - from);
  const auto over = [&](double s) { return y_at(s) - (y0 - fall * (s - from)); };
  double lo = std::numeric_limits<double>::infinity();
  double hi = -lo;
  constexpr double kStep = 0.5;
  const int steps = static_cast<int>((to - from) / kStep);
  for (int i = 0; i <= steps; i++) {
    const double s = from + i * kStep;
    lo = std::min(lo, over(s));
    hi = std::max(hi, over(s));
  }
  const auto X = [&](double s) { return 12 + (s - from) / (to - from) * (w - 24); };
  const auto Y = [&](double v) { return 12 + (hi - v) / std::max(1.0, hi - lo) * (h - 36); };
  for (int i = 1; i <= steps; i++) {
    const double s = from + i * kStep;
    const mg::Kicker* on = kicker_at(park, s);
    ge::lab::draw_line(pic, X(s - kStep), Y(over(s - kStep)), X(s), Y(over(s)),
                       on ? colour_of(*on->trick) : Rgba{20, 20, 20}, 2);
  }
  for (const mg::Kicker* k : park) {
    const double s = k->s.value_or(0);
    ge::lab::draw_text(pic, X(s) - 6, Y(over(s)) - 14, k->id, colour_of(*k->trick));
  }
  for (const mg::Checkpoint& g : level.checkpoints) {
    if (g.s < from || g.s > to) continue;
    ge::lab::draw_line(pic, X(g.s), h - 22, X(g.s), h - 10, {230, 120, 20}, 2);
  }
  ge::lab::draw_text(pic, w - 170, 6, f1(hi - lo) + " M TOP TO BOTTOM", {80, 80, 80});
  return pic;
}

void run(ge::lab::Lab& lab) {
  const int px = static_cast<int>(lab.arg_num("size"));
  for (const int version : ge::mountain::labs::map_versions(lab)) {
    for (const double seed : lab.arg_nums("seeds")) {
      const auto built =
          ge::mountain::labs::build_map(lab, seed, version, std::nullopt, std::nullopt, true);
      if (!built) continue;
      const mg::Level& map = *built;
      const std::string id = ge::mountain::labs::map_id(seed, version);
      const std::vector<const mg::Kicker*> park = park_of(map);
      size_t errors = 0;
      for (const an::Finding& f : an::analyze_level(map).findings) {
        if (f.severity != an::Severity::error) continue;
        errors++;
        lab.fail(id + ": " + f.rule + " " + f.message,
                 "Read the park in this lab's pictures and fix the planner or the stamp in "
                 "plugins/mountain/mapgen/park.cpp (skill: mountain).");
      }
      if (park.empty()) {
        lab.fail(id + ": the tricks map carries no park",
                 "Run with MOUNTAIN debug output to see each course's refusal and fix "
                 "plugins/mountain/mapgen/park.cpp (skill: mountain).");
        continue;
      }
      size_t counts[3] = {0, 0, 0};
      for (const mg::Kicker* k : park) counts[static_cast<int>(*k->trick)]++;
      const double span = park.back()->s.value_or(0) + park.back()->landing -
                          (park.front()->s.value_or(0) - park.front()->ramp);
      lab.metric(id + ".kickers", double(park.size()), "count");
      lab.metric(id + ".low", double(counts[0]), "count");
      lab.metric(id + ".medium", double(counts[1]), "count");
      lab.metric(id + ".high", double(counts[2]), "count");
      lab.metric(id + ".span", span, "m");
      lab.metric(id + ".gates", double(map.checkpoints.size()), "count");
      lab.metric(id + ".errors", double(errors), "count");
      std::vector<std::vector<std::string>> rows;
      rows.reserve(park.size());
      for (const mg::Kicker* k : park) {
        const double s = k->s.value_or(0);
        rows.push_back({k->id, mg::trick_size_name(*k->trick), f1(s - k->ramp), f1(s),
                        f1(s + k->landing), f1(k->height), f1(k->shape ? k->shape->dig : 0)});
      }
      lab.table({id + ": the park (arcs down the line, m)",
                 {"kicker", "size", "foot", "lip", "end", "lip m", "dig m"},
                 rows});
      lab.table({id + ": the map",
                 {"course", "length", "kickers", "gates", "digest"},
                 {{map.resort ? map.resort->course : std::string("-"), f1(map.track.length),
                   std::to_string(park.size()), std::to_string(map.checkpoints.size()),
                   mg::level_digest(map)}}});
      auto inputs = ge::mountain::labs::map_inputs(lab, map);
      inputs.emplace_back("tricks", "1");
      std::string told = "The park carries " + std::to_string(park.size()) + " kickers (";
      told.append(std::to_string(counts[0])).append(" low, ").append(std::to_string(counts[1]));
      told.append(" medium, ").append(std::to_string(counts[2])).append(" high) over ");
      told.append(f1(span)).append(" m of the line.");
      const std::string name = "SEED " + std::to_string(static_cast<long long>(seed));
      lab.shot({id + "-park", name + " TERRAIN PARK, CLOSE UP",
                "The park from above, the face as stamped and lit from the north-west: the line "
                "coloured by the kicker on it (low green, medium blue, high red, grey between), "
                "each lip a black bar across with its id, the gates orange and numbered, the "
                "woods dark green. " +
                    told,
                "the kickers in a ladder of sizes down a straight stretch of the line, each "
                "landing a dug slope below its lip, no gate on a kicker, no tree in the park",
                inputs},
               draw_close(map, park, px));
      lab.shot({id + "-profile", name + " TERRAIN PARK, SIDE",
                "The park from the side with its mean fall taken out (height over the straight "
                "line from its first ramp's foot to its last run-out, stretched), skied left to "
                "right: each kicker coloured by its size (low green, medium blue, high red), "
                "the line between black, the gates orange ticks at the foot.",
                "every kicker a ramp up to its lip, a level deck, a landing slope falling below "
                "the line and a run-out back up to it; a gate before each ramp, none on a kicker",
                inputs},
               draw_profile(map, park, px, std::max(200, px / 3)));
      lab.shot({id + "-map", name + " TERRAIN PARK FROM ABOVE",
                "The map from above with the park stamped into it: the face shaded by height, "
                "the packed snow blue, the woods dark green, the raced line red and its gates "
                "numbered.",
                "the park on the raced line, clear of its start and its finish", inputs},
               ge::mountain::labs::draw_map(map, px));
    }
  }
}

const ge::lab::LabSpec kSpec{
    "park",
    ge::lab::LabKind::picture,
    "plugin:mountain",
    "plugins/mountain/mapgen",
    "mountain",
    "Where does the terrain park stand on a tricks map, and is every kicker built and spaced to "
    "rule?",
    {
        {"seeds", "38", "seeds, comma-separated"},
        {"size", "1024", "the pictures' width, px"},
        {"version", "1", "generator versions, comma-separated: 1 the single piste, 8 the ski area"},
        ge::mountain::labs::kRegionFlag,
    },
    "--seeds=38 --size=384 --version=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
