// A damaged map is refused or read whole, never read out of range: the fixture of every family
// mutated thousands of ways (bytes flipped, the file cut short, words written over the lump
// directory and the records) from a seeded stream, every mutant either refused with a reason and
// a next step, or read into a view whose every index is in range and whose tree a point walk
// leaves, and whose clip hulls a point walk ends in. The sanitizer rows make an out-of-range read
// a failure here. A brush world is mutated too, and every mutant read is traced.
//
// ge:summary Test: thousands of mutated BSP maps are refused or read in range, never overrun
// ge:keywords test brush bsp fuzz mutate damage refuse bounds sanitizer v29 bsp2 v38 v46
// ge:skill brush testing
#include <string>

#include "brush/anim.hpp"
#include "brush/read.hpp"
#include "brush/trace.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "core/prng.hpp"

namespace {

namespace br = ge::brush;
namespace core = ge::core;

void mutate(core::Rng& rng, std::vector<uint8_t>& b) {
  if (b.size() < 8) return;
  switch (core::int_in(rng, 0, 4)) {
    case 0:  // a few bytes flipped anywhere
      for (int k = core::int_in(rng, 1, 8); k > 0; --k) {
        b[core::pick_index(rng, b.size())] ^= uint8_t(core::int_in(rng, 1, 255));
      }
      break;
    case 1:  // cut short
      b.resize(core::pick_index(rng, b.size()));
      break;
    case 2: {  // a word over the lump directory
      const size_t at = 4 + 4 * core::pick_index(rng, 40);
      const auto v = uint32_t(core::int_in(rng, -100000, 100000));
      for (int k = 0; k < 4 && at + size_t(k) < b.size(); ++k)
        b[at + size_t(k)] = uint8_t(v >> (8 * k));
      break;
    }
    case 3: {  // a small word over a record (an index, a count, an offset)
      const size_t at = core::pick_index(rng, b.size() / 4) * 4;
      const auto v = uint32_t(core::int_in(rng, -70000, 70000));
      for (int k = 0; k < 4; ++k) b[at + size_t(k)] = uint8_t(v >> (8 * k));
      break;
    }
    default: {  // a byte set to its extremes
      b[core::pick_index(rng, b.size())] = core::chance(rng, 0.5) ? 0xff : 0x80;
      break;
    }
  }
}

// Every index in the view in range.
bool sound(const br::Map& m) {
  for (uint32_t i : m.indices) {
    if (i >= m.vertices.size()) return false;
  }
  for (const br::Surface& s : m.surfaces) {
    if (s.first > m.indices.size() || s.count > m.indices.size() - s.first) return false;
    if (s.texture >= m.textures.size()) return false;
    if (s.lightmap >= 0) {
      if (size_t(s.lightmap) >= m.lightmaps.size()) return false;
      const br::Lightmap& l = m.lightmaps[size_t(s.lightmap)];
      if (l.offset + uint64_t(l.width) * l.height * 3 > m.light_rgb.size()) return false;
    }
  }
  for (const br::Leaf& l : m.leaves) {
    if (l.first_brush > m.leaf_brushes.size() || l.brushes > m.leaf_brushes.size() - l.first_brush)
      return false;
  }
  for (uint32_t b : m.leaf_brushes) {
    if (b >= m.brushes.size()) return false;
  }
  for (const br::Brush& b : m.brushes) {
    if (b.first_side > m.brush_sides.size() || b.sides > m.brush_sides.size() - b.first_side)
      return false;
  }
  for (const br::BrushSide& s : m.brush_sides) {
    if (s.plane >= m.planes.size() || s.texture >= int32_t(m.textures.size())) return false;
  }
  for (const br::ClipNode& n : m.clip_nodes) {
    if (n.plane >= m.planes.size()) return false;
    for (int32_t c : n.child) {
      if (c >= 0 && size_t(c) >= m.clip_nodes.size()) return false;
    }
  }
  const br::TextureAnims anims = br::build_texture_anims(m);
  for (uint32_t t = 0; t < m.textures.size(); ++t) {
    for (const double at : {0.0, 0.7, 13.3}) {
      if (br::texture_at(anims, t, at, t % 2 == 0, t) >= m.textures.size()) return false;
    }
  }
  return true;
}

}  // namespace

