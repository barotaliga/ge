// Tests: the brush trace (plugins/brush/trace.hpp) over brush worlds written as version 38 maps
// (write_brush_world). The brushes, sides and leaf brushes read back; a box falling on a floor
// stops 1/32 above it with the floor's plane, surface and contents; a box inside a brush is start
// solid, and all solid when it never leaves; a box touching a face is inside it; the mask picks
// the brushes; a slanted side is hit with its own normal; a brush model is traced through its own
// head node; the tree reaches every brush clipping them all would; the contents at a point are the
// brushes'; and a seeded set of traces holds golden (tests/golden/brush_trace.txt; rewrite it with
// GE_GOLDEN_WRITE=1 when the trace deliberately changes).
//
// ge:summary Test: the brush trace over written brush worlds: floors, solids, masks, slopes, golden
// ge:keywords test brush trace sweep position-test start-solid all-solid epsilon mask slope golden
// ge:keywords brush-world point-contents brush-model
// ge:skill brush testing
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/trace.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "core/prng.hpp"
#include "golden.hpp"

namespace {

namespace br = ge::brush;
namespace core = ge::core;

constexpr const char* kGolden = "tests/golden/brush_trace.txt";
constexpr int32_t kSolid = 1, kWater = 0x20, kPlayerClip = 0x10000;
constexpr int32_t kPlayerSolid = 0x1 | 0x2 | 0x10000 | 0x2000000;
const float kMins[3] = {-16, -16, -24}, kMaxs[3] = {16, 16, 32}, kZero[3] = {0, 0, 0};

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1,
                   int32_t contents = kSolid, const std::string& tex = "world/floor") {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, contents, tex);
}

// A floor, a wall, a pool of water over its own floor, a ramp rising toward +y, a player clip, and
// a door (model 1).
br::BrushWorld world() {
  br::BrushWorld w;
  w.models.resize(2);
  auto& m = w.models[0];
  m.push_back(box(-256, -512, -16, 512, 512, 0));
  m.push_back(box(256, -512, 0, 272, 512, 256, kSolid, "world/wall"));
  m.push_back(box(-512, -512, -128, -256, 512, -16, kWater, "world/water"));
  m.push_back(box(-512, -512, -144, -256, 512, -128));
  br::WorldBrush ramp = box(-128, 0, 0, 0, 128, 64, kSolid, "world/ramp");
  const float up[3] = {0, -1, 2};  // rises 1 in 2 toward +y
  br::add_side(&ramp, up, 0, "world/ramp");
  m.push_back(ramp);
  m.push_back(box(100, 100, 0, 140, 140, 200, kPlayerClip, ""));
  w.models[1].push_back(box(400, -32, 0, 416, 32, 128, kSolid, "door/metal"));
  br::Entity door;
  door.pairs = {{"classname", "func_door"}, {"model", "*1"}};
  w.entities.push_back(door);
  return w;
}

br::Map read_world(const br::BrushWorld& w) {
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  br::Map m;
  CHECK_MSG(br::read_bsp(bytes, &m, &error), error);
  return m;
}

br::Trace sweep(br::BrushTracer& t, float x0, float y0, float z0, float x1, float y1, float z1,
                const float* mins = kMins, const float* maxs = kMaxs, int32_t mask = kPlayerSolid,
                uint32_t model = 0) {
  const float a[3] = {x0, y0, z0}, b[3] = {x1, y1, z1};
  return t.trace(a, b, mins, maxs, mask, model);
}

const std::string& name_of(const br::Map& m, const br::Trace& r) {
  static const std::string none;
  return r.texture < 0 ? none : m.textures[size_t(r.texture)].name;
}

TEST(a_brush_world_reads_back_its_brushes_sides_and_leaves) {
  const br::Map m = read_world(world());
  CHECK(m.family == br::Family::v38);
  CHECK(m.brushes.size() == 7);
  if (m.brushes.size() != 7) return;
  CHECK(m.brushes[4].sides == 7 && m.brushes[2].contents == kWater);
  CHECK(m.models.size() == 2);
  // Planes in pairs, the even one of an axial pair facing +axis with its type.
  for (size_t i = 0; i + 1 < m.planes.size(); i += 2) {
    CHECK(m.planes[i].n[0] == -m.planes[i + 1].n[0] && m.planes[i].d == -m.planes[i + 1].d);
    CHECK(m.planes[i].type == m.planes[i + 1].type);
  }
  // Every leaf's brushes in range, and the player clip's sides have no surface.
  for (const br::Leaf& l : m.leaves) CHECK(l.first_brush + l.brushes <= m.leaf_brushes.size());
  const br::Brush& clip = m.brushes[5];
  for (uint32_t k = 0; k < clip.sides; ++k) CHECK(m.brush_sides[clip.first_side + k].texture == -1);
  CHECK(m.entities.size() == 2 && m.entities[1].get("model") == "*1");
}

