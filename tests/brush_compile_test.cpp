// Tests: the map compiler (plugins/brush/compile). A hollow box room with a player start and a
// light compiles to a version 29 map our reader takes: one open leaf, six faces (the walls'
// inner sides, merged), lightmaps brighter near the light than in the far corner, and clip hulls
// in which the player's box stands on the floor 24 units up. A room with a hole in its wall is
// refused as a leak naming the start; a map with no entity is refused; a brush entity is a model
// of its own; a pillar in the room splits the open space and shadows the wall behind it. Built as
// version 38, the room keeps its brushes (a clip brush and a pool among them), and a box swept
// through them stops short of the wall, at the clip brush only for the player's mask.
//
// ge:summary Test: the map compiler: a sealed room compiles and reads, leaks refused, hulls, light
// ge:keywords test map-compiler compile_map leak hull light model brush-entity
// ge:skill brush testing
#include <algorithm>
#include <string>
#include <vector>

#include "brush/compile/compile.hpp"
#include "brush/compile/plan.hpp"
#include "brush/compile/svg.hpp"
#include "brush/read.hpp"
#include "brush/trace.hpp"
#include "check.hpp"

namespace {

namespace br = ge::brush;
namespace mc = ge::brush::mapc;

// A hollow box: inside lo..hi, walls `t` thick.
void hollow(mc::Entity* world, const mc::V3& lo, const mc::V3& hi, double t, bool hole = false) {
  const std::string tex = "wall";
  // Floor, ceiling.
  world->brushes.push_back(
      mc::box_brush({lo[0] - t, lo[1] - t, lo[2] - t}, {hi[0] + t, hi[1] + t, lo[2]}, "floor"));
  world->brushes.push_back(
      mc::box_brush({lo[0] - t, lo[1] - t, hi[2]}, {hi[0] + t, hi[1] + t, hi[2] + t}, "ceiling"));
  // Four walls; the hole cuts the −x wall short of the floor.
  world->brushes.push_back(mc::box_brush({lo[0] - t, lo[1] - t, hole ? lo[2] + 64 : lo[2]},
                                         {lo[0], hi[1] + t, hi[2]}, tex));
  world->brushes.push_back(
      mc::box_brush({hi[0], lo[1] - t, lo[2]}, {hi[0] + t, hi[1] + t, hi[2]}, tex));
  world->brushes.push_back(mc::box_brush({lo[0], lo[1] - t, lo[2]}, {hi[0], lo[1], hi[2]}, tex));
  world->brushes.push_back(mc::box_brush({lo[0], hi[1], lo[2]}, {hi[0], hi[1] + t, hi[2]}, tex));
}

mc::Entity point(const std::string& cls, const std::string& origin) {
  mc::Entity e;
  e.keys = {{"classname", cls}, {"origin", origin}};
  return e;
}

mc::Source room(bool hole = false) {
  mc::Source s;
  mc::Entity world;
  world.keys = {{"classname", "worldspawn"}, {"light", "10"}};
  hollow(&world, {0, 0, 0}, {224, 224, 128}, 16, hole);
  s.entities.push_back(world);
  s.entities.push_back(point("info_player_start", "64 64 24"));
  mc::Entity l = point("light", "180 180 100");
  l.set("light", "250");
  s.entities.push_back(l);
  return s;
}

int open_leaves(const br::Map& m) {
  int n = 0;
  for (const br::Leaf& l : m.leaves) n += l.contents == -1 ? 1 : 0;
  return n;
}

}  // namespace

TEST(compile_room_reads) {
  std::vector<uint8_t> bytes;
  mc::CompileReport rep;
  std::string err;
  CHECK_MSG(mc::compile_map(room(), {}, &bytes, &rep, &err), "compile failed: " + err);
  br::Map m;
  CHECK_MSG(br::read_bsp(bytes, &m, &err), "read failed: " + err);
  CHECK(m.version == 29);
  CHECK(open_leaves(m) == 1);
  CHECK_MSG(m.surfaces.size() == 6, "faces: " + std::to_string(m.surfaces.size()));
  CHECK(m.models.size() == 1);
  CHECK(rep.faces_without_side == 0);
}

