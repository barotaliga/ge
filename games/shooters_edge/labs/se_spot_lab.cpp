// LAB `se-spot`: ONE PLACE ON A MAP LOOKED AT EVERY WAY, to work out how a player gets there (and
// where the bots' nav falls short of it: a tread it misses, a door a button opens, a teleporter).
// Given `--at="x y z"` (a player's feet) and `--r` (the box round it, units), the lab draws:
//
// - TOP: the floor from above (the map's point hull traced down every pixel; `--clip-top=<z>` cuts
//   the map away over a height and steps through every roof under it, so a level under another
//   is seen; the WIRE drops the edges over it too), coloured by height
//   with a contour every 16 units, water tinted blue, solid dark; over it the nav's nodes and links
//   by kind, the items by their call ("ya", "mh"), the doors, plats, buttons (a SHOT button ringed)
//   and teleporters as boxes with each teleporter's destination, the spawns, and `--route`;
// - WIRE: the map's face outlines in the box from four corners at 35 degrees, the nav and route
//   laid over in 3D;
// - INSIDE: four eyes round the spot looking at it (and `--eye` when given): the faces shaded and
//   outlined, the doors brown, buttons red, plats purple, teleporters magenta, the nav, items and
//   route drawn into the depth buffer, so walls hide what is behind them;
// - DEPTH: the same eyes' depth buffer, near warm to far cold, a contour every 64 units;
// - FLOORS: the same eyes with every face by its slope: floors a player stands on green (normal z
//   0.7 and over), slopes orange, walls grey; and NORMALS, each face's normal as a colour;
// - CUTS: two upright sections through the spot (along x and along y): solid, empty and water,
//   every floor a standing player finds straight down (green ticks), the nav's nodes within 16 of
//   the cut, the player's box at the spot to scale.
//
// Every view is a CPU raster, so its bytes are the same on every machine. Without `--basedir` the
// map is the fixture halls (plugins/brush), so CI needs no game data; with it and `--map` the
// pictures are of the player's own game and stay private.
//
// ge:summary Lab: one spot on a map from above, wireframes, inside views, depth, slopes, cuts, nav
// ge:keywords lab se-spot spot location photo pictures depth normals floors slope wireframe
// ge:keywords section cut top-view nav-overlay route-overlay scripted-route item-route button
// ge:keywords teleporter door inside-view contact-sheet map-study clip-top cut-away roof
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "brush/write.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "progs/entity_text.hpp"
#include "shooter/hull.hpp"
#include "shooter/nav.hpp"
#include "shooter/world.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/bots/map_nav.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/labs/wire_inside.hpp"
#include "shooters_edge/labs/wire_view.hpp"
#include "shooters_edge/net/classic_team.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace pg = ge::progs;
namespace cl = shooters_edge::client;
namespace sv = shooters_edge::server;
namespace bt = shooters_edge::bots;
namespace cn = shooters_edge::net;
namespace la = shooters_edge::labs;
using ge::core::num::to_fixed;

constexpr gl::Rgba kInk{30, 34, 40, 255};

// A brush entity or a point entity the pictures mark.
enum class Mark : uint8_t { door, plat, button, teleport, wall, trigger };
struct Thing {
  Mark kind = Mark::door;
  int ent = 0;
  size_t model = 0;   // the brush model, 0: none
  sh::Vec3f lo, hi;   // its box at rest
  bool shot = false;  // a button with health: shot, not touched
  bool has_dest = false;
  sh::Vec3f dest;  // a teleporter's destination
  std::string label;
};
struct Item {
  sh::Vec3f at;
  std::string call;
};

gl::Rgba kind_colour(sh::NavKind k) {
  switch (k) {
    case sh::NavKind::walk:
      return {120, 124, 130, 255};
    case sh::NavKind::drop:
      return {60, 120, 220, 255};
    case sh::NavKind::jump:
      return {240, 140, 30, 255};
    case sh::NavKind::ride:
      return {150, 80, 200, 255};
    case sh::NavKind::swim:
      return {40, 180, 200, 255};
    case sh::NavKind::rocket:
      return {220, 40, 40, 255};
    case sh::NavKind::grenade:
      return {40, 160, 60, 255};
    case sh::NavKind::script:
      return {230, 0, 140, 255};
  }
  return kInk;
}

gl::Rgba mark_colour(Mark m) {
  switch (m) {
    case Mark::door:
      return {150, 100, 50, 255};
    case Mark::plat:
      return {150, 80, 200, 255};
    case Mark::button:
      return {220, 40, 40, 255};
    case Mark::teleport:
      return {220, 50, 200, 255};
    case Mark::wall:
      return {120, 120, 90, 255};
    case Mark::trigger:
      return {200, 200, 60, 255};
  }
  return kInk;
}

const char* mark_name(Mark m) {
  switch (m) {
    case Mark::door:
      return "door";
    case Mark::plat:
      return "plat";
    case Mark::button:
      return "button";
    case Mark::teleport:
      return "teleport";
    case Mark::wall:
      return "wall";
    case Mark::trigger:
      return "trigger";
  }
  return "?";
}

// Height to colour: low blue-green, through green and yellow, high red-brown.
gl::Rgba height_colour(double t) {
  t = std::clamp(t, 0.0, 1.0);
  struct Stop {
    double v;
    gl::Rgba c;
  };
  static constexpr Stop kStops[] = {{0, {70, 110, 160, 255}},
                                    {0.35, {110, 175, 120, 255}},
                                    {0.65, {225, 215, 120, 255}},
                                    {1, {205, 110, 80, 255}}};
  for (size_t i = 1; i < std::size(kStops); i++)
    if (t <= kStops[i].v) {
      const double u = (t - kStops[i - 1].v) / (kStops[i].v - kStops[i - 1].v);
      auto mix = [&](uint8_t a, uint8_t b) { return static_cast<uint8_t>(a + (b - a) * u); };
      return {mix(kStops[i - 1].c.r, kStops[i].c.r), mix(kStops[i - 1].c.g, kStops[i].c.g),
              mix(kStops[i - 1].c.b, kStops[i].c.b), 255};
    }
  return kStops[std::size(kStops) - 1].c;
}

bool vec_of(const std::string& s, sh::Vec3f& out) {
  std::istringstream in(s);
  return static_cast<bool>(in >> out.x >> out.y >> out.z);
}