TEST(a_box_falling_on_the_floor_stops_a_32nd_above_it) {
  const br::Map m = read_world(world());
  br::BrushTracer t(m);
  const br::Trace r = sweep(t, 0, -200, 100, 0, -200, 0);
  CHECK(!r.start_solid && !r.all_solid);
  // d1 = 100 - 24, d2 = 0 - 24: (76 - 1/32) / 100.
  CHECK_BITS(r.fraction, float((76.0 - 0.03125) / 100.0));
  CHECK_BITS(r.end[2], 100.0f + r.fraction * -100.0f);
  CHECK(r.end[2] > 24 && r.end[2] < 24.04f);
  CHECK(r.plane.n[2] == 1 && r.plane.d == 0);
  CHECK(r.contents == kSolid && name_of(m, r) == "world/floor");
  // From there, a trace that does not move is clear; from on the floor it is inside.
  CHECK(!sweep(t, 0, -200, r.end[2], 0, -200, r.end[2]).start_solid);
  const br::Trace on = sweep(t, 0, -200, 24, 0, -200, 24);
  CHECK(on.start_solid && on.all_solid && on.fraction == 0 && on.contents == kSolid);
}

TEST(a_box_inside_a_brush_is_start_solid_and_all_solid_when_it_stays) {
  const br::Map m = read_world(world());
  br::BrushTracer t(m);
  const br::Trace stay = sweep(t, 0, 0, -8, 10, 0, -8, kZero, kZero);
  CHECK(stay.start_solid && stay.all_solid);
  const br::Trace leave = sweep(t, 0, -200, -8, 0, -200, 100, kZero, kZero);
  CHECK(leave.start_solid && !leave.all_solid);
}

TEST(the_mask_picks_the_brushes) {
  const br::Map m = read_world(world());
  br::BrushTracer t(m);
  // The pool: passed by the player's mask, hit by the water mask.
  const br::Trace dive = sweep(t, -400, 0, 64, -400, 0, -200, kZero, kZero);
  CHECK(dive.contents == kSolid && dive.end[2] > -128 && dive.end[2] < -127.9f);
  const br::Trace wet = sweep(t, -400, 0, 64, -400, 0, -200, kZero, kZero, kWater);
  CHECK(wet.contents == kWater && wet.end[2] > -16.0f && wet.end[2] < -15.9f);
  // The player clip stops the player and not a shot.
  CHECK(sweep(t, 0, 120, 100, 200, 120, 100).fraction < 1);
  CHECK(sweep(t, 0, 120, 100, 200, 120, 100, kZero, kZero, kSolid).fraction == 1);
  CHECK(name_of(m, sweep(t, 0, 120, 100, 200, 120, 100)).empty());
}

TEST(a_slanted_side_is_hit_with_its_own_normal) {
  const br::Map m = read_world(world());
  br::BrushTracer t(m);
  const br::Trace r = sweep(t, -64, 64, 200, -64, 64, 0, kZero, kZero);
  CHECK(r.fraction < 1 && name_of(m, r) == "world/ramp");
  CHECK(r.plane.n[2] > 0.89f && r.plane.n[2] < 0.9f && r.plane.n[1] < 0);
  CHECK(r.end[2] > 32 && r.end[2] < 32.1f);  // the ramp is 32 high at y 64
  // Walking along +y into it with the player's box hits the slope, not a wall.
  const br::Trace walk = sweep(t, -64, -64, 25, -64, 64, 25);
  CHECK(walk.fraction < 1 && walk.plane.n[2] > 0.89f);
}

TEST(a_brush_model_is_traced_through_its_own_head_node) {
  const br::Map m = read_world(world());
  br::BrushTracer t(m);
  // The door is not in the world's tree, and is in its own.
  CHECK(sweep(t, 300, 0, 64, 500, 0, 64, kMins, kMaxs, kPlayerSolid, 0).fraction == 1);
  const br::Trace r = sweep(t, 300, 0, 64, 500, 0, 64, kMins, kMaxs, kPlayerSolid, 1);
  CHECK(r.fraction < 1 && r.plane.n[0] == -1 && name_of(m, r) == "door/metal");
  CHECK(r.end[0] > 383.9f && r.end[0] < 384);
}

TEST(the_contents_at_a_point_are_the_brushes) {
  const br::Map m = read_world(world());
  br::BrushTracer t(m);
  const auto at = [&](float x, float y, float z, uint32_t model = 0) {
    const float p[3] = {x, y, z};
    return t.contents(p, model);
  };
  CHECK(at(0, -200, 10) == 0);
  CHECK(at(0, -200, -8) == kSolid);
  CHECK(at(-400, 0, -64) == kWater);
  CHECK(at(120, 120, 50) == kPlayerClip);
  CHECK(at(264, 0, 100) == kSolid);
  CHECK(at(408, 0, 64) == 0 && at(408, 0, 64, 1) == kSolid);
  CHECK(at(5000, 0, 0) == 0);
}