TEST(compile_v38_keeps_brushes_a_box_sweeps_against) {
  mc::Source src = room();
  // A clip brush across the room's middle, and a pool of water in a corner.
  src.entities[0].brushes.push_back(mc::box_brush({150, 0, 0}, {160, 224, 128}, "clip"));
  src.entities[0].brushes.push_back(mc::box_brush({0, 160, 0}, {64, 224, 32}, "*water1"));
  mc::CompileOptions opt;
  opt.version = 38;
  std::vector<uint8_t> bytes;
  mc::CompileReport rep;
  std::string err;
  CHECK_MSG(mc::compile_map(src, opt, &bytes, &rep, &err), "compile failed: " + err);
  CHECK(rep.clip_nodes == 0);
  br::Map m;
  CHECK_MSG(br::read_bsp(bytes, &m, &err), "read failed: " + err);
  CHECK(m.version == 38);
  CHECK_MSG(m.brushes.size() == 8, "brushes: " + std::to_string(m.brushes.size()));
  CHECK(m.clusters == 1);
  br::BrushTracer tracer(m);
  const float mins[3] = {-16, -16, -24}, maxs[3] = {16, 16, 32};
  // Toward the +y wall: stops a box's half and 1/32 short of it.
  const float a[3] = {100, 64, 40}, b[3] = {100, 400, 40};
  br::Trace t = tracer.trace(a, b, mins, maxs, 0x1);
  CHECK(!t.start_solid);
  CHECK_MSG(t.end[1] > 207.9f && t.end[1] < 208.0f, "end y " + std::to_string(t.end[1]));
  // The clip brush stops the player's mask, not a shot's (solid alone).
  const float c[3] = {64, 64, 40}, d[3] = {200, 64, 40};
  CHECK(tracer.trace(c, d, mins, maxs, 0x1 | 0x10000).end[0] < 150 - 16);
  CHECK(tracer.trace(c, d, mins, maxs, 0x1).fraction == 1.0f);
  // The water's contents at a point in the pool.
  const float w[3] = {32, 192, 16};
  CHECK((tracer.contents(w) & 0x20) != 0);
}

TEST(compile_leak_refused) {
  std::vector<uint8_t> bytes;
  mc::CompileReport rep;
  std::string err;
  CHECK(!mc::compile_map(room(true), {}, &bytes, &rep, &err));
  CHECK(rep.leaked);
  CHECK_MSG(err.find("info_player_start") != std::string::npos, "leak error: " + err);
}

TEST(compile_needs_an_entity) {
  mc::Source s = room();
  s.entities.resize(1);
  std::vector<uint8_t> bytes;
  mc::CompileReport rep;
  std::string err;
  CHECK(!mc::compile_map(s, {}, &bytes, &rep, &err));
  CHECK(err.find("player start") != std::string::npos);
}

TEST(compile_hull_floor) {
  std::vector<uint8_t> bytes;
  mc::CompileReport rep;
  std::string err;
  CHECK(mc::compile_map(room(), {}, &bytes, &rep, &err));
  br::Map m;
  CHECK(br::read_bsp(bytes, &m, &err));
  // Hull 1 (the player's box): the origin is open at 25 over the floor and solid at 23.
  const auto contents = [&](int hull, float x, float y, float z) {
    int32_t n = m.models[0].hulls[hull - 1];
    while (n >= 0) {
      const br::ClipNode& c = m.clip_nodes[size_t(n)];
      const br::Plane& p = m.planes[c.plane];
      const float d = p.n[0] * x + p.n[1] * y + p.n[2] * z - p.d;
      n = c.child[d >= 0 ? 0 : 1];
    }
    return n;
  };
  CHECK(contents(1, 128, 128, 25) == -1);
  CHECK(contents(1, 128, 128, 23) == -2);
  CHECK(contents(1, 128, 128, 128 - 31) == -2);
  CHECK(contents(1, 10, 128, 64) == -2);  // within 16 of the −x wall
  CHECK(contents(1, 20, 128, 64) == -1);
  CHECK(contents(2, 40, 128, 48) == -1);
  CHECK(contents(2, 30, 128, 48) == -2);
  // Each hull's root is its first clip node and every child is numbered after its parent: a
  // server takes the root's number as the hull's first node and refuses a trace below it.
  for (int hull = 0; hull < 2; hull++) {
    const int32_t root = m.models[0].hulls[hull];
    for (size_t n = size_t(root); n < m.clip_nodes.size(); n++)
      for (int32_t ch : m.clip_nodes[n].child)
        CHECK_MSG(ch < 0 || ch > int32_t(n), "hull " + std::to_string(hull + 1) + " node " +
                                                 std::to_string(n) + " child " +
                                                 std::to_string(ch));
  }
}