// Every floor a standing player finds straight down at (x, y) between `top` and `bottom` (feet),
// top first.
void feet_floors(const sh::MoveWorld& w, float x, float y, float top, float bottom,
                 std::vector<float>& out) {
  out.clear();
  float z = top + 24;
  while (z > bottom + 24) {
    if (!sh::box_free(w, {x, y, z})) {
      z -= 4;
      continue;
    }
    const sh::HullTrace tr = sh::player_trace(w, {x, y, z}, {x, y, bottom + 24});
    if (tr.startsolid || tr.fraction == 1) return;
    if (tr.plane.normal.z >= 0.7f) out.push_back(tr.endpos.z - 24);
    z = tr.endpos.z - 57;
  }
}

struct Scene {
  std::string name;
  std::unique_ptr<cl::LoadedMap> map;
  bt::MapNav nav;
  std::vector<Thing> things;
  std::vector<Item> items;
  std::vector<sh::Vec3f> spawns;
  sh::Vec3f at;                // the spot (feet)
  float r = 384;               // the box's half side
  // `--clip-top`: the top view cut away over this height and every roof under it stepped through
  // (nav_top.hpp `floor_below`), a level under another seen; NaN: from the spot's eye height.
  float clip_top = std::numeric_limits<float>::quiet_NaN();
  std::vector<int32_t> route;  // --route's nodes
  bool swim = false;           // the swim nodes and links drawn too
};

bool inside_box(const Scene& s, sh::Vec3f p, float pad = 0) {
  const float r = s.r + pad;
  return p.x >= s.at.x - r && p.x <= s.at.x + r && p.y >= s.at.y - r && p.y <= s.at.y + r &&
         p.z >= s.at.z - r && p.z <= s.at.z + r;
}

void read_things(Scene& s, std::string& error) {
  std::vector<pg::EntityBlock> blocks;
  if (!pg::parse_entity_text(s.map->spec.entities.c_str(), blocks, error)) return;
  const ge::brush::Map& bsp = s.map->bsp.map;
  std::vector<std::pair<std::string, sh::Vec3f>> dests;
  for (const pg::EntityBlock& b : blocks) {
    const char* cn = pg::block_value(b, "classname");
    const char* tn = pg::block_value(b, "targetname");
    const char* org = pg::block_value(b, "origin");
    sh::Vec3f o;
    if (cn && tn && org && std::string(cn) == "info_teleport_destination" && vec_of(org, o))
      dests.push_back({tn, o});
  }
  int ent = 0;
  for (const pg::EntityBlock& b : blocks) {
    ent++;
    const char* cn = pg::block_value(b, "classname");
    if (!cn) continue;
    const std::string c = cn;
    const char* org = pg::block_value(b, "origin");
    sh::Vec3f o;
    const char* flags = pg::block_value(b, "spawnflags");
    bt::ItemSpot it;
    if (org && vec_of(org, o) && bt::item_of(cn, flags ? std::atoi(flags) : 0, it)) {
      s.items.push_back({o, bt::item_call(it)});
      continue;
    }
    if (org && vec_of(org, o) && c.rfind("info_player", 0) == 0) {
      s.spawns.push_back(o);
      continue;
    }
    const char* model = pg::block_value(b, "model");
    if (!model || model[0] != '*') continue;
    Thing t;
    t.ent = ent - 1;
    t.model = static_cast<size_t>(std::atoi(model + 1));
    if (t.model == 0 || t.model >= bsp.models.size()) continue;
    if (c == "func_door" || c == "func_door_secret")
      t.kind = Mark::door;
    else if (c == "func_plat" || c == "func_train")
      t.kind = Mark::plat;
    else if (c == "func_button")
      t.kind = Mark::button;
    else if (c == "trigger_teleport")
      t.kind = Mark::teleport;
    else if (c == "func_wall" || c == "func_illusionary" || c == "func_episodegate")
      t.kind = Mark::wall;
    else if (c.rfind("trigger_", 0) == 0)
      t.kind = Mark::trigger;
    else
      continue;
    const ge::brush::Model& m = bsp.models[t.model];
    t.lo = {m.mins[0], m.mins[1], m.mins[2]};
    t.hi = {m.maxs[0], m.maxs[1], m.maxs[2]};
    const char* health = pg::block_value(b, "health");
    t.shot = t.kind == Mark::button && health && std::atof(health) > 0;
    const char* target = pg::block_value(b, "target");
    const char* tname = pg::block_value(b, "targetname");
    if (t.kind == Mark::teleport && target)
      for (const auto& d : dests)
        if (d.first == target) t.has_dest = true, t.dest = d.second;
    t.label = std::string(mark_name(t.kind)) + " " + std::to_string(t.ent);
    if (t.shot) t.label += " SHOT";
    if (tname) t.label += " (" + std::string(tname) + ")";
    if (target && t.kind != Mark::teleport) t.label += " > " + std::string(target);
    s.things.push_back(t);
  }
}

