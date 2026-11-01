// The map turned into a bake's scene (its faces charted one a chart, its sky faces and liquids as
// materials), the bake run, and the cache's codec.
//
// ge:summary Defines rebake_map (map to bake scene, charts, the bake) and the rebake cache codec
// ge:keywords bsp brush rebake bake scene chart rgb9e5 cache codec
// ge:skill brush render
// ge:test tests/brush_rebake_test.cpp
#include "brush/rebake.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "bake/chart.hpp"

namespace ge::brush {

namespace {

constexpr uint32_t kMagic = 0x42524547u;  // "GERB" little-endian
constexpr uint32_t kVersion = 2;          // 1: no bounce

// A liquid's surface (its upward face) over the plan: x and z bounds, m, and its level.
struct Pool {
  double lo[2] = {1e30, 1e30}, hi[2] = {-1e30, -1e30};
  double level = 0;
};
constexpr double kPoolMargin = 0.5;  // m: a light this far outside a pool's bounds is still in it
constexpr double kPoolDepth = 3.0;   // m: how deep under a surface a sunk light is looked for
constexpr double kPoolLift = 0.25;   // m: a sunk light's height over its pool once raised
constexpr double kPoolSpread = 0.6;  // m: its size once raised (the surface spreads its light)

// How a liquid's surface glows, a share of its texture's colour, by the reader's mark.
double glow_of(Liquid l) {
  switch (l) {
    case Liquid::lava:
      return 8.0;
    case Liquid::slime:
      return 0.35;
    case Liquid::teleport:
      return 0.6;
    case Liquid::none:
    case Liquid::water:
    case Liquid::other:
      break;
  }
  return 0;
}

void put32(std::vector<uint8_t>& b, uint32_t v) {
  for (int k = 0; k < 4; k++) b.push_back(static_cast<uint8_t>(v >> (8 * k)));
}

uint32_t get32(const uint8_t* p) {
  return uint32_t{p[0]} | uint32_t{p[1]} << 8 | uint32_t{p[2]} << 16 | uint32_t{p[3]} << 24;
}

// The bake's front is the side its triangle turns counter-clockwise to; the file winds the other
// way once its axes are the engine's, so each triangle is turned to face its vertices' normal.
void face_front(const Map& map, bake::Scene& scene, uint32_t i) {
  uint32_t* t = &scene.indices[i];
  const float* a = &scene.positions[size_t{t[0]} * 3];
  const float* b = &scene.positions[size_t{t[1]} * 3];
  const float* c = &scene.positions[size_t{t[2]} * 3];
  const double e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
  const double e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
  const double x[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                       e1[0] * e2[1] - e1[1] * e2[0]};
  double n[3];
  to_engine(map.vertices[t[0]].nrm, n);
  if (x[0] * n[0] + x[1] * n[1] + x[2] * n[2] < 0) std::swap(t[1], t[2]);
}

}  // namespace

uint32_t pack_rgb9e5(const float rgb[3]) {
  constexpr double kMax = 65408.0;  // (511 / 512) x 2^16
  double c[3];
  for (int k = 0; k < 3; k++) c[k] = std::clamp(static_cast<double>(rgb[k]), 0.0, kMax);
  if (!(c[0] == c[0])) c[0] = 0;  // NaN
  if (!(c[1] == c[1])) c[1] = 0;
  if (!(c[2] == c[2])) c[2] = 0;
  const double most = std::max(c[0], std::max(c[1], c[2]));
  int e = 0;
  std::frexp(most, &e);  // most = m 2^e, m in [0.5, 1): floor(log2 most) = e - 1
  int shared = most > 0 ? std::max(-16, e - 1) + 16 : 0;
  double denom = std::ldexp(1.0, shared - 15 - 9);
  if (std::floor(most / denom + 0.5) >= 512) {
    denom *= 2;
    shared++;
  }
  uint32_t out = static_cast<uint32_t>(shared) << 27;
  for (int k = 0; k < 3; k++) {
    const auto m = static_cast<uint32_t>(std::min(511.0, std::floor(c[k] / denom + 0.5)));
    out |= m << (9 * k);
  }
  return out;
}

void unpack_rgb9e5(uint32_t packed, float rgb[3]) {
  const double scale = std::ldexp(1.0, static_cast<int>(packed >> 27) - 15 - 9);
  for (int k = 0; k < 3; k++) rgb[k] = static_cast<float>(((packed >> (9 * k)) & 511u) * scale);
}

bool rebake_map(const Map& map, const RebakeInput& input, const RebakeOptions& options,
                Rebaked* out, std::string* error) {
  bake::Scene scene;
  scene.positions.reserve(map.vertices.size() * 3);
  for (const Vertex& v : map.vertices) {
    double p[3];
    to_engine(v.pos, p);
    for (int k = 0; k < 3; k++) scene.positions.push_back(static_cast<float>(p[k]));
  }
  scene.indices = map.indices;
  scene.tri_material.assign(scene.triangles(), 0);
  scene.tri_chart.assign(scene.triangles(), bake::kNoChart);
  // One material a texture, as it is drawn (sky, liquid or a lit face).
  for (size_t t = 0; t < map.textures.size(); t++) {
    bake::Material m;
    if (t < input.albedo.size()) {
      for (int k = 0; k < 3; k++) m.albedo[k] = input.albedo[t][k];
    }
    scene.materials.push_back(m);
  }
  if (scene.materials.empty()) scene.materials.push_back({});
  constexpr uint32_t kNone = ~0u;
  std::vector<uint32_t> below(scene.materials.size(), kNone);  // a liquid's dark underside
  std::vector<Pool> pools;
  std::vector<uint32_t> tris;
  std::vector<uint32_t> chart_of(map.surfaces.size(), bake::kNoChart);
  for (size_t si = 0; si < map.surfaces.size(); si++) {
    const Surface& s = map.surfaces[si];
    const uint32_t mat = s.texture < scene.materials.size() ? s.texture : 0;
    bake::Material& m = scene.materials[mat];
    tris.clear();
    for (uint32_t i = s.first; i + 3 <= s.first + s.count; i += 3) {
      scene.tri_material[i / 3] = mat;
      tris.push_back(i / 3);
      face_front(map, scene, i);
    }
    if (s.kind == SurfaceKind::sky) {
      m.sky = true;
      continue;
    }
    if (s.kind == SurfaceKind::liquid) {
      // A liquid glows up out of its surface only: the face a pool shows from beneath lights
      // nothing, or its glow reaches every wall under every pool it is joined to.
      double n[3] = {0, 0, 0};
      if (s.count >= 3) to_engine(map.vertices[map.indices[s.first]].nrm, n);
      if (n[1] < 0) {
        if (below[mat] == kNone) {
          below[mat] = static_cast<uint32_t>(scene.materials.size());
          bake::Material dark = scene.materials[mat];
          for (float& e : dark.emit) e = 0;
          scene.materials.push_back(dark);
        }
        for (const uint32_t t : tris) scene.tri_material[t] = below[mat];
        continue;
      }
      const double g = mat < map.textures.size() ? glow_of(map.textures[mat].liquid) : 0;
      for (int k = 0; k < 3; k++) m.emit[k] = static_cast<float>(m.albedo[k] * g);
      if (n[1] > 0 && !tris.empty()) {
        Pool pool;
        for (const uint32_t t : tris) {
          for (int c = 0; c < 3; c++) {
            const float* q = &scene.positions[size_t{scene.indices[t * 3 + c]} * 3];
            pool.lo[0] = std::min(pool.lo[0], double{q[0]});
            pool.lo[1] = std::min(pool.lo[1], double{q[2]});
            pool.hi[0] = std::max(pool.hi[0], double{q[0]});
            pool.hi[1] = std::max(pool.hi[1], double{q[2]});
            pool.level = q[1];
          }
        }
        pools.push_back(pool);
      }
      continue;
    }
    if (tris.empty() || s.kind != SurfaceKind::planar) continue;
    chart_of[si] = bake::add_chart(scene, tris, options.luxel_m);
  }
  scene.lights = input.lights;
  // A map's maker sinks lights in a pool to make it glow, and the classic compile lets their light
  // through the surface. The bake's liquids are solid, so such a light is raised to just above its
  // pool and widened, lighting the room over the pool as it was meant to (and not the walls under
  // it, whose light would bleed up over the waterline as a bright band).
  for (bake::Light& l : scene.lights) {
    if (l.kind == bake::LightKind::sun) continue;
    for (const Pool& p : pools) {
      if (l.pos[0] < p.lo[0] - kPoolMargin || l.pos[0] > p.hi[0] + kPoolMargin ||
          l.pos[2] < p.lo[1] - kPoolMargin || l.pos[2] > p.hi[1] + kPoolMargin)
        continue;
      if (l.pos[1] >= p.level || l.pos[1] < p.level - kPoolDepth) continue;
      l.pos[1] = p.level + kPoolLift;
      l.radius = std::max(l.radius, kPoolSpread);
      break;
    }
  }
  for (int k = 0; k < 3; k++) scene.sky[k] = input.sky[k];
  if (!bake::pack_charts(scene, options.atlas_width)) {
    *error = "the rebake's charts do not fit an atlas " + std::to_string(options.atlas_width) +
             " luxels wide; pass a wider atlas or a coarser luxel (RebakeOptions). Skill: brush.";
    return false;
  }
  const bake::Lightmap lm = bake::bake_scene(scene, options.bake, &out->stats);
  out->width = lm.width;
  out->height = lm.height;
  const size_t n = size_t{lm.width} * lm.height;
  out->rgb9e5.resize(n);
  out->bounce.resize(n);
  out->dir.resize(n * 4);
  for (size_t i = 0; i < n; i++) {
    out->rgb9e5[i] = pack_rgb9e5(&lm.rgb[i * 3]);
    float b[3];
    for (int k = 0; k < 3; k++) b[k] = std::max(lm.rgb[i * 3 + k] - lm.direct[i * 3 + k], 0.0f);
    out->bounce[i] = pack_rgb9e5(b);
    for (int k = 0; k < 3; k++) {
      const double v = std::clamp(static_cast<double>(lm.dir[i * 4 + k]), -1.0, 1.0);
      out->dir[i * 4 + k] = static_cast<uint8_t>(std::floor(v * 127.5 + 128.0));  // -1..1 to 0..255
    }
    out->dir[i * 4 + 3] = lm.dir[i * 4 + 3] > 0 ? 255 : 0;
  }
  out->lm.assign(map.vertices.size() * 2, -1.0f);
  for (size_t si = 0; si < map.surfaces.size(); si++) {
    if (chart_of[si] == bake::kNoChart) continue;
    const bake::Chart& ch = scene.charts[chart_of[si]];
    const Surface& s = map.surfaces[si];
    for (uint32_t i = s.first; i < s.first + s.count; i++) {
      const uint32_t v = map.indices[i];
      const double p[3] = {scene.positions[size_t{v} * 3], scene.positions[size_t{v} * 3 + 1],
                           scene.positions[size_t{v} * 3 + 2]};
      bake::chart_uv(ch, lm.width, lm.height, p, &out->lm[size_t{v} * 2]);
    }
  }
  return true;
}

std::vector<uint8_t> write_rebaked(const Rebaked& baked) {
  std::vector<uint8_t> b;
  const size_t luxels = baked.rgb9e5.size();
  b.reserve(20 + luxels * 12 + baked.lm.size() * 4);
  put32(b, kMagic);
  put32(b, kVersion);
  put32(b, baked.width);
  put32(b, baked.height);
  put32(b, static_cast<uint32_t>(baked.lm.size() / 2));
  for (const uint32_t v : baked.rgb9e5) put32(b, v);
  b.insert(b.end(), baked.dir.begin(), baked.dir.end());
  for (size_t i = 0; i < luxels; i++) put32(b, i < baked.bounce.size() ? baked.bounce[i] : 0);
  for (const float f : baked.lm) {
    uint32_t u = 0;
    std::memcpy(&u, &f, 4);
    put32(b, u);
  }
  return b;
}

bool read_rebaked(std::span<const uint8_t> bytes, Rebaked* out, std::string* error) {
  auto refuse = [&](const std::string& what) {
    *error =
        "rebaked lightmap cache: " + what + "; delete the cache file and rebake. Skill: brush.";
    return false;
  };
  if (bytes.size() < 20 || get32(bytes.data()) != kMagic) return refuse("not a rebake file");
  const uint32_t version = get32(bytes.data() + 4);
  if (version != 1 && version != kVersion) return refuse("written by another version");
  const uint32_t w = get32(bytes.data() + 8), h = get32(bytes.data() + 12);
  const uint32_t verts = get32(bytes.data() + 16);
  const uint64_t luxels = uint64_t{w} * h;
  const uint64_t per = version == 1 ? 8 : 12;
  if (w > 16384 || h > 16384 || bytes.size() != 20 + luxels * per + uint64_t{verts} * 8) {
    return refuse("its size does not match its header");
  }
  out->width = w;
  out->height = h;
  out->rgb9e5.resize(luxels);
  const uint8_t* p = bytes.data() + 20;
  for (size_t i = 0; i < luxels; i++, p += 4) out->rgb9e5[i] = get32(p);
  out->dir.assign(p, p + luxels * 4);
  p += luxels * 4;
  out->bounce.clear();
  if (version >= 2) {
    out->bounce.resize(luxels);
    for (size_t i = 0; i < luxels; i++, p += 4) out->bounce[i] = get32(p);
  }
  out->lm.resize(size_t{verts} * 2);
  for (float& f : out->lm) {
    const uint32_t u = get32(p);
    std::memcpy(&f, &u, 4);
    p += 4;
  }
  return true;
}

}  // namespace ge::brush
