// LAB `se-nav`: where can a player go on a map, by the player move's own proof? The navigation
// graph (plugins/shooter/nav.hpp) is built on the nav course (stairs, a crate, a pit, two ramps:
// games/shooters_edge/sim/nav_course.hpp) and on the test arena (its door left out and tagged),
// and pictured from above to scale: the floor shaded by height off the map's own point hull, every
// node, every link coloured by its kind, and the quickest routes drawn as the proofs actually
// moved a player along them; each route again from the side. Tables of the graph's counts and the
// routes; the build's time is measured.
//
// ge:summary Lab: the nav graph proven by the move, on the nav course and the arena, from above
// ge:keywords lab se-nav nav nav-graph links walk drop jump run route reach proof top-view
// ge:keywords side-view nav-course arena door
// ge:skill shooter bots labs
// ge:test tests/shooter_nav_test.cpp
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "shooter/nav.hpp"
#include "shooters_edge/sim/arena.hpp"
#include "shooters_edge/sim/nav_course.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace se = shooters_edge::sim;
using ge::core::num::to_fixed;
using In = std::vector<std::pair<std::string, std::string>>;

constexpr int kMargin = 24;  // pixels round the map, for the legend's row

std::string hex64(uint64_t v) {
  char buf[17];
  std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(v));
  return buf;
}

std::string key_of(std::string s) {
  for (char& c : s)
    if (c == ' ' || c == '_') c = '-';
  return s;
}

const gl::Rgba kKindColour[3] = {
    {150, 150, 140, 255},  // walk
    {60, 110, 200, 255},   // drop
    {210, 40, 40, 255},    // jump
};
const gl::Rgba kMoverColour{170, 60, 200, 255};
const gl::Rgba kRouteColour{20, 20, 20, 255};

struct Map {
  std::string name;
  sh::MoveWorld world;
  std::vector<sh::NavMover> movers;
  struct Route {
    std::string name;
    sh::Vec3f from, to;
  };
  std::vector<Route> routes;
};

// The picture's pixel for a map point (x east, y north up the picture), one pixel a unit.
struct View {
  sh::Vec3f lo, hi;
  double px(double x) const { return kMargin + (x - lo.x); }
  double py(double y) const { return kMargin + (hi.y - y); }
};

// The floor from above: a point traced down the map's own point hull per pixel, shaded by the
// height it stops at (solid at the top: a wall, dark).
gl::Picture floor_picture(const sh::MoveWorld& world, const View& v) {
  const int w = static_cast<int>(v.hi.x - v.lo.x) + 2 * kMargin;
  const int h = static_cast<int>(v.hi.y - v.lo.y) + 2 * kMargin;
  gl::Picture p = gl::create_picture(w, h, {245, 245, 240, 255});
  const sh::Hull& hull = world.solids[0].point_hull;
  const float span = v.hi.z - v.lo.z;
  for (int py = kMargin; py < h - kMargin; py++)
    for (int px = kMargin; px < w - kMargin; px++) {
      const float x = v.lo.x + static_cast<float>(px - kMargin) + 0.5f;
      const float y = v.hi.y - static_cast<float>(py - kMargin) - 0.5f;
      const sh::HullTrace tr = sh::trace_hull(hull, {x, y, v.hi.z - 1}, {x, y, v.lo.z});
      if (tr.startsolid) {
        gl::set_pixel(p, px, py, {70, 74, 72, 255});
        continue;
      }
      const float t = (tr.endpos.z - v.lo.z) / span;  // 0 low .. 1 high
      const auto g = static_cast<uint8_t>(150 + 95 * t);
      gl::set_pixel(p, px, py,
                    {static_cast<uint8_t>(g - 20), g, static_cast<uint8_t>(g - 35), 255});
    }
  return p;
}