// The box's twelve edges.
template <class F>
void box_edges(sh::Vec3f lo, sh::Vec3f hi, F&& f) {
  const sh::Vec3f c[8] = {{lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z},
                          {lo.x, hi.y, lo.z}, {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z},
                          {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}};
  static constexpr int kE[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                    {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
  for (const auto& e : kE) f(c[e[0]], c[e[1]]);
}

// The nav's links with both ends in the box (feet at the nodes).
template <class F>
void each_link(const Scene& s, F&& f) {
  const sh::NavGraph& g = s.nav.graph;
  for (size_t a = 0; a < g.nodes.size(); a++) {
    const sh::NavNode& n = g.nodes[a];
    const sh::Vec3f fa{n.origin.x, n.origin.y, n.origin.z - 24};
    if (!inside_box(s, fa)) continue;
    for (int32_t k = 0; k < n.links; k++) {
      const sh::NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
      const sh::Vec3f o = g.nodes[static_cast<size_t>(l.to)].origin;
      const sh::Vec3f fb{o.x, o.y, o.z - 24};
      if (!inside_box(s, fb)) continue;
      f(fa, fb, l, g.nodes[static_cast<size_t>(l.to)].swim || n.swim);
    }
  }
}

std::vector<sh::Vec3f> route_feet(const Scene& s) {
  std::vector<sh::Vec3f> out;
  for (const int32_t n : s.route) {
    const sh::Vec3f o = s.nav.graph.nodes[static_cast<size_t>(n)].origin;
    out.push_back({o.x, o.y, o.z - 24});
  }
  return out;
}

// --- TOP -------------------------------------------------------------------------------------

gl::Picture top_picture(const Scene& s, int px_size) {
  const double scale = px_size / (2.0 * s.r);
  const int m = 30;
  gl::Picture p = gl::create_picture(px_size + 2 * m, px_size + 2 * m + 16, {250, 250, 246, 255});
  const sh::MoveWorld& w = s.nav.world;
  const sh::Hull& hull = w.solids[0].point_hull;
  auto wx = [&](double px) { return s.at.x - s.r + (px - m + 0.5) / scale; };
  auto wy = [&](double py) { return s.at.y + s.r - (py - m + 0.5) / scale; };
  auto sx = [&](double x) { return m + (x - (s.at.x - s.r)) * scale; };
  auto sy = [&](double y) { return m + ((s.at.y + s.r) - y) * scale; };
  const float ztop = s.at.z + s.r, zlo = s.at.z - s.r;
  std::vector<float> hz(static_cast<size_t>(px_size) * static_cast<size_t>(px_size), 1e9f);
  std::vector<uint8_t> wet(hz.size(), 0);
  for (int y = 0; y < px_size; y++)
    for (int x = 0; x < px_size; x++) {
      const float X = static_cast<float>(wx(x + m)), Y = static_cast<float>(wy(y + m));
      const size_t i =
          static_cast<size_t>(y) * static_cast<size_t>(px_size) + static_cast<size_t>(x);
      // From the spot's eye height (a room's floor under its ceiling), or from the first open
      // space above it within the box, down to the box's floor; solid all the way: dark.
      float from = std::min(ztop, s.at.z + 64);
      if (!std::isnan(s.clip_top)) {
        // Cut away over the clip: down from it through any roof to the first open space.
        from = std::min(ztop, s.clip_top);
        while (from > zlo && sh::point_contents(hull, {X, Y, from}) == sh::kContentsSolid)
          from -= 4;
      } else {
        while (from < ztop && sh::point_contents(hull, {X, Y, from}) == sh::kContentsSolid)
          from += 8;
        if (sh::point_contents(hull, {X, Y, from}) == sh::kContentsSolid) {
          from = std::min(ztop, s.at.z + 64);
          while (from > zlo && sh::point_contents(hull, {X, Y, from}) == sh::kContentsSolid)
            from -= 8;
        }
      }
      const sh::HullTrace tr = sh::trace_hull(hull, {X, Y, from}, {X, Y, zlo});
      if (tr.startsolid || from <= zlo) {
        gl::set_pixel(p, x + m, y + m, {70, 74, 72, 255});
        continue;
      }
      hz[i] = tr.endpos.z;
      wet[i] = sh::point_contents(hull, {X, Y, tr.endpos.z + 1}) <= sh::kContentsWater ||
               sh::point_contents(hull, {X, Y, std::min(ztop, s.at.z + 8)}) <= sh::kContentsWater;
      gl::Rgba c = height_colour((tr.endpos.z - zlo) / (ztop - zlo));
      if (tr.fraction == 1) c = {225, 225, 230, 255};
      if (wet[i])
        c = {static_cast<uint8_t>(c.r / 2 + 40), static_cast<uint8_t>(c.g / 2 + 70),
             static_cast<uint8_t>(c.b / 2 + 120), 255};
      gl::set_pixel(p, x + m, y + m, c);
    }
  // Contours every 16 units.
  for (int y = 1; y < px_size; y++)
    for (int x = 1; x < px_size; x++) {
      const size_t i =
          static_cast<size_t>(y) * static_cast<size_t>(px_size) + static_cast<size_t>(x);
      const float a = hz[i], b = hz[i - 1], c = hz[i - static_cast<size_t>(px_size)];
      if (a > 1e8f) continue;
      auto band = [](float z) { return static_cast<int>(std::floor(z / 16)); };
      if ((b < 1e8f && band(a) != band(b)) || (c < 1e8f && band(a) != band(c)))
        gl::set_pixel(p, x + m, y + m, {40, 50, 45, 255});
    }
  // Things: boxes, teleporter arrows.
  for (const Thing& t : s.things) {
    if (t.hi.x < s.at.x - s.r || t.lo.x > s.at.x + s.r || t.hi.y < s.at.y - s.r ||
        t.lo.y > s.at.y + s.r || t.hi.z < zlo || t.lo.z > ztop)
      continue;
    const gl::Rgba c = mark_colour(t.kind);
    const double x0 = sx(t.lo.x), x1 = sx(t.hi.x), y0 = sy(t.hi.y), y1 = sy(t.lo.y);
    gl::draw_line(p, x0, y0, x1, y0, c, 2);
    gl::draw_line(p, x1, y0, x1, y1, c, 2);
    gl::draw_line(p, x1, y1, x0, y1, c, 2);
    gl::draw_line(p, x0, y1, x0, y0, c, 2);
    if (t.shot) gl::draw_ring(p, (x0 + x1) / 2, (y0 + y1) / 2, 9, c);
    gl::draw_text(p, x0, y1 + 2, t.label + " z" + to_fixed(t.lo.z, 0) + ".." + to_fixed(t.hi.z, 0),
                  c);
    if (t.has_dest) {
      const double dx = sx(t.dest.x), dy = sy(t.dest.y);
      const bool in = dx >= m && dx < m + px_size && dy >= m && dy < m + px_size;
      if (in) {
        gl::draw_line(p, (x0 + x1) / 2, (y0 + y1) / 2, dx, dy, c, 1);
        gl::fill_disc(p, dx, dy, 4, c);
      }
      gl::draw_text(
          p, x0, y1 + 11,
          "to " + to_fixed(t.dest.x, 0) + " " + to_fixed(t.dest.y, 0) + " " + to_fixed(t.dest.z, 0),
          c);
    }
  }
  // The nav: links by kind, then the nodes.
  each_link(s, [&](sh::Vec3f a, sh::Vec3f b, const sh::NavLink& l, bool swim) {
    if (swim && !s.swim) return;
    gl::Rgba c = kind_colour(l.kind);
    if (swim) c = {175, 215, 225, 255};
    gl::draw_line(p, sx(a.x), sy(a.y), sx(b.x), sy(b.y), c, 1);
  });
  for (const sh::NavNode& n : s.nav.graph.nodes) {
    const sh::Vec3f f{n.origin.x, n.origin.y, n.origin.z - 24};
    if (!inside_box(s, f) || (n.swim && !s.swim)) continue;
    gl::fill_disc(p, sx(f.x), sy(f.y), n.swim ? 1.5 : 2.5,
                  n.swim ? gl::Rgba{40, 180, 200, 255} : kInk);
  }
  // The scripted ways: each step, numbered.
  for (const sh::NavScript& sc : s.nav.graph.scripts)
    for (size_t k = 0; k < sc.steps.size(); k++) {
      const sh::Vec3f f = sc.steps[k].at - sh::Vec3f{0, 0, 24};
      if (k > 0) {
        const sh::Vec3f e = sc.steps[k - 1].at - sh::Vec3f{0, 0, 24};
        gl::draw_line(p, sx(e.x), sy(e.y), sx(f.x), sy(f.y), kind_colour(sh::NavKind::script), 3);
      }
      gl::fill_disc(p, sx(f.x), sy(f.y), 4, kind_colour(sh::NavKind::script));
      gl::draw_text(p, sx(f.x) - 14, sy(f.y) + 4, (sc.steps[k].hop ? "H" : "W") + std::to_string(k),
                    {230, 0, 140, 255});
    }
  // The route.
  const std::vector<sh::Vec3f> rt = route_feet(s);
  for (size_t k = 1; k < rt.size(); k++)
    gl::draw_line(p, sx(rt[k - 1].x), sy(rt[k - 1].y), sx(rt[k].x), sy(rt[k].y), {20, 20, 20, 255},
                  4);
  for (size_t k = 1; k < rt.size(); k++)
    gl::draw_line(p, sx(rt[k - 1].x), sy(rt[k - 1].y), sx(rt[k].x), sy(rt[k].y),
                  {255, 230, 40, 255}, 2);
  for (size_t k = 0; k < rt.size(); k++)
    gl::draw_text(p, sx(rt[k].x) + 4, sy(rt[k].y) - 10,
                  std::to_string(k) + ":" + to_fixed(rt[k].z, 0), {20, 20, 20, 255});
  // Items and spawns.
  for (const Item& it : s.items) {
    if (!inside_box(s, it.at, 32)) continue;
    gl::fill_disc(p, sx(it.at.x), sy(it.at.y), 6, {255, 255, 255, 255});
    gl::draw_ring(p, sx(it.at.x), sy(it.at.y), 6, kInk);
    gl::draw_text(p, sx(it.at.x) + 8, sy(it.at.y) - 4, it.call + " z" + to_fixed(it.at.z, 0), kInk,
                  2);
  }
  for (const sh::Vec3f& sp : s.spawns)
    if (inside_box(s, sp, 32))
      gl::draw_text(p, sx(sp.x) - 3, sy(sp.y) - 4, "S", {0, 90, 200, 255}, 2);
  // The spot.
  gl::draw_line(p, sx(s.at.x) - 10, sy(s.at.y), sx(s.at.x) + 10, sy(s.at.y), {200, 0, 0, 255}, 2);
  gl::draw_line(p, sx(s.at.x), sy(s.at.y) - 10, sx(s.at.x), sy(s.at.y) + 10, {200, 0, 0, 255}, 2);
  // The scale and the legend.
  const double bar = 64 * scale;
  gl::draw_line(p, m, m + px_size + 8, m + bar, m + px_size + 8, kInk, 2);
  gl::draw_text(p, m + bar + 6, m + px_size + 4, "64 UNITS", kInk);
  int lx = m + static_cast<int>(bar) + 80;
  for (int k = 0; k < sh::kNavKinds; k++) {
    const auto kind = static_cast<sh::NavKind>(k);
    gl::draw_line(p, lx, m + px_size + 8, lx + 16, m + px_size + 8, kind_colour(kind), 3);
    gl::draw_text(p, lx + 20, m + px_size + 4, sh::nav_kind_name(kind), kInk);
    lx += 30 + gl::text_width(sh::nav_kind_name(kind));
  }
  gl::draw_text(p, m, 8,
                "X EAST, Y NORTH UP. FLOOR " + to_fixed(zlo, 0) + " BLUE .. " + to_fixed(ztop, 0) +
                    " RED, CONTOUR EVERY 16",
                kInk);
  return p;
}

// --- WIRE ------------------------------------------------------------------------------------

gl::Picture wire_corner(const Scene& s, std::span<const la::WireEdge> all, double yaw) {
  std::vector<la::WireEdge> edges;
  for (const la::WireEdge& e : all)
    if (inside_box(s, e.a) && inside_box(s, e.b) &&
        (std::isnan(s.clip_top) || (e.a.z <= s.clip_top && e.b.z <= s.clip_top)))
      edges.push_back(e);  // `--clip-top`: the edges over it cut away (a roof, a level above)
  la::WireView v;
  v.yaw = yaw;
  v.elevation = 35;
  v.width = 1100;
  v.height = 820;
  const sh::Vec3f lo{s.at.x - s.r, s.at.y - s.r, s.at.z - s.r},
      hi{s.at.x + s.r, s.at.y + s.r, s.at.z + s.r};
  std::vector<sh::Vec3f> pts;
  box_edges(lo, hi, [&](sh::Vec3f a, sh::Vec3f b) { pts.push_back(a), pts.push_back(b); });
  v.fit(pts);
  gl::Picture p = la::wire_picture(edges, v);
  auto line = [&](sh::Vec3f a, sh::Vec3f b, gl::Rgba c, double t) {
    double ax, ay, af, bx, by, bf;
    v.pixel(a, ax, ay, af);
    v.pixel(b, bx, by, bf);
    gl::draw_line(p, ax, ay, bx, by, c, t);
  };
  for (const Thing& t : s.things)
    if (inside_box(s, (t.lo + t.hi) * 0.5f, 64))
      box_edges(t.lo, t.hi, [&](sh::Vec3f a, sh::Vec3f b) { line(a, b, mark_colour(t.kind), 2); });
  each_link(s, [&](sh::Vec3f a, sh::Vec3f b, const sh::NavLink& l, bool) {
    if (l.kind != sh::NavKind::walk && l.kind != sh::NavKind::swim)
      line(a, b, kind_colour(l.kind), 2);
  });
  const std::vector<sh::Vec3f> rt = route_feet(s);
  for (size_t k = 1; k < rt.size(); k++) line(rt[k - 1], rt[k], {255, 200, 0, 255}, 3);
  for (const Item& it : s.items) {
    if (!inside_box(s, it.at, 32)) continue;
    double x, y, f;
    v.pixel(it.at, x, y, f);
    gl::fill_disc(p, x, y, 5, {255, 255, 255, 255});
    gl::draw_ring(p, x, y, 5, kInk);
    gl::draw_text(p, x + 7, y - 4, it.call, kInk, 2);
  }
  double x, y, f;
  v.pixel(s.at, x, y, f);
  gl::draw_ring(p, x, y, 8, {200, 0, 0, 255});
  return p;
}

// --- INSIDE ----------------------------------------------------------------------------------

// An eye `back` units from the spot toward `yaw` (the eye looks back along it), `up` over it,
// pulled in from a wall.
la::InsideCamera eye_toward(const Scene& s, double yaw, float back, float up) {
  const double a = yaw * ge::core::kPi / 180;
  const sh::Vec3f look{s.at.x, s.at.y, s.at.z + 24};
  const sh::Vec3f want{look.x + back * static_cast<float>(ge::core::libm::cos(a)),
                       look.y + back * static_cast<float>(ge::core::libm::sin(a)), look.z + up};
  const sh::HullTrace tr = sh::trace_hull(s.nav.world.solids[0].point_hull, look, want);
  const float f = tr.fraction < 1 ? std::max(0.05f, tr.fraction - 0.06f) : 1.0f;
  la::InsideCamera c;
  c.eye = {look.x + (want.x - look.x) * f, look.y + (want.y - look.y) * f,
           look.z + (want.z - look.z) * f};
  const double dx = look.x - c.eye.x, dy = look.y - c.eye.y, dz = look.z - c.eye.z;
  c.yaw = ge::core::libm::atan2(dy, dx) * 180 / ge::core::kPi;
  c.pitch = ge::core::libm::atan2(dz, std::sqrt(dx * dx + dy * dy)) * 180 / ge::core::kPi;
  c.fov = 100;
  c.width = 960;
  c.height = 600;
  c.fade = s.r * 4;
  return c;
}

void draw_scene_overlays(const Scene& s, la::InsideView& iv) {
  each_link(s, [&](sh::Vec3f a, sh::Vec3f b, const sh::NavLink& l, bool swim) {
    if (swim && !s.swim) return;
    const sh::Vec3f lift{0, 0, 2};
    iv.line(a + lift, b + lift, kind_colour(l.kind), 1, false);
  });
  for (const sh::NavNode& n : s.nav.graph.nodes) {
    const sh::Vec3f f{n.origin.x, n.origin.y, n.origin.z - 24};
    if (!inside_box(s, f) || n.swim) continue;
    iv.line(f, {f.x, f.y, f.z + 6}, kInk, 3, false);
  }
  for (const Thing& t : s.things)
    if (t.kind == Mark::teleport || t.kind == Mark::trigger)
      box_edges(t.lo, t.hi,
                [&](sh::Vec3f a, sh::Vec3f b) { iv.line(a, b, mark_colour(t.kind), 2, false); });
  const std::vector<sh::Vec3f> rt = route_feet(s);
  for (size_t k = 1; k < rt.size(); k++)
    iv.line(rt[k - 1] + sh::Vec3f{0, 0, 4}, rt[k] + sh::Vec3f{0, 0, 4}, {255, 200, 0, 255}, 4,
            false);
  // The player's box at the spot.
  box_edges({s.at.x - 16, s.at.y - 16, s.at.z}, {s.at.x + 16, s.at.y + 16, s.at.z + 56},
            [&](sh::Vec3f a, sh::Vec3f b) { iv.line(a, b, {200, 0, 0, 255}, 2, false); });
}

void label_items(const Scene& s, la::InsideView& iv) {
  for (const Item& it : s.items) {
    double x, y;
    if (!inside_box(s, it.at, 64) || !iv.pixel(it.at, x, y)) continue;
    gl::fill_disc(iv.picture(), x, y, 5, {255, 255, 255, 255});
    gl::draw_ring(iv.picture(), x, y, 5, kInk);
    gl::draw_text(iv.picture(), x + 7, y - 4, it.call, kInk, 2);
  }
  for (const Thing& t : s.things) {
    double x, y;
    if (t.kind == Mark::wall || !inside_box(s, (t.lo + t.hi) * 0.5f, 64) ||
        !iv.pixel((t.lo + t.hi) * 0.5f, x, y))
      continue;
    gl::draw_text(iv.picture(), x, y, t.label, mark_colour(t.kind));
  }
}

enum class InsideKind : uint8_t { view, depth, floors, normals };

gl::Picture inside_picture(const Scene& s, std::span<const la::WireEdge> edges,
                           const la::InsideCamera& c, InsideKind kind) {
  la::InsideView iv(c);
  const ge::brush::Map& map = s.map->bsp.map;
  using P = la::InsideView::Paint;
  const P paint = kind == InsideKind::floors    ? P::floors
                  : kind == InsideKind::normals ? P::normal
                                                : P::shade;
  iv.faces(map, paint);
  for (const Thing& t : s.things) {
    if (t.kind == Mark::teleport || t.kind == Mark::trigger) continue;  // drawn as outlines
    const gl::Rgba col = mark_colour(t.kind);
    iv.model_faces(map, t.model, paint, kind == InsideKind::view ? &col : nullptr);
  }
  if (kind == InsideKind::depth) {
    // The depth buffer as colour: near warm to far cold, a contour every 64 units.
    const std::vector<double>& d = iv.depth();
    gl::Picture p = gl::create_picture(c.width, c.height, {236, 238, 242, 255});
    // Spread over the depth the picture holds (its 2nd to 98th percentile).
    std::vector<double> seen;
    for (const double v : d)
      if (v < 1e29) seen.push_back(v);
    double near = 0, far = s.r * 3;
    if (!seen.empty()) {
      std::sort(seen.begin(), seen.end());
      near = seen[seen.size() / 50];
      far = std::max(near + 1, seen[seen.size() - 1 - seen.size() / 50]);
    }
    for (int y = 0; y < c.height; y++)
      for (int x = 0; x < c.width; x++) {
        const size_t i =
            static_cast<size_t>(y) * static_cast<size_t>(c.width) + static_cast<size_t>(x);
        if (d[i] > 1e29) continue;
        gl::Rgba col = height_colour(1 - std::clamp((d[i] - near) / (far - near), 0.0, 1.0));
        const bool edge =
            (x > 0 && static_cast<int>(d[i] / 64) != static_cast<int>(d[i - 1] / 64)) ||
            (y > 0 && static_cast<int>(d[i] / 64) !=
                          static_cast<int>(d[i - static_cast<size_t>(c.width)] / 64));
        if (edge) col = {30, 30, 30, 255};
        gl::set_pixel(p, x, y, col);
      }
    gl::draw_text(p, 8, 8,
                  "NEAR " + to_fixed(near, 0) + " RED .. FAR " + to_fixed(far, 0) +
                      " UNITS BLUE, CONTOUR EVERY 64",
                  kInk);
    return p;
  }
  if (kind == InsideKind::view) iv.edges(edges);
  draw_scene_overlays(s, iv);
  label_items(s, iv);
  return iv.picture();
}

// --- CUTS ------------------------------------------------------------------------------------

gl::Picture cut_picture(const Scene& s, bool along_x, int px_size) {
  const double scale = px_size / (2.0 * s.r);
  const int m = 30;
  gl::Picture p = gl::create_picture(px_size + 2 * m, px_size + 2 * m, {250, 250, 246, 255});
  const sh::MoveWorld& w = s.nav.world;
  const sh::Hull& hull = w.solids[0].point_hull;
  auto world_at = [&](double u, double z) {
    return along_x ? sh::Vec3f{static_cast<float>(u), s.at.y, static_cast<float>(z)}
                   : sh::Vec3f{s.at.x, static_cast<float>(u), static_cast<float>(z)};
  };
  const double u0 = (along_x ? s.at.x : s.at.y) - s.r, z1 = s.at.z + s.r;
  auto su = [&](double u) { return m + (u - u0) * scale; };
  auto sz = [&](double z) { return m + (z1 - z) * scale; };
  for (int y = 0; y < px_size; y++)
    for (int x = 0; x < px_size; x++) {
      const sh::Vec3f q = world_at(u0 + (x + 0.5) / scale, z1 - (y + 0.5) / scale);
      const int c = sh::point_contents(hull, q);
      gl::Rgba col{240, 240, 236, 255};
      if (c == sh::kContentsSolid)
        col = {80, 84, 82, 255};
      else if (c <= sh::kContentsWater)
        col = {150, 190, 230, 255};
      gl::set_pixel(p, x + m, y + m, col);
    }
  // The floors a standing player finds, every 4 units along the cut.
  std::vector<float> fl;
  const int across_n = static_cast<int>(2 * s.r / 4);
  for (int k = 0; k <= across_n; k++) {
    const double u = u0 + 4.0 * k;
    const sh::Vec3f q = world_at(u, 0);
    feet_floors(w, q.x, q.y, s.at.z + s.r, s.at.z - s.r, fl);
    for (const float f : fl)
      gl::draw_line(p, su(u) - 2, sz(f), su(u) + 2, sz(f), {40, 170, 60, 255}, 2);
  }
  // Movers crossing the cut.
  for (const Thing& t : s.things) {
    const float c = along_x ? s.at.y : s.at.x;
    if ((along_x ? t.lo.y : t.lo.x) > c || (along_x ? t.hi.y : t.hi.x) < c) continue;
    const double a = su(along_x ? t.lo.x : t.lo.y), b = su(along_x ? t.hi.x : t.hi.y);
    const gl::Rgba col = mark_colour(t.kind);
    gl::draw_line(p, a, sz(t.lo.z), b, sz(t.lo.z), col, 2);
    gl::draw_line(p, b, sz(t.lo.z), b, sz(t.hi.z), col, 2);
    gl::draw_line(p, b, sz(t.hi.z), a, sz(t.hi.z), col, 2);
    gl::draw_line(p, a, sz(t.hi.z), a, sz(t.lo.z), col, 2);
    gl::draw_text(p, a + 2, sz(t.hi.z) + 2, t.label, col);
  }
  // The nav within 16 of the cut.
  auto near_cut = [&](sh::Vec3f f) {
    const float d = along_x ? f.y - s.at.y : f.x - s.at.x;
    return d <= 16 && d >= -16;
  };
  each_link(s, [&](sh::Vec3f a, sh::Vec3f b, const sh::NavLink& l, bool) {
    if (!near_cut(a) || !near_cut(b)) return;
    gl::draw_line(p, su(along_x ? a.x : a.y), sz(a.z), su(along_x ? b.x : b.y), sz(b.z),
                  kind_colour(l.kind), 2);
  });
  for (const sh::NavNode& n : s.nav.graph.nodes) {
    const sh::Vec3f f{n.origin.x, n.origin.y, n.origin.z - 24};
    if (!inside_box(s, f) || !near_cut(f)) continue;
    gl::fill_disc(p, su(along_x ? f.x : f.y), sz(f.z), 3, kInk);
  }
  // The player's box at the spot, and the heights.
  const double a = su((along_x ? s.at.x : s.at.y) - 16), b = su((along_x ? s.at.x : s.at.y) + 16);
  const gl::Rgba red{200, 0, 0, 255};
  gl::draw_line(p, a, sz(s.at.z), b, sz(s.at.z), red, 2);
  gl::draw_line(p, b, sz(s.at.z), b, sz(s.at.z + 56), red, 2);
  gl::draw_line(p, b, sz(s.at.z + 56), a, sz(s.at.z + 56), red, 2);
  gl::draw_line(p, a, sz(s.at.z + 56), a, sz(s.at.z), red, 2);
  const double z_first = std::ceil((s.at.z - s.r) / 32) * 32;
  for (int k = 0; z_first + 32.0 * k <= z1; k++)
    gl::draw_text(p, 2, sz(z_first + 32.0 * k) - 3, to_fixed(z_first + 32.0 * k, 0),
                  {110, 110, 110, 255});
  gl::draw_text(p, m, 8,
                std::string(along_x ? "CUT ALONG X (WEST LEFT, EAST RIGHT) AT Y "
                                    : "CUT ALONG Y (SOUTH LEFT, NORTH RIGHT) AT X ") +
                    to_fixed(along_x ? s.at.y : s.at.x, 0) + ". GREEN: WHERE A PLAYER STANDS",
                kInk);
  return p;
}

// --- RUN -------------------------------------------------------------------------------------

bool load_scene(gl::Lab& lab, const ge::progs::Progs& progs, Scene& s, std::string& error) {
  const std::string& basedir = lab.arg("basedir");
  std::string entities;
  std::vector<uint8_t> bytes, store;
  std::span<const uint8_t> file;
  if (basedir.empty()) {
    if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bytes, &error))
      return false;
    file = bytes;
    s.name = "halls";
    entities = cn::kTeamHallsEntities;
  } else {
    cl::SearchPath files;
    if (!cl::open_game_dir(basedir, lab.arg("game"), files, error)) return false;
    s.name = lab.arg("map");
    file = cl::find_file(files, "maps/" + s.name + ".bsp", store);
    if (file.empty()) {
      error = "no maps/" + s.name + ".bsp in " + basedir + "/" + lab.arg("game");
      return false;
    }
  }
  s.map = cl::map_from_bytes(file, s.name, error);
  if (!s.map) return false;
  if (!entities.empty()) s.map->spec.entities = entities;
  sv::QcServerOptions so;
  so.loop.max_edicts = 1024;
  std::unique_ptr<sv::QcServer> qc = sv::make_qc_server(progs, so, error);
  if (!qc) return false;
  sv::RulesRunner rules = sv::qc_runner(std::move(qc));
  sv::QcServer& q = *sv::qc_of(rules);
  rules.console("deathmatch 1");
  if (!rules.start(s.map->spec, error)) return false;
  sv::use_map_sets(q, s.map->bsp);
  if (!bt::build_map_nav(q, s.map->spec, "info_player_deathmatch", s.nav, error)) return false;
  read_things(s, error);
  return error.empty();
}