TEST(compile_light_falls_off) {
  std::vector<uint8_t> bytes;
  mc::CompileReport rep;
  std::string err;
  CHECK(mc::compile_map(room(), {}, &bytes, &rep, &err));
  CHECK(rep.lights == 1);
  CHECK(rep.luxels > 0);
  br::Map m;
  CHECK(br::read_bsp(bytes, &m, &err));
  // The floor's luxels: brightest near (200, 200), the least light far off.
  int lo = 255, hi = 0;
  for (size_t i = 0; i < m.surfaces.size(); ++i) {
    const br::Surface& s = m.surfaces[i];
    if (s.lightmap < 0) continue;
    const br::Lightmap& lm = m.lightmaps[size_t(s.lightmap)];
    for (uint32_t k = 0; k < lm.width * lm.height; ++k) {
      const int v = m.light_rgb[lm.offset + k * 3];
      lo = std::min(lo, v);
      hi = std::max(hi, v);
    }
  }
  CHECK_MSG(hi > 150, "brightest " + std::to_string(hi));
  CHECK_MSG(lo >= 10 && lo < 60, "darkest " + std::to_string(lo));
}

TEST(compile_brush_entity_model) {
  mc::Source s = room();
  mc::Entity door;
  door.keys = {{"classname", "func_door"}, {"angle", "90"}};
  door.brushes.push_back(mc::box_brush({100, 100, 0}, {140, 108, 96}, "door"));
  s.entities.push_back(door);
  std::vector<uint8_t> bytes;
  mc::CompileReport rep;
  std::string err;
  CHECK_MSG(mc::compile_map(s, {}, &bytes, &rep, &err), err);
  br::Map m;
  CHECK(br::read_bsp(bytes, &m, &err));
  CHECK(m.models.size() == 2);
  bool named = false;
  for (const br::Entity& e : m.entities) {
    for (const auto& [k, v] : e.pairs) named = named || (k == "model" && v == "*1");
  }
  CHECK(named);
}

TEST(compile_pillar_shadow) {
  mc::Source s = room();
  s.entities[0].brushes.push_back(mc::box_brush({150, 150, 0}, {170, 170, 128}, "pillar"));
  std::vector<uint8_t> bytes;
  mc::CompileReport rep;
  std::string err;
  CHECK_MSG(mc::compile_map(s, {}, &bytes, &rep, &err), err);
  br::Map m;
  CHECK(br::read_bsp(bytes, &m, &err));
  CHECK(open_leaves(m) >= 4);
  CHECK(rep.faces_without_side == 0);
}

namespace {

// Two rooms joined by a door, a pool in the first, stairs and a ramp, all turned round (0, 0).
constexpr const char* kPlan = R"(# a test plan
world message="Test" light=10
textures floor=f wall=w ceil=c solid=s
room hall z=0..192 box 64,-128 448,128
room door z=0..112 box 448,-48 512,48
room side z=0..160 poly 512,-96 704,-96 704,96 608,192 512,96
room pit z=-96..0 box 128,-64 256,64
liquid z=-96..-16 box 128,-64 256,64 kind=water
stairs z=0..64 box 320,-128 448,-64 up=-x
ramp z=0..48 box 560,-96 656,-32 up=+y
light 256,0,160 value=300
spawn 384,0,24 angle=0
entity item_armor1 96,96,24 once
symmetry rotate=0,0
)";

mc::Source plan_source(const char* text) {
  mc::Source s;
  std::string err;
  CHECK_MSG(mc::read_plan(text, &s, &err), "plan: " + err);
  return s;
}

}  // namespace

TEST(plan_compiles) {
  const mc::Source s = plan_source(kPlan);
  // The world, then two lights, two spawns (turned) and the armour once.
  CHECK(s.entities.size() == 6);
  int spawns = 0;
  for (const mc::Entity& e : s.entities) {
    const std::string* c = e.get("classname");
    if (c && *c == "info_player_deathmatch") {
      ++spawns;
      const std::string* o = e.get("origin");
      const std::string* a = e.get("angle");
      CHECK(o && (*o == "384 0 24" || *o == "-384 0 24"));
      CHECK(a && (*o == "384 0 24" ? *a == "0" : *a == "180"));
    }
  }
  CHECK(spawns == 2);
  std::vector<uint8_t> bytes;
  mc::CompileReport rep;
  std::string err;
  CHECK_MSG(mc::compile_map(s, {}, &bytes, &rep, &err), "compile: " + err);
  CHECK(rep.warnings.empty());
  br::Map m;
  CHECK_MSG(br::read_bsp(bytes, &m, &err), err);
  int water = 0;
  for (const br::Leaf& l : m.leaves) water += l.contents == -3 ? 1 : 0;
  CHECK(water >= 2);  // a pool in each hall
}