// The tree only decides which brushes a piece reaches: a map whose every leaf lists every world
// brush (and answers every contents bit) gives the same bits, trace for trace.
TEST(the_tree_finds_what_clipping_every_brush_finds) {
  const br::Map m = read_world(world());
  br::Map flat = m;
  flat.leaf_brushes.clear();
  const auto world_brushes = uint32_t(flat.brushes.size() - 1);  // the door is model 1's
  for (uint32_t b = 0; b < world_brushes; ++b) flat.leaf_brushes.push_back(b);
  for (br::Leaf& l : flat.leaves) {
    l.contents = -1;
    l.first_brush = 0;
    l.brushes = world_brushes;
  }
  br::BrushTracer t(m), all(flat);
  core::Rng rng = core::make_rng(7u);
  int hits = 0;
  for (int i = 0; i < 400; ++i) {
    float a[3], b[3];
    for (int k = 0; k < 3; ++k) {
      a[k] = float(core::range(rng, k == 2 ? -150 : -520, k == 2 ? 300 : 520));
      b[k] = i % 8 == 0 ? a[k] : float(core::range(rng, k == 2 ? -150 : -520, k == 2 ? 300 : 520));
    }
    const bool point = i % 3 == 0;
    const br::Trace x = t.trace(a, b, point ? kZero : kMins, point ? kZero : kMaxs, kPlayerSolid);
    const br::Trace y = all.trace(a, b, point ? kZero : kMins, point ? kZero : kMaxs, kPlayerSolid);
    CHECK_BITS(x.fraction, y.fraction);
    for (int k = 0; k < 3; ++k) CHECK_BITS(x.end[k], y.end[k]);
    CHECK(x.start_solid == y.start_solid && x.all_solid == y.all_solid);
    if (x.fraction < 1 && !x.start_solid) {
      CHECK(x.contents == y.contents && x.texture == y.texture);
      ++hits;
    }
  }
  CHECK(hits > 100);
}

TEST(a_world_without_six_axial_sides_is_refused) {
  br::BrushWorld w;
  w.models.resize(1);
  br::WorldBrush b = box(0, 0, 0, 10, 10, 10);
  b.sides.pop_back();
  w.models[0].push_back(b);
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK(!br::write_brush_world(w, &bytes, &error));
  CHECK(error.find("next:") != std::string::npos);
}

TEST(seeded_traces_hold_golden) {
  const br::Map m = read_world(world());
  br::BrushTracer t(m);
  core::Rng rng = core::make_rng(20261105u);
  std::string table = "# fraction end-xyz normal-xyz flags (brush_trace_test)\n";
  std::vector<std::vector<uint64_t>> rows;
  const auto bits = [](float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    return uint64_t(u);
  };
  for (int i = 0; i < 64; ++i) {
    float a[3], b[3];
    for (int k = 0; k < 3; ++k) {
      a[k] = float(core::range(rng, k == 2 ? -40 : -520, k == 2 ? 300 : 520));
      b[k] = float(core::range(rng, k == 2 ? -40 : -520, k == 2 ? 300 : 520));
    }
    const bool point = i % 4 == 0;
    const br::Trace r = t.trace(a, b, point ? kZero : kMins, point ? kZero : kMaxs, kPlayerSolid);
    const uint64_t flags =
        (r.all_solid ? 1u : 0u) | (r.start_solid ? 2u : 0u) | (uint64_t(uint32_t(r.contents)) << 2);
    std::vector<uint64_t> row = {
        bits(r.fraction),   bits(r.end[0]),     bits(r.end[1]),     bits(r.end[2]),
        bits(r.plane.n[0]), bits(r.plane.n[1]), bits(r.plane.n[2]), flags};
    char line[160];
    std::snprintf(line, sizeof line, "%08llx %08llx %08llx %08llx %08llx %08llx %08llx %llx\n",
                  static_cast<unsigned long long>(row[0]), static_cast<unsigned long long>(row[1]),
                  static_cast<unsigned long long>(row[2]), static_cast<unsigned long long>(row[3]),
                  static_cast<unsigned long long>(row[4]), static_cast<unsigned long long>(row[5]),
                  static_cast<unsigned long long>(row[6]), static_cast<unsigned long long>(row[7]));
    table += line;
    rows.push_back(row);
  }
  const char* write = std::getenv("GE_GOLDEN_WRITE");
  if (write && *write) {
    std::ofstream(ge::test::root_path(kGolden)) << table;
    std::printf("  wrote %s\n", kGolden);
    return;
  }
  const auto golden = ge::test::golden_rows(kGolden);
  CHECK(golden.size() == rows.size());
  for (size_t r = 0; r < golden.size() && r < rows.size(); ++r) CHECK(golden[r] == rows[r]);
}

}  // namespace