void run(gl::Lab& lab) {
  std::string error;
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, error)) {
    lab.fail("se-spot: the baseline mod: " + error,
             "next: run tests/shooters_edge_baseline_mod_test.cpp");
    return;
  }
  Scene s;
  if (!load_scene(lab, progs, s, error)) {
    lab.fail("se-spot: " + error,
             "Pass --basedir=<the folder holding the game folder> and --map=<name>. Skill: bots.");
    return;
  }
  s.r = static_cast<float>(lab.arg_num("r"));
  if (!lab.arg("clip-top").empty()) s.clip_top = static_cast<float>(lab.arg_num("clip-top"));
  s.swim = lab.arg_bool("swim");
  if (!vec_of(lab.arg("at"), s.at)) {
    // No spot: the map's first item.
    if (s.items.empty()) {
      lab.fail("se-spot: no --at and no item on " + s.name,
               "next: pass --at=\"x y z\" (a player's feet on the map)");
      return;
    }
    s.at = s.items.front().at;
  }
  {
    std::istringstream r(lab.arg("route"));
    sh::Vec3f a, b;
    char sep = 0;
    if (r >> a.x >> a.y >> a.z >> sep >> b.x >> b.y >> b.z) {
      const int32_t na = sh::nav_nearest(s.nav.graph, {a.x, a.y, a.z + 24});
      const int32_t nb = sh::nav_nearest(s.nav.graph, {b.x, b.y, b.z + 24});
      ge::ai::Search search;
      if (na >= 0 && nb >= 0) s.route = sh::nav_route(s.nav.graph, na, nb, search).nodes;
      if (s.route.empty()) lab.note("se-spot: --route found no way between the two points");
    }
  }
  const std::vector<la::WireEdge> edges = la::wire_edges(s.map->bsp.map);
  const std::string where =
      to_fixed(s.at.x, 0) + " " + to_fixed(s.at.y, 0) + " " + to_fixed(s.at.z, 0);
  const std::vector<std::pair<std::string, std::string>> inputs = {
      {"map", s.name},           {"at", where}, {"r", to_fixed(s.r, 0)}, {"route", lab.arg("route")},
      {"clip-top", lab.arg("clip-top")}};
  const std::string id0 = lab.arg("id");
  auto id = [&](const std::string& v) { return id0.empty() ? v : id0 + "-" + v; };
  const int top_px = static_cast<int>(lab.arg_num("px"));

  if (lab.wants(id("top")))
    lab.shot(
        {id("top"), "SPOT " + where + " ON " + s.name + " FROM ABOVE",
         "The floor from above in a box of +-" + to_fixed(s.r, 0) +
             " round the spot (red cross): coloured by height with a contour every 16 units, "
             "water blue, solid dark (traced down from the spot's eye height, or the first open "
             "space above it). The nav's nodes (dots) and links by kind, items by call, "
             "doors, plats, buttons (SHOT ones ringed) and teleporters (with where they send) as "
             "boxes, spawns S, and the route in yellow with each node's feet height.",
         "A floor with no node on it (a tread the grid missed); a jump link that cannot be "
         "flown; a door or button on the way; where a route should go and does not.",
         inputs},
        top_picture(s, top_px));
  constexpr double kCorners[4] = {45, 135, 225, 315};
  constexpr const char* kCornerName[4] = {"north-east", "north-west", "south-west", "south-east"};
  for (int k = 0; k < 4; k++) {
    // WireView's yaw turns the map; a corner's name is where the eye stands.
    const std::string v = id(std::string("wire-") + kCornerName[k]);
    if (!lab.wants(v)) continue;
    lab.shot(
        {v,
         "SPOT " + where + " ON " + s.name + " AS A WIREFRAME, TURNED " + to_fixed(kCorners[k], 0),
         "Every face outline of the world inside the box, seen at 35 degrees with the map "
         "turned " +
             to_fixed(kCorners[k], 0) +
             " degrees: the nav's jumps, drops and swims in "
             "colour, doors, buttons and teleporters as boxes, the route in yellow, items "
             "labelled.",
         "The shape of the steps, ledges and openings round the spot.", inputs},
        wire_corner(s, edges, kCorners[k]));
  }
  struct Eye {
    std::string name;
    la::InsideCamera cam;
  };
  std::vector<Eye> eyes;
  eyes.reserve(5);
  for (int k = 0; k < 4; k++)
    eyes.push_back(
        {std::string("from-") + kCornerName[k],
         eye_toward(s, kCorners[k], s.r * 0.75f, static_cast<float>(lab.arg_num("eye-up")))});
  {
    std::istringstream e(lab.arg("eye"));
    la::InsideCamera c;
    double yaw = 0, pitch = 0;
    if (e >> c.eye.x >> c.eye.y >> c.eye.z >> yaw >> pitch) {
      c.yaw = yaw;
      c.pitch = pitch;
      c.fov = 100;
      c.width = 960;
      c.height = 600;
      c.fade = s.r * 4;
      eyes.push_back({"eye", c});
    }
  }
  constexpr const char* kKindName[4] = {"view", "depth", "floors", "normals"};
  constexpr const char* kKindWords[4] = {
      "the faces shaded and outlined, doors brown, buttons red, plats purple, teleporters and "
      "triggers as magenta and yellow boxes, the nav's nodes and links by kind, the route yellow, "
      "the player's box red at the spot",
      "the depth buffer: near red through yellow and green to far blue (spread over the depths "
      "this view holds), a black contour every 64 units along the view",
      "every face by its slope as the eye sees it: a floor a player stands on green, a slope too "
      "steep orange, walls grey, ceilings blue-grey; the nav over it",
      "every face's normal as a colour (x red, y green, z blue); the nav over it"};
  for (const Eye& e : eyes)
    for (int k = 0; k < 4; k++) {
      const std::string v = id(std::string(kKindName[k]) + "-" + e.name);
      if (!lab.wants(v)) continue;
      const la::InsideCamera& c = e.cam;
      lab.shot(
          {v, "SPOT " + where + " ON " + s.name + ", " + kKindName[k] + " " + e.name,
           "From inside the map, the eye at " + to_fixed(c.eye.x, 0) + " " + to_fixed(c.eye.y, 0) +
               " " + to_fixed(c.eye.z, 0) + " (yaw " + to_fixed(c.yaw, 0) + ", pitch " +
               to_fixed(c.pitch, 0) + ", fov 100): " + kKindWords[k] + ".",
           "How the spot is reached: steps, gaps, ledges, doors in the way.", inputs},
          inside_picture(s, edges, c, static_cast<InsideKind>(k)));
    }
  for (int k = 0; k < 2; k++) {
    const std::string v = id(k == 0 ? "cut-x" : "cut-y");
    if (!lab.wants(v)) continue;
    lab.shot(
        {v, "SPOT " + where + " ON " + s.name + ", CUT ALONG " + (k == 0 ? "X" : "Y"),
         "An upright section through the spot: solid dark, empty pale, water blue; a green tick "
         "wherever a standing player finds a floor straight down (every 4 units), the nav's "
         "nodes and links within 16 units of the cut, movers crossing it, the player's box red.",
         "Each step's rise and tread (a tread with no node is a tread the grid missed).", inputs},
        cut_picture(s, k == 0, top_px));
  }
  // THE SHEET: every view of the spot on one picture (a quarter size each), for one look.
  if (lab.wants(id("sheet"))) {
    const int tw = 480, th = 300, cols = 4;
    std::vector<std::pair<std::string, gl::Picture>> tiles;
    auto square = [&](const gl::Picture& pic, const std::string& name) {
      tiles.push_back({name, gl::resized(pic, th * pic.width / pic.height, th)});
    };
    square(top_picture(s, top_px), "TOP");
    square(cut_picture(s, true, top_px), "CUT ALONG X");
    square(cut_picture(s, false, top_px), "CUT ALONG Y");
    square(wire_corner(s, edges, kCorners[2]), "WIRE TURNED 225");
    for (const Eye& e : eyes)
      for (int k = 0; k < 4; k++)
        square(inside_picture(s, edges, e.cam, static_cast<InsideKind>(k)),
               std::string(kKindName[k]) + " " + e.name);
    const int rows = static_cast<int>((tiles.size() + cols - 1) / cols);
    gl::Picture sheet = gl::create_picture(cols * tw, rows * (th + 14), {250, 250, 246, 255});
    for (size_t k = 0; k < tiles.size(); k++) {
      const int x = static_cast<int>(k % cols) * tw, y = static_cast<int>(k / cols) * (th + 14);
      gl::blit(sheet, tiles[k].second, x + (tw - tiles[k].second.width) / 2, y + 14);
      gl::draw_text(sheet, x + 4, y + 3, tiles[k].first, kInk);
    }
    lab.shot(
        {id("sheet"), "SPOT " + where + " ON " + s.name + ", EVERY VIEW",
         "Every picture of the spot at a glance: from above, the two cuts, a wireframe, and each "
         "inside eye as shaded view, depth, slopes and normals (each picture alone beside it "
         "at full size).",
         "Which view answers the question; then open that one.", inputs},
        sheet);
  }
  // The things and items in the box, as a table.
  gl::LabTable tt{
      "what stands in the box round the spot", {"kind", "label", "box lo", "box hi", "to"}, {}};
  auto v3 = [](sh::Vec3f v) {
    return to_fixed(v.x, 0) + " " + to_fixed(v.y, 0) + " " + to_fixed(v.z, 0);
  };
  for (const Thing& t : s.things)
    if (inside_box(s, (t.lo + t.hi) * 0.5f, 64))
      tt.rows.push_back(
          {mark_name(t.kind), t.label, v3(t.lo), v3(t.hi), t.has_dest ? v3(t.dest) : ""});
  for (const Item& it : s.items)
    if (inside_box(s, it.at, 32)) tt.rows.push_back({"item", it.call, v3(it.at), "", ""});
  lab.table(tt);
  int nodes = 0;
  for (const sh::NavNode& n : s.nav.graph.nodes)
    nodes += inside_box(s, {n.origin.x, n.origin.y, n.origin.z - 24});
  lab.metric("spot.nodes", nodes, "count");
  lab.metric("spot.route-nodes", static_cast<double>(s.route.size()), "count");
}

