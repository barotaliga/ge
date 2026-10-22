// LAB `routes`: THE SKI ROUTES (R42) on a ski area: the area from above with every route drawn in
// orange over its runs and lifts (a wild line's drops and jumps marked), each route close up (the
// face as stamped, the woods it threads and the lane felled through them, its drops and jumps),
// each route from the side (its height down its arc, its pitch over a colour window under it,
// the black's ceiling and the route's), and the routes' table with the map's analysis. A claim
// about "the second drop on SR1 of seed 38" is a claim about a row here.
//
// ge:summary Lab: the ski routes on a ski area, from above, close up and from the side
// ge:keywords lab ski-route orange-route wild-route route-jump drop profile r42 picture
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
#include "core/math.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/analysis/index.hpp"
#include "mountain/labs/level_map.hpp"
#include "mountain/labs/resort_map.hpp"
#include "mountain/mapgen/digest.hpp"
#include "mountain/mapgen/resort_rules.hpp"
#include "mountain/mapgen/rules.hpp"
#include "mountain/mapgen/ski_routes.hpp"

namespace {

namespace mg = ge::mountain::mapgen;
namespace an = ge::mountain::analysis;
namespace libm = ge::core::libm;
using ge::lab::Rgba;

constexpr Rgba kOrange{240, 130, 10};
constexpr Rgba kDrop{150, 0, 150};

std::string f0(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.0f", v);
  return b;
}

// A box in plan, m.
struct Box {
  double x0, x1, z0, z1;
};

// The face in `box` shaded by height and lit from the north-west, the packed snow blue, `px`
// wide; the woods dark green over it.
ge::lab::Picture draw_ground(const mg::Level& level, const Box& box, int px, int& ph) {
  const double k = px / (box.x1 - box.x0);
  ph = std::min(4 * px, static_cast<int>(std::ceil((box.z1 - box.z0) * k)));
  auto pic = ge::lab::create_picture(px, ph, {0, 0, 0});
  for (int py = 0; py < ph; py++) {
    for (int pxx = 0; pxx < px; pxx++) {
      const double x = box.x0 + (pxx + 0.5) / k;
      const double z = box.z0 + (py + 0.5) / k;
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
  for (const mg::TreeDef& t : level.trees) {
    if (t.x < box.x0 || t.x > box.x1 || t.z < box.z0 || t.z > box.z1) continue;
    ge::lab::fill_disc(pic, (t.x - box.x0) * k, (t.z - box.z0) * k, std::max(0.6, t.crown * k),
                       {28, 72, 40, 220});
  }
  return pic;
}

void polyline(ge::lab::Picture& pic, const std::vector<mg::TrackPoint>& pts, const Box& box,
              double k, Rgba c, int thick) {
  for (size_t i = 1; i < pts.size(); i++) {
    ge::lab::draw_line(pic, (pts[i - 1].x - box.x0) * k, (pts[i - 1].z - box.z0) * k,
                       (pts[i].x - box.x0) * k, (pts[i].z - box.z0) * k, c, thick);
  }
}

// The station of a route at arc `s`.
const mg::TrackPoint& at_arc(const mg::SkiRoute& r, double s) {
  const auto i = static_cast<size_t>(
      std::clamp(std::lround(s / 4), 0L, static_cast<long>(r.points.size()) - 1));
  return r.points[i];
}

// A route's drops and jumps over it: a magenta ring at each drop's top, a black bar across each
// jump's lip with its id.
void marks(ge::lab::Picture& pic, const mg::Level& level, const mg::SkiRoute& r, const Box& box,
           double k) {
  for (const mg::SkiRoute::Drop& d : r.drops) {
    const mg::TrackPoint& p = at_arc(r, d.s);
    ge::lab::draw_ring(pic, (p.x - box.x0) * k, (p.z - box.z0) * k, 5, kDrop);
  }
  for (const mg::Kicker& kk : level.kickers) {
    if (kk.route != r.id) continue;
    const double ax = libm::cos(kk.heading);
    const double az = -libm::sin(kk.heading);
    const double h = std::max(kk.width / 2, 3 / k);
    ge::lab::draw_line(pic, (kk.x - ax * h - box.x0) * k, (kk.z - az * h - box.z0) * k,
                       (kk.x + ax * h - box.x0) * k, (kk.z + az * h - box.z0) * k, {0, 0, 0}, 2);
    ge::lab::draw_text(pic, (kk.x + ax * h - box.x0) * k + 3, (kk.z + az * h - box.z0) * k - 4,
                       kk.id, {0, 0, 0});
  }
}

// THE AREA FROM ABOVE: the runs in their colours, the lifts, every route orange and its id.
ge::lab::Picture draw_area(const mg::Level& level, int px) {
  const Box box{0, level.size, 0, level.size};
  int ph = 0;
  auto pic = draw_ground(level, box, px, ph);
  const double k = px / level.size;
  if (!level.resort) return pic;
  const mg::ResortMap& area = *level.resort;
  for (const mg::Run& r : area.runs) {
    const bool road = r.kind == mg::RunKind::road;
    polyline(pic, r.points, box, k,
             road ? Rgba{120, 120, 120} : ge::mountain::labs::grade_colour(r.grade), road ? 1 : 2);
  }
  for (const mg::Lift& l : area.lifts) {
    ge::lab::draw_line(pic, l.bottom.x * k, l.bottom.z * k, l.top.x * k, l.top.z * k, {60, 60, 60},
                       1);
  }
  for (const mg::SkiRoute& r : area.routes) {
    polyline(pic, r.points, box, k, kOrange, 3);
    marks(pic, level, r, box, k);
    const mg::TrackPoint& p = r.points[r.points.size() / 3];
    ge::lab::draw_text(pic, p.x * k + 6, p.z * k, r.id, kOrange, 2);
  }
  return pic;
}

// ONE ROUTE CLOSE UP: the box round it, its line orange, its drops and jumps marked.
ge::lab::Picture draw_close(const mg::Level& level, const mg::SkiRoute& r, int px) {
  Box box{std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
          std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
  for (const mg::TrackPoint& p : r.points) {
    box.x0 = std::min(box.x0, p.x);
    box.x1 = std::max(box.x1, p.x);
    box.z0 = std::min(box.z0, p.z);
    box.z1 = std::max(box.z1, p.z);
  }
  constexpr double kPad = 60;
  box = {box.x0 - kPad, box.x1 + kPad, box.z0 - kPad, box.z1 + kPad};
  int ph = 0;
  auto pic = draw_ground(level, box, px, ph);
  const double k = px / (box.x1 - box.x0);
  polyline(pic, r.points, box, k, kOrange, 2);
  marks(pic, level, r, box, k);
  return pic;
}

// ONE ROUTE FROM THE SIDE: its height down its arc (black), its pitch over a colour window under
// it (orange) against the black's ceiling (grey) and the route's (red), its drops magenta ticks,
// its jumps black ticks.
ge::lab::Picture draw_profile(const mg::Level& level, const mg::SkiRoute& r, int w, int h) {
  auto pic = ge::lab::create_picture(w, h, {246, 246, 242});
  const double top = r.points.front().y;
  const double low = r.points.back().y;
  const double half = h / 2.0;
  const auto X = [&](double s) { return 12 + s / r.length * (w - 24); };
  const auto Y = [&](double y) { return 10 + (top - y) / std::max(1.0, top - low) * (half - 20); };
  for (size_t i = 1; i < r.points.size(); i++) {
    ge::lab::draw_line(pic, X(r.points[i - 1].s), Y(r.points[i - 1].y), X(r.points[i].s),
                       Y(r.points[i].y), {20, 20, 20}, 2);
  }
  // The pitch band: 0 at the foot of the picture, 1.3 (a cliff's) at the half.
  constexpr double kTopPitch = 1.3;
  const auto P = [&](double v) { return h - 10 - v / kTopPitch * (half - 20); };
  ge::lab::draw_line(pic, 12, P(mg::rules::track::max_grade), w - 12,
                     P(mg::rules::track::max_grade), {150, 150, 150}, 1);
  ge::lab::draw_line(pic, 12, P(mg::resort_rules::route::steepest), w - 12,
                     P(mg::resort_rules::route::steepest), {210, 30, 30}, 1);
  const double window = mg::rules::track::colour_window;
  size_t j = 0;
  double last_x = -1;
  double last_y = 0;
  for (size_t i = 0; i < r.points.size(); i++) {
    while (j < r.points.size() && r.points[j].s - r.points[i].s < window) j++;
    if (j >= r.points.size()) break;
    const double pitch = (r.points[i].y - r.points[j].y) / (r.points[j].s - r.points[i].s);
    const double x = X((r.points[i].s + r.points[j].s) / 2);
    if (last_x >= 0) ge::lab::draw_line(pic, last_x, last_y, x, P(pitch), kOrange, 2);
    last_x = x;
    last_y = P(pitch);
  }
  for (const mg::SkiRoute::Drop& d : r.drops) {
    ge::lab::draw_line(pic, X(d.s), half - 14, X(d.s), half - 2, kDrop, 3);
  }
  for (const mg::Kicker& k : level.kickers) {
    if (k.route != r.id) continue;
    double best = std::numeric_limits<double>::infinity();
    double s = 0;
    for (const mg::TrackPoint& p : r.points) {
      const double d = ge::core::hypot(p.x - k.x, p.z - k.z);
      if (d < best) {
        best = d;
        s = p.s;
      }
    }
    ge::lab::draw_line(pic, X(s), half - 14, X(s), half - 2, {0, 0, 0}, 2);
    ge::lab::draw_text(pic, X(s) + 3, half - 16, k.id, {0, 0, 0});
  }
  ge::lab::draw_text(pic, w - 200, 6, f0(top - low) + " M DOWN " + f0(r.length) + " M",
                     {80, 80, 80});
  return pic;
}

void run(ge::lab::Lab& lab) {
  const int px = static_cast<int>(lab.arg_num("size"));
  for (const int version : ge::mountain::labs::map_versions(lab)) {
    for (const double seed : lab.arg_nums("seeds")) {
      const auto built = ge::mountain::labs::build_map(lab, seed, version);
      if (!built) continue;
      const mg::Level& map = *built;
      const std::string id = ge::mountain::labs::map_id(seed, version);
      if (!map.resort) {
        lab.fail(id + ": the map is no ski area, so it carries no routes",
                 "next: ask for --version=9 (the straight routes) or 10 (the wild ones).");
        continue;
      }
      size_t errors = 0;
      for (const an::Finding& f : an::analyze_level(map).findings) {
        if (f.severity != an::Severity::error) continue;
        errors++;
        lab.fail(id + ": " + f.rule + " " + f.message,
                 "Read the routes in this lab's pictures and fix the search in "
                 "plugins/mountain/mapgen/ski_routes.cpp or the jumps in route_features.cpp "
                 "(skill: mountain).");
      }
      const auto& routes = map.resort->routes;
      std::vector<std::vector<std::string>> rows;
      double longest = 0;
      size_t jumps = 0;
      size_t drops = 0;
      for (const mg::SkiRoute& r : routes) {
        size_t on = 0;
        for (const mg::Kicker& k : map.kickers) on += k.route == r.id ? 1 : 0;
        jumps += on;
        drops += r.drops.size();
        longest = std::max(longest, r.length);
        rows.push_back({r.id, r.from, r.into.run + " @" + f0(r.into.s), f0(r.length),
                        f0(r.points.front().y - r.points.back().y), f0(r.steepest * 100),
                        std::to_string(r.drops.size()), std::to_string(on), f0(r.woods),
                        f0(r.edge)});
      }
      lab.metric(id + ".routes", double(routes.size()), "count");
      lab.metric(id + ".longest", longest, "m");
      lab.metric(id + ".drops", double(drops), "count");
      lab.metric(id + ".jumps", double(jumps), "count");
      lab.metric(id + ".errors", double(errors), "count");
      lab.table({id + ": the ski routes",
                 {"route", "from", "onto", "m", "fall m", "steepest %", "drops", "jumps", "woods m",
                  "edge m"},
                 rows});
      lab.table({id + ": the map",
                 {"course", "attempt", "routes", "digest"},
                 {{map.resort->course, std::to_string(map.attempt), std::to_string(routes.size()),
                   mg::level_digest(map)}}});
      const auto inputs = ge::mountain::labs::map_inputs(lab, map);
      const std::string name = "SEED " + std::to_string(static_cast<long long>(seed));
      const bool wild =
          std::any_of(routes.begin(), routes.end(), [](const mg::SkiRoute& r) { return r.wild; });
      lab.shot({id + "-routes", name + " SKI ROUTES",
                "The ski area from above: the face shaded by height, the packed snow blue, the "
                "woods dark green, every run in its grade's colour and the lane grey, the lifts "
                "thin dark lines, every ski route thick orange with its id" +
                    std::string(wild ? ", its drops magenta rings and its jumps black bars." : "."),
                "each route leaving a chair's or a gondola's top, off the pistes, never under a "
                "lift, coming down onto a run",
                inputs},
               draw_area(map, px));
      for (const mg::SkiRoute& r : routes) {
        lab.shot({id + "-" + r.id, name + " " + r.id + " CLOSE UP",
                  "Route " + r.id +
                      " from above: the face as stamped, the woods dark green, the "
                      "line orange, its drops magenta rings and its jumps black bars.",
                  wild ? "a line that turns through the woods and along the edges in a lane "
                         "felled a few metres wide, never through a trunk"
                       : "a line through open snow, clear of every trunk",
                  inputs},
                 draw_close(map, r, px));
        lab.shot({id + "-" + r.id + "-side", name + " " + r.id + " FROM THE SIDE",
                  "Route " + r.id +
                      " down its arc: its height (black, top), its pitch over a "
                      "100 m window (orange, foot) against a black's ceiling (grey) and a "
                      "route's (red); drops magenta ticks, jumps black ticks.",
                  "the orange pitch over the grey line somewhere and never over the red", inputs},
                 draw_profile(map, r, px, std::max(240, px / 3)));
      }
    }
  }
}

const ge::lab::LabSpec kSpec{
    "routes",
    ge::lab::LabKind::picture,
    "plugin:mountain",
    "plugins/mountain/mapgen",
    "mountain",
    "Where do a ski area's orange routes go off its tops, how steep, and what is laid down a wild "
    "one?",
    {
        {"seeds", "38", "seeds, comma-separated"},
        {"size", "1024", "the pictures' width, px"},
        {"version", "10",
         "generator versions, comma-separated: 9 the straight routes, 10 the wild ones"},
        ge::mountain::labs::kRegionFlag,
    },
    "--seeds=38 --size=256 --version=10",
    run,
};
GE_LAB(kSpec);

}  // namespace
