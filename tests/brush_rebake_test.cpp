// plugins/brush/rebake.hpp: the sample fixture's lit faces are charted and rebaked (every lit
// vertex placed in the atlas, its luxels lit by the fixture's lights), sky faces and liquids are
// not charted, RGB9E5 keeps a colour to its 9-bit precision, and the cache's bytes read back to
// the same rebake and refuse a damaged file.
//
// ge:summary Test: a brush map rebaked: charts, light, RGB9E5, the cache's codec
// ge:keywords test bsp brush rebake bake rgb9e5 cache codec rebake_map write_rebaked
// ge:skill testing brush
#include "brush/rebake.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"

namespace {

using namespace ge::brush;

Map fixture_map() {
  std::vector<uint8_t> bytes;
  std::string err;
  CHECK(write_bsp(sample_fixture(), Family::v29, &bytes, &err));
  Map m;
  CHECK(read_bsp(bytes, &m, &err));
  return m;
}

// The fixture's lights as a bake's: each `light` entity's origin, a classic value's strength.
std::vector<ge::bake::Light> lights_of(const Map& m) {
  std::vector<ge::bake::Light> out;
  for (const Entity& e : m.entities) {
    if (e.get("classname") != "light") continue;
    float p[3] = {0, 0, 0};
    std::sscanf(e.get("origin").c_str(), "%f %f %f", &p[0], &p[1], &p[2]);
    ge::bake::Light l;
    to_engine(p, l.pos);
    l.colour[0] = l.colour[1] = l.colour[2] = 4;
    l.radius = 0.2;
    out.push_back(l);
  }
  return out;
}

}  // namespace

// Every planar face's vertices land in the atlas; sky faces and liquids stay uncharted; the
// fixture's lights light the rebake.
TEST(rebake_fixture) {
  const Map m = fixture_map();
  RebakeInput in;
  in.lights = lights_of(m);
  CHECK(!in.lights.empty());
  RebakeOptions o;
  o.atlas_width = 1024;
  o.bake.samples = 8;
  o.bake.bounces = 1;
  Rebaked out;
  std::string err;
  CHECK(rebake_map(m, in, o, &out, &err));
  CHECK(out.width == 1024 && out.height > 0);
  CHECK(out.lm.size() == m.vertices.size() * 2);
  CHECK(out.stats.luxels > 0);
  int planar = 0;
  for (const Surface& s : m.surfaces) {
    for (uint32_t i = s.first; i < s.first + s.count; i++) {
      const float u = out.lm[size_t{m.indices[i]} * 2], v = out.lm[size_t{m.indices[i]} * 2 + 1];
      if (s.kind == SurfaceKind::planar) {
        CHECK(u > 0 && u < 1 && v > 0 && v < 1);
        planar++;
      } else {
        CHECK(u == -1.0f);
      }
    }
  }
  CHECK(planar > 0);
  float lit = 0;
  for (const uint32_t p : out.rgb9e5) {
    float c[3];
    unpack_rgb9e5(p, c);
    lit = std::max(lit, c[1]);
  }
  CHECK(lit > 0.1f);
  // The bounce is the irradiance less the direct light: never more than it, and some of it lit.
  CHECK(out.bounce.size() == out.rgb9e5.size());
  float bounced = 0;
  for (size_t i = 0; i < out.bounce.size(); i++) {
    float c[3], b[3];
    unpack_rgb9e5(out.rgb9e5[i], c);
    unpack_rgb9e5(out.bounce[i], b);
    for (int k = 0; k < 3; k++) CHECK(b[k] <= c[k] * 1.01f + 1e-6f);
    bounced = std::max(bounced, b[1]);
  }
  CHECK(bounced > 0);
  CHECK(bounced < lit);
}

// With no gather pass there is no bounce.
TEST(rebake_direct_only_has_no_bounce) {
  const Map m = fixture_map();
  RebakeInput in;
  in.lights = lights_of(m);
  RebakeOptions o;
  o.atlas_width = 1024;
  o.bake.samples = 8;
  o.bake.bounces = 0;
  Rebaked out;
  std::string err;
  CHECK(rebake_map(m, in, o, &out, &err));
  for (const uint32_t p : out.bounce) {
    float b[3];
    unpack_rgb9e5(p, b);
    CHECK(b[0] == 0 && b[1] == 0 && b[2] == 0);
  }
}

// RGB9E5 keeps each channel within half a step of the largest channel's precision.
TEST(rebake_rgb9e5) {
  const float cases[][3] = {
      {0, 0, 0}, {1, 0.5f, 0.25f}, {0.003f, 0.001f, 0.0f}, {37.5f, 12, 0.1f}, {70000, 1, 1}};
  for (const auto& c : cases) {
    float back[3];
    unpack_rgb9e5(pack_rgb9e5(c), back);
    const float most = std::max(std::min(c[0], 65408.0f), std::max(c[1], c[2]));
    for (int k = 0; k < 3; k++) {
      CHECK(std::fabs(back[k] - std::min(c[k], 65408.0f)) <= most / 512 + 1e-9f);
    }
  }
}

// The cache's bytes read back to the same rebake; a cut file and another magic are refused.
TEST(rebake_codec) {
  Rebaked r;
  r.width = 3;
  r.height = 2;
  for (uint32_t i = 0; i < 6; i++) r.rgb9e5.push_back(i * 0x01020304u);
  for (uint32_t i = 0; i < 24; i++) r.dir.push_back(uint8_t(i * 7));
  r.lm = {0.25f, 0.5f, -1, -1};
  const std::vector<uint8_t> bytes = write_rebaked(r);
  Rebaked back;
  std::string err;
  CHECK(read_rebaked(bytes, &back, &err));
  CHECK(back.width == 3 && back.height == 2);
  CHECK(back.rgb9e5 == r.rgb9e5);
  CHECK(back.dir == r.dir);
  CHECK(back.lm == r.lm);
  CHECK(back.bounce == std::vector<uint32_t>(6, 0));
  r.bounce = {9, 8, 7, 6, 5, 4};
  CHECK(read_rebaked(write_rebaked(r), &back, &err));
  CHECK(back.bounce == r.bounce);
  // A version 1 file (no bounce) still reads, with no bounce.
  std::vector<uint8_t> v1 = write_rebaked(r);
  v1[4] = 1;
  v1.erase(v1.begin() + 20 + std::ptrdiff_t{48}, v1.begin() + 20 + std::ptrdiff_t{72});
  CHECK(read_rebaked(v1, &back, &err));
  CHECK(back.rgb9e5 == r.rgb9e5 && back.dir == r.dir && back.lm == r.lm && back.bounce.empty());
  std::vector<uint8_t> cut(bytes.begin(), bytes.end() - 1);
  CHECK(!read_rebaked(cut, &back, &err));
  CHECK(err.find("rebake") != std::string::npos);
  std::vector<uint8_t> other = bytes;
  other[0] ^= 1;
  CHECK(!read_rebaked(other, &back, &err));
}