const gl::LabSpec kSpec{
    "se-spot",
    gl::LabKind::picture,
    "game:shooters_edge",
    "one place on a map looked at every way, the bots' nav over it "
    "(games/shooters_edge/labs/se_spot_lab.cpp)",
    "bots",
    "How does a player get to this spot, and what does the bots' nav make of it?",
    {
        {"basedir", "", "a folder holding the game folder; empty: the fixture halls"},
        {"game", "id1", "the game folder under --basedir"},
        {"map", "e1m2", "the map under --basedir"},
        {"at", "", "the spot, a player's feet: 'x y z' (empty: the map's first item)"},
        {"r", "256", "the box round the spot, units each way"},
        {"clip-top", "",
         "the top view cut away over this height, every roof under it stepped through (a level "
         "under another); empty: from the spot's eye height"},
        {"route", "", "a nav route drawn: 'x y z > x y z' (feet; the nearest nodes)"},
        {"eye", "", "one more inside view: 'x y z yaw pitch' (degrees)"},
        {"eye-up", "96", "the four inside eyes stand this far over the spot, units"},
        {"swim", "0", "the water's swim nodes and links drawn too (many: they hide the rest)"},
        {"px", "900", "the top view's and the cuts' size, pixels"},
        {"id", "", "a prefix for the pictures' ids (several spots in one folder)"},
    },
    "--r=128 --px=300",
    run,
};
GE_LAB(kSpec);

}  // namespace