void legend(gl::Picture& p) {
  int x = kMargin;
  const char* names[4] = {"WALK", "DROP", "JUMP", "THROUGH A MOVER"};
  for (int k = 0; k < 4; k++) {
    const gl::Rgba c = k < 3 ? kKindColour[k] : kMoverColour;
    gl::fill_rect(p, x, 8, 14, 6, c);
    gl::draw_text(p, x + 18, 7, names[k], {30, 30, 30, 255});
    x += 18 + gl::text_width(names[k]) + 18;
  }
  gl::draw_line(p, x, 11, x + 14, 11, kRouteColour, 3);
  gl::draw_text(p, x + 18, 7, "ROUTE AS PROVEN", {30, 30, 30, 255});
}

const sh::NavLink* link_between(const sh::NavGraph& g, int32_t a, int32_t b) {
  const sh::NavNode& n = g.nodes[static_cast<size_t>(a)];
  for (int32_t k = 0; k < n.links; k++)
    if (g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)].to == b)
      return &g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
  return nullptr;
}

void run_map(gl::Lab& lab, const Map& m, const sh::NavSettings& set, const In& in_base) {
  In in = in_base;
  in.push_back({"map", m.name});
  sh::NavGraph g;
  const auto t0 = std::chrono::steady_clock::now();
  sh::build_nav(m.world, m.movers, sh::MoveSettings{}, set, g);
  const double build_ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  // What a player can get to from the first route's start.
  ge::ai::Search reach;
  const int32_t reached =
      m.routes.empty() ? 0 : sh::nav_reach(g, sh::nav_nearest(g, m.routes[0].from), reach);
  const int32_t unreached = static_cast<int32_t>(g.nodes.size()) - reached;
  const std::string k = "nav." + m.name;

  lab.metric(k + ".nodes", static_cast<double>(g.nodes.size()), "count");
  lab.metric(k + ".links", static_cast<double>(g.links.size()), "count");
  for (int q = 0; q < 3; q++)
    lab.metric(k + ".links-" + sh::nav_kind_name(static_cast<sh::NavKind>(q)),
               static_cast<double>(g.counts.by_kind[q]), "count");
  int64_t through_mover = 0;
  for (const sh::NavLink& l : g.links)
    if (l.mover >= 0) through_mover++;
  lab.metric(k + ".links-through-mover", static_cast<double>(through_mover), "count");
  lab.metric(k + ".proofs", static_cast<double>(g.counts.proofs), "count");
  lab.metric(k + ".refused", static_cast<double>(g.counts.refused), "count");
  lab.metric(k + ".move-commands", static_cast<double>(g.counts.commands), "count");
  lab.metric(k + ".unreached", unreached, "count");
  lab.metric(k + ".build-ms", build_ms, "ms", gl::Det::measured);

  gl::LabTable counts{"the " + m.name + "'s graph", {"count", "value"}, {}};
  counts.rows = {{"grid columns", std::to_string(g.counts.columns)},
                 {"nodes", std::to_string(g.nodes.size())},
                 {"links", std::to_string(g.links.size())},
                 {"walk", std::to_string(g.counts.by_kind[0])},
                 {"drop", std::to_string(g.counts.by_kind[1])},
                 {"jump", std::to_string(g.counts.by_kind[2])},
                 {"through a mover", std::to_string(through_mover)},
                 {"proofs run", std::to_string(g.counts.proofs)},
                 {"candidates refused", std::to_string(g.counts.refused)},
                 {"jumps without room to run up", std::to_string(g.counts.no_run_up)},
                 {"player moves run", std::to_string(g.counts.commands)},
                 {"nodes no way reaches from the first start", std::to_string(unreached)},
                 {"nav hash", hex64(sh::nav_hash(g))}};
  lab.table(counts);

  // The picture from above.
  const View v{g.lo, g.hi};
  gl::Picture p = floor_picture(m.world, v);
  for (size_t a = 0; a < g.nodes.size(); a++) {
    const sh::NavNode& na = g.nodes[a];
    for (int32_t q = 0; q < na.links; q++) {
      const sh::NavLink& l = g.links[static_cast<size_t>(na.first_link) + static_cast<size_t>(q)];
      const sh::NavNode& nb = g.nodes[static_cast<size_t>(l.to)];
      if (l.kind == sh::NavKind::walk && l.mover < 0 && static_cast<size_t>(l.to) < a)
        continue;  // a walk both ways is drawn once
      const gl::Rgba c = l.mover >= 0 ? kMoverColour : kKindColour[static_cast<int>(l.kind)];
      gl::draw_line(p, v.px(na.origin.x), v.py(na.origin.y), v.px(nb.origin.x), v.py(nb.origin.y),
                    c, l.kind == sh::NavKind::walk ? 1 : 1.5);
    }
  }
  for (size_t a = 0; a < g.nodes.size(); a++) {
    const sh::NavNode& n = g.nodes[a];
    const bool got = reach.reached(static_cast<int32_t>(a));
    const gl::Rgba c = got ? gl::Rgba{40, 90, 60, 255} : gl::Rgba{200, 30, 120, 255};
    gl::fill_disc(p, v.px(n.origin.x), v.py(n.origin.y), got ? 1.6 : 3, c);
  }

  gl::LabTable routes{
      "the quickest routes on the " + m.name,
      {"route", "nodes", "walk", "drop", "jump", "through mover", "time s", "expanded"},
      {}};
  ge::ai::Search search;
  std::vector<gl::Trace> side;
  for (const Map::Route& rt : m.routes) {
    const sh::NavRoute r =
        sh::nav_route(g, sh::nav_nearest(g, rt.from), sh::nav_nearest(g, rt.to), search);
    int by[4] = {};
    gl::Trace t{rt.name, {}, {}};
    std::vector<sh::Vec3f> moved;
    for (size_t i = 0; i + 1 < r.nodes.size(); i++) {
      const sh::NavLink* l = link_between(g, r.nodes[i], r.nodes[i + 1]);
      if (!l) continue;
      by[static_cast<int>(l->kind)]++;
      if (l->mover >= 0) by[3]++;
      moved.clear();
      sh::prove_link(m.world, sh::MoveSettings{}, g.nodes[static_cast<size_t>(r.nodes[i])].origin,
                     g.nodes[static_cast<size_t>(l->to)].origin, l->kind, l->speed, set, &moved);
      for (size_t j = 0; j + 1 < moved.size(); j++)
        gl::draw_line(p, v.px(moved[j].x), v.py(moved[j].y), v.px(moved[j + 1].x),
                      v.py(moved[j + 1].y), kRouteColour, 3);
      for (const sh::Vec3f& q : moved) {
        t.x.push_back(q.x);
        t.y.push_back(q.z);
      }
    }
    side.push_back(t);
    routes.rows.push_back({rt.name, std::to_string(r.nodes.size()), std::to_string(by[0]),
                           std::to_string(by[1]), std::to_string(by[2]), std::to_string(by[3]),
                           r.nodes.empty() ? "no way" : to_fixed(r.time_s, 3),
                           std::to_string(r.expanded)});
    lab.metric(k + ".route." + key_of(rt.name) + ".time", r.nodes.empty() ? -1 : r.time_s, "s");
    if (!r.nodes.empty()) {
      const sh::Vec3f a = g.nodes[static_cast<size_t>(r.nodes.front())].origin;
      const sh::Vec3f b = g.nodes[static_cast<size_t>(r.nodes.back())].origin;
      gl::draw_ring(p, v.px(a.x), v.py(a.y), 6, kRouteColour);
      gl::fill_disc(p, v.px(b.x), v.py(b.y), 5, kRouteColour);
    }
  }
  lab.table(routes);
  legend(p);

  const bool course = m.name == "course";
  lab.shot(
      {m.name + "-top", course ? "THE NAV COURSE FROM ABOVE" : "THE ARENA FROM ABOVE",
       "To scale, a pixel a unit, x east, y north up. The floor shaded by height off the "
       "map's own point hull (walls dark); every node a dot (green: reached from the first "
       "route's start, large magenta: no way there); every link proven by the player move, "
       "coloured by kind; the "
       "quickest routes drawn as the proofs moved a player (ring: start, dot: goal). " +
           std::to_string(g.nodes.size()) + " nodes, " + std::to_string(g.links.size()) +
           " links, " + std::to_string(unreached) + " nodes out of reach.",
       course ? "stairs at the platform's south end walked both ways, its long edge dropped "
                "off only (blue); the crate reached only by red jumps; the pit crossed "
                "only by red jumps, dropped into anywhere and climbed out up "
                "its north steps; the ramp walked up to the east ledge; the high ledge past "
                "the steep ramp large magenta, reached by no link"
              : "a dense grey walk mesh in both rooms; purple links only in the doorway (the "
                "door left out of the proofs and tagged); the plat's square tagged too; the "
                "ledge 104 high in the south-east large magenta: no link climbs it (riding the "
                "plat is a later row's)",
       in},
      p);

  gl::LineChart c;
  c.spikes = false;
  c.frame = {course ? "THE NAV COURSE'S ROUTES FROM THE SIDE" : "THE ARENA'S ROUTES FROM THE SIDE",
             "each route's origins as the move ran it", "x (units)", "z (units)"};
  c.traces = side;
  const std::string svg = gl::to_svg(gl::line_chart(c).plot);
  lab.attach(m.name + "-side.svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({m.name + "-side", c.frame.title,
            "The routes from above, seen from the south: the player's origin (24 above the feet) "
            "at every command the proofs ran.",
            course ? "the platform route steps down the stairs in 16s, arcs over the pit at about "
                     "z 24..50 and climbs the ramp's straight slope to 88; the pit route stays at "
                     "-72 and climbs out in steps of 16"
                   : "both routes flat at z = 24 through the doorway",
            in},
           gl::to_picture(gl::line_chart(c).plot));

  if (g.nodes.empty())
    lab.fail(
        "se-nav: the " + m.name + " has no nodes",
        "next: run tests/shooter_nav_test.cpp and read column_floors in plugins/shooter/nav.cpp");
}