TEST(plan_faults_name_lines) {
  mc::Source s;
  std::string err;
  CHECK(!mc::read_plan("room a z=0..64 box 0,0 64,64\nroom b z=64..0 box 0,0 8,8\nbogus 1\n", &s,
                       &err));
  CHECK_MSG(err.find("line 2:") != std::string::npos, err);
  CHECK_MSG(err.find("line 3:") != std::string::npos, err);
  CHECK(!mc::read_plan("solid z=0..64 box 0,0 64,64 tex=*water\n", &s, &err));
  CHECK(err.find("liquid") != std::string::npos);
}

// A plan's intent: its modes, areas and tricks, repeated by the symmetry unless `once`.
TEST(plan_intent) {
  const char* plan =
      "modes duel,4on4\n"
      "symmetry rotate=256,128\n"
      "room hall z=0..128 box 0,0 512,256\n"
      "area ra z=0..128 box 0,0 128,128 intent=camp modes=4on4 note=\"one way in\"\n"
      "area mid z=0..128 box 192,0 320,256 intent=fight once\n"
      "trick up from 64,64,24 to 200,64,88 kind=rj level=1\n"
      "spawn 64,200,24\n";
  mc::Source s;
  mc::PlanIntent in;
  std::string err;
  CHECK_MSG(mc::read_plan(plan, &s, &err, &in), err);
  CHECK(in.modes == "duel,4on4");
  CHECK(in.areas.size() == 3);  // ra, its turned copy, mid once
  CHECK(in.tricks.size() == 2);
  CHECK(in.areas[0].intent == "camp" && in.areas[0].note == "one way in");
  CHECK(mc::area_holds(in.areas[0], 64, 64, 24));
  CHECK(!mc::area_holds(in.areas[0], 64, 64, 200));
  const mc::PlanArea& turned = in.areas[2];
  CHECK(turned.copy == 1 && turned.name == "ra");
  CHECK(mc::area_holds(turned, 448, 192, 24));
  CHECK(in.tricks[1].copy == 1 && in.tricks[1].from[0] == 448 && in.tricks[1].level == 1);
  CHECK(!mc::read_plan("room a z=0..64 box 0,0 64,64\narea b z=0..64 box 0,0 8,8 intent=nap\n", &s,
                       &err));
  CHECK_MSG(err.find("line 2:") != std::string::npos, err);
  CHECK(!mc::read_plan("room a z=0..64 box 0,0 64,64\ntrick t from 0,0,0 to 1,1,1 kind=fly\n", &s,
                       &err));
}

TEST(subtract_box_from_box) {
  const mc::Brush outer = mc::box_brush({0, 0, 0}, {300, 300, 300}, "s");
  const mc::Brush inner = mc::box_brush({100, 100, 100}, {200, 200, 200}, "room");
  CHECK(mc::subtract(outer, inner).size() == 6);
  // Apart: untouched.
  const mc::Brush far = mc::box_brush({400, 0, 0}, {500, 100, 100}, "room");
  CHECK(mc::subtract(outer, far).size() == 1);
}

TEST(svg_floor_plan) {
  const std::string svg = R"svg(<?xml version="1.0"?>
<svg xmlns="http://www.w3.org/2000/svg" data-ge="world message=&quot;Drawn&quot;">
  <g transform="translate(0,0)">
    <rect x="0" y="-256" width="256" height="256" data-ge="room hall z=0..128"/>
    <path d="M 256 -160 h 128 v 64 h -128 Z" data-ge="room run z=0..96"/>
    <polyline points="64,-200 192,-200" stroke-width="16" data-ge="wall z=0..64"/>
    <circle cx="128" cy="-64" r="8" data-ge="spawn z=24 angle=90"/>
    <circle cx="128" cy="-160" r="8" data-ge="light z=100 value=250"/>
  </g>
</svg>)svg";
  std::string plan, err;
  CHECK_MSG(mc::svg_to_plan(svg, &plan, &err), err);
  CHECK_MSG(plan.find("room hall z=0..128 box 0,256 256,0") != std::string::npos, plan);
  CHECK_MSG(plan.find("spawn 128,64,24 angle=90") != std::string::npos, plan);
  CHECK_MSG(plan.find("thick=16") != std::string::npos, plan);
  const mc::Source s = plan_source(plan.c_str());
  std::vector<uint8_t> bytes;
  mc::CompileReport rep;
  CHECK_MSG(mc::compile_map(s, {}, &bytes, &rep, &err), "compile: " + err);
  // A curve is refused with its line.
  CHECK(!mc::svg_to_plan("<svg>\n<path d=\"M0 0 C 1 1 2 2 3 3\" data-ge=\"room a z=0..64\"/></svg>",
                         &plan, &err));
  CHECK(err.find("svg line 2") != std::string::npos);
}