TEST(mutated_maps_are_refused_or_read_in_range) {
  const br::Family families[] = {br::Family::v29, br::Family::bsp2, br::Family::v38,
                                 br::Family::v46};
  core::Rng rng = core::make_rng(20261023u);
  for (br::Family f : families) {
    std::vector<uint8_t> clean;
    std::string error;
    CHECK(br::write_bsp(br::sample_fixture(), f, &clean, &error));
    int read = 0, refused = 0;
    for (int trial = 0; trial < 800; ++trial) {
      std::vector<uint8_t> b = clean;
      for (int k = core::int_in(rng, 1, 3); k > 0 && !b.empty(); --k) mutate(rng, b);
      br::Map m;
      if (!br::read_bsp(b, &m, &error)) {
        ++refused;
        CHECK(error.find("next:") != std::string::npos);
        continue;
      }
      ++read;
      CHECK(sound(m));
      for (int k = 0; k < 16; ++k) {
        const float p[3] = {float(core::range(rng, -200, 1000)),
                            float(core::range(rng, -200, 1000)), float(core::range(rng, -50, 400))};
        const uint32_t leaf = br::leaf_at(m, p, uint32_t(core::pick_index(rng, m.models.size())));
        CHECK(leaf < m.leaves.size());
        br::cluster_sees(m, m.leaves[leaf].cluster, int32_t(core::int_in(rng, -2, 9)));
        // A clip hull's walk ends (a looping hull was refused) on a contents number.
        if (k % 3 && !m.clip_nodes.empty()) CHECK(br::hull_contents(m, p, 0, k % 3) < 0);
      }
      br::build_phs(&m);
      CHECK(m.phs.empty() || m.phs.size() == m.pvs.size());
    }
    // Both outcomes are exercised: a mutant often lands in pixels or luxels and still reads.
    CHECK(read > 50 && refused > 50);
  }
}

// A brush world (version 38 with brushes) mutated the same ways: refused, or read in range and
// traced to an end, every trace's end a number and its contents a brush's.
TEST(mutated_brush_worlds_are_refused_or_traced_in_range) {
  br::BrushWorld w;
  w.models.resize(2);
  const float f0[3] = {-256, -256, -16}, f1[3] = {256, 256, 0};
  const float w0[3] = {64, -256, 0}, w1[3] = {80, 256, 128};
  const float d0[3] = {-64, -16, 0}, d1[3] = {-48, 16, 96};
  w.models[0].push_back(br::box_brush(f0, f1));
  w.models[0].push_back(br::box_brush(w0, w1, 0x20, "world/water"));
  br::WorldBrush ramp = br::box_brush(d0, d1);
  const float n[3] = {1, 0, 1};
  br::add_side(&ramp, n, -20);
  w.models[0].push_back(ramp);
  w.models[1].push_back(br::box_brush(d0, d1, 1, "door/metal"));
  std::vector<uint8_t> clean;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &clean, &error), error);
  core::Rng rng = core::make_rng(20261105u);
  const float mins[3] = {-16, -16, -24}, maxs[3] = {16, 16, 32};
  int read = 0, refused = 0;
  for (int trial = 0; trial < 800; ++trial) {
    std::vector<uint8_t> b = clean;
    for (int k = core::int_in(rng, 1, 3); k > 0 && !b.empty(); --k) mutate(rng, b);
    br::Map m;
    if (!br::read_bsp(b, &m, &error)) {
      ++refused;
      CHECK(error.find("next:") != std::string::npos);
      continue;
    }
    ++read;
    CHECK(sound(m));
    br::BrushTracer t(m);
    for (int k = 0; k < 8; ++k) {
      float a[3], e[3];
      for (int j = 0; j < 3; ++j) {
        a[j] = float(core::range(rng, -300, 300));
        e[j] = k % 4 == 0 ? a[j] : float(core::range(rng, -300, 300));
      }
      const auto model = uint32_t(core::pick_index(rng, m.models.size()));
      const br::Trace r = t.trace(a, e, mins, maxs, -1, model);
      CHECK(r.fraction >= 0 && r.fraction <= 1);
      CHECK(r.texture < int32_t(m.textures.size()));
      t.contents(a, model);
    }
  }
  CHECK(read > 50 && refused > 50);
}