void run(gl::Lab& lab) {
  sh::NavSettings set;
  set.spacing = static_cast<float>(lab.arg_num("spacing"));
  set.reach = static_cast<float>(lab.arg_num("reach"));
  if (set.spacing < 8 || set.reach < set.spacing) {
    lab.fail("se-nav: --spacing must be at least 8 and --reach at least --spacing",
             "next: make lab LAB=se-nav ARGS=--help");
    return;
  }
  const In in = {{"spacing", lab.arg("spacing")}, {"reach", lab.arg("reach")}};
  const std::string which = lab.arg("map");

  if (which == "course" || which == "both") {
    se::NavCourse c;
    se::build_nav_course(c);
    Map m;
    m.name = "course";
    sh::map_world(c.world.model(), m.world);
    m.routes = {{"platform to east ledge", c.platform, c.east_ledge},
                {"pit to crate", c.pit_floor, c.crate_top},
                {"east floor to platform", c.east_floor, c.platform}};
    run_map(lab, m, set, in);
  }
  if (which == "arena" || which == "both") {
    se::Arena a;
    se::build_arena(a);
    Map m;
    m.name = "arena";
    sh::map_world(a.world.model(), m.world);
    // The door's and the plat's entity numbers as the arena spawns them are the program's; the
    // lab tags them by model index, which is what it draws.
    m.movers = {{se::kDoorModel, a.door.mins, a.door.maxs},
                {se::kPlatModel, a.plat.mins, a.plat.maxs}};
    m.routes = {
        {"west spawn to east room", a.spawn[0], {400, 64, 24}},
        {"east room to health", {400, -128, 24}, {a.health_origin.x, a.health_origin.y, 24}}};
    run_map(lab, m, set, in);
  }
}

const gl::LabSpec kSpec{
    "se-nav",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "plugins/shooter/nav.hpp (build_nav, prove_link, nav_route) on "
    "games/shooters_edge/sim/nav_course.hpp and arena.hpp",
    "shooter",
    "Where can a player go on a map, by the player move's own proof: which floors are nodes, "
    "which links walk, drop or need a jump on the move, what a mover closes, and the quickest "
    "routes?",
    {
        {"map", "both", "course, arena or both"},
        {"spacing", "32", "the grid's columns, units apart"},
        {"reach", "224", "the furthest a jump link is tried, units"},
    },
    "--map=course",
    run,
};
GE_LAB(kSpec);

}  // namespace
