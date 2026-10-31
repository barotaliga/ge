// A classic map's own areas of fog: the placed volumes' keys, the liquids' hazes, the sky's share
// by rays straight up through the scene's faces, and the sun map's block for the fog.
//
// ge:summary Definitions for a classic map's fog: entity keys, the liquids' hazes, the sky share
// ge:keywords bsp brush fog fog-volume fog_volume lava-haze slime-haze sky-share sun-map
// ge:skill brush render
// ge:test tests/brush_bsp_fog_test.cpp
#include "brush/app/bsp_fog.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "render/frame.hpp"
#include "render/mist_fog.hpp"

namespace ge::brush::app {

namespace r = ge::render;

namespace {

// Up to `n` numbers off `text`; how many it read.
int numbers(const std::string& text, double* out, int n) {
  const char* p = text.c_str();
  int k = 0;
  while (k < n) {
    char* end = nullptr;
    const double v = std::strtod(p, &end);
    if (end == p) break;
    out[k++] = v;
    p = end;
  }
  return k;
}

// The key `key` as one number, or `fallback`.
double number(const Entity& e, const char* key, double fallback) {
  double v = fallback;
  return numbers(e.get(key), &v, 1) == 1 ? v : fallback;
}

// The key `key` as three numbers into `out`, kept as they were unless it holds three.
void triple(const Entity& e, const char* key, double out[3]) {
  double v[3];
  if (numbers(e.get(key), v, 3) == 3)
    for (int k = 0; k < 3; ++k) out[k] = v[k];
}

// The box from file corners `lo` .. `hi` into the volume's centre and half sizes (engine m).
void box_of(const float lo[3], const float hi[3], r::FogVolume& v) {
  double a[3], b[3];
  to_engine(lo, a);
  to_engine(hi, b);
  for (int k = 0; k < 3; ++k) {
    v.centre[k] = (a[k] + b[k]) / 2;
    v.half[k] = std::fabs(b[k] - a[k]) / 2;
  }
}

}  // namespace

bool fog_volume_of(const Map& map, const Entity& e, r::FogVolume* out) {
  if (e.get("classname") != "fog_volume") return false;
  r::FogVolume v;
  const std::string& model = e.get("model");
  if (model.size() > 1 && model[0] == '*') {
    const long m = std::strtol(model.c_str() + 1, nullptr, 10);
    if (m <= 0 || size_t(m) >= map.models.size()) return false;
    const Model& b = map.models[size_t(m)];
    box_of(b.mins, b.maxs, v);
  } else {
    double origin[3], size[3];
    if (numbers(e.get("origin"), origin, 3) != 3 || numbers(e.get("size"), size, 3) != 3)
      return false;
    float lo[3], hi[3];
    for (int k = 0; k < 3; ++k) {
      lo[k] = float(origin[k] - std::fabs(size[k]) / 2);
      hi[k] = float(origin[k] + std::fabs(size[k]) / 2);
    }
    box_of(lo, hi, v);
  }
  if (v.half[0] <= 0 || v.half[1] <= 0 || v.half[2] <= 0) return false;
  // Indoors the defaults are a room's: a soft edge of a hand, a noise of a few metres.
  v.edge = 0.5;
  v.noise_size = 4;
  v.density = std::max(number(e, "density", v.density), 0.0);
  v.falloff = std::max(number(e, "falloff", 0), 0.0);
  v.edge = std::max(number(e, "edge", v.edge), 0.0);
  v.phase = std::clamp(number(e, "phase", v.phase), -0.95, 0.95);
  v.noise = std::clamp(number(e, "noise", v.noise), 0.0, 1.0);
  v.noise_size = std::max(number(e, "noise_size", v.noise_size), 0.01);
  v.flicker = float(std::clamp(number(e, "flicker", 0), 0.0, 1.0));
  v.sky = float(std::clamp(number(e, "sky", -1), -1.0, 1.0));  // -1: measure it
  triple(e, "albedo", v.albedo);
  triple(e, "glow", v.glow);
  double wind[3] = {0, 0, 0};
  triple(e, "wind", wind);
  // The map's axes (z up) to the engine's (y up), m/s as written.
  v.wind[0] = wind[0];
  v.wind[1] = wind[2];
  v.wind[2] = wind[1];
  *out = v;
  return true;
}

double sky_share(const Scene& s, const r::FogVolume& v) {
  // The floor's points, a little over it; each ray straight up meets the scene's faces.
  const double y0 = v.centre[1] - v.half[1] + 0.1;
  int seen = 0, open = 0;
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 8; ++j) {
      const double x = v.centre[0] + ((i + 0.5) / 8 * 2 - 1) * v.half[0];
      const double z = v.centre[2] + ((j + 0.5) / 8 * 2 - 1) * v.half[2];
      double best = 1e30;
      int kind = -1;
      for (size_t t = 0; t + 2 < s.indices.size(); t += 3) {
        const DrawVertex& a = s.vertices[s.indices[t]];
        if (a.kind != kSolid && a.kind != kSky) continue;
        const DrawVertex& b = s.vertices[s.indices[t + 1]];
        const DrawVertex& c = s.vertices[s.indices[t + 2]];
        // The point in the triangle's plan (x, z) by its barycentric weights.
        const double d = (b.pos[2] - c.pos[2]) * (a.pos[0] - c.pos[0]) +
                         (c.pos[0] - b.pos[0]) * (a.pos[2] - c.pos[2]);
        if (std::fabs(d) < 1e-12) continue;
        const double wa =
            ((b.pos[2] - c.pos[2]) * (x - c.pos[0]) + (c.pos[0] - b.pos[0]) * (z - c.pos[2])) / d;
        const double wb =
            ((c.pos[2] - a.pos[2]) * (x - c.pos[0]) + (a.pos[0] - c.pos[0]) * (z - c.pos[2])) / d;
        const double wc = 1 - wa - wb;
        if (wa < 0 || wb < 0 || wc < 0) continue;
        const double y = wa * a.pos[1] + wb * b.pos[1] + wc * c.pos[1];
        if (y > y0 && y < best) {
          best = y;
          kind = int(a.kind);
        }
      }
      ++seen;
      if (kind != kSolid) ++open;
    }
  }
  return seen > 0 ? double(open) / seen : 1.0;
}

std::vector<r::FogVolume> bsp_fog(const Scene& s, const FogRows& rows) {
  std::vector<r::FogVolume> out;
  if (rows.placed) {
    for (const Entity& e : s.map.entities) {
      r::FogVolume v;
      if (!fog_volume_of(s.map, e, &v)) continue;
      if (v.sky < 0) v.sky = float(sky_share(s, v));
      out.push_back(v);
    }
  }
  for (const LiquidPatch& p : s.liquids) {
    const bool lava = p.kind == kLava && rows.lava;
    const bool slime = p.kind == kSlime && rows.slime;
    if (!lava && !slime) continue;
    r::FogVolume v;
    const double h = lava ? rows.lava_height : rows.slime_height;
    v.centre[0] = (p.box[0] + p.box[2]) / 2;
    v.centre[1] = p.level + h / 2;
    v.centre[2] = (p.box[1] + p.box[3]) / 2;
    v.half[0] = (p.box[2] - p.box[0]) / 2;
    v.half[1] = h / 2;
    v.half[2] = (p.box[3] - p.box[1]) / 2;
    v.edge = std::min({0.4, v.half[0], v.half[2]});
    v.noise = 0.55;
    v.noise_size = 2;
    v.phase = 0.2;
    if (lava) {
      // A dark smoke over the melt, lit by its own glow and rising on its heat.
      v.density = rows.lava_density;
      v.falloff = rows.lava_falloff;
      v.albedo[0] = 0.5, v.albedo[1] = 0.42, v.albedo[2] = 0.36;
      for (int k = 0; k < 3; ++k) v.glow[k] = rows.lava_glow[k];
      v.wind[1] = 0.5;
      v.flicker = rows.lava_flicker;
      v.rate_hz = 3;
      v.seed = uint32_t(out.size());
    } else {
      v.density = rows.slime_density;
      v.falloff = 0.8;
      v.albedo[0] = 0.6, v.albedo[1] = 0.75, v.albedo[2] = 0.55;
      for (int k = 0; k < 3; ++k) v.glow[k] = rows.slime_glow[k];
      v.wind[1] = 0.1;
    }
    v.sky = float(sky_share(s, v));
    out.push_back(v);
  }
  if (rows.air != nullptr) {
    for (const LiquidPatch& p : s.liquids) {
      if (p.kind != kWater && p.kind != kSlime) continue;
      // The pool's sky share by its own floor: a probe as tall as a room over its surface.
      r::FogVolume probe;
      probe.centre[0] = (p.box[0] + p.box[2]) / 2;
      probe.centre[2] = (p.box[1] + p.box[3]) / 2;
      probe.half[0] = (p.box[2] - p.box[0]) / 2;
      probe.half[2] = (p.box[3] - p.box[1]) / 2;
      probe.half[1] = 1;
      probe.centre[1] = p.level + 1;
      const double sky = sky_share(s, probe);
      const wind::Mist mist = pool_mist(p, *rows.air, rows.hour, sky);
      if (mist.kind == wind::MistKind::none) continue;
      r::MistPlace at;
      at.centre[0] = probe.centre[0];
      at.centre[1] = probe.centre[2];
      at.half[0] = probe.half[0];
      at.half[1] = probe.half[2];
      at.floor_y = p.level;
      at.drift[0] = std::max(rows.air->wind, 0.0) * sky;
      r::FogVolume v = r::mist_volume(mist, at);
      v.sky = float(sky);
      v.seed = uint32_t(out.size());
      out.push_back(v);
    }
  }
  return out;
}

wind::Mist pool_mist(const LiquidPatch& pool, const wind::Climate& air, double hour, double sky) {
  if (pool.kind != kWater && pool.kind != kSlime) return {};
  // On the surface, over open water, as wet as ground can be.
  wind::Site site;
  site.y = pool.level;
  site.above = 1;
  site.water_m = 0;
  site.wet = 1;
  site.sky = std::clamp(sky, 0.0, 1.0);
  return wind::mist_at(air, site, hour);
}

r::OrthoShadow bsp_sun_map(const BspDraw& d) {
  r::OrthoShadow m;
  const GpuJob& j = d.staged.ortho[0];
  for (int k = 0; k < 3; ++k) {
    m.o[k] = j.o[k];
    m.x[k] = j.x[k];
    m.y[k] = j.y[k];
    m.z[k] = j.z[k];
  }
  m.half = j.o[3];
  m.depth = j.info[1];
  m.size = kOrthoSize;
  m.bias = -0.0015;  // bsp.wgsl's own, against the faces' acne
  return m;
}

void create_bsp_fog_shadow(gpu::Device& device, BspFogShadow& s, const BspDraw& d) {
  s.params =
      r::make_buffer(device, "bsp.fog.shadow", sizeof(r::ShadowParams), wgpu::BufferUsage::Uniform);
  const r::ShadowParams none{};
  device.queue.WriteBuffer(s.params, 0, &none, sizeof none);
  s.group = r::make_group(device, "bsp.fog.shadow", r::sun_shadow_layout(device),
                          {s.params, d.orthos_view, d.compare});
}

void prepare_bsp_fog_shadow(gpu::Device& device, BspFogShadow& s, const BspDraw& d,
                            const r::FrameView& view) {
  // Nothing casts until the map is drawn (the sun's map is drawn when the renderer needs it).
  const r::ShadowParams p =
      d.ortho_held[0] ? r::ortho_shadow_params(bsp_sun_map(d), view.eye) : r::ShadowParams{};
  device.queue.WriteBuffer(s.params, 0, &p, sizeof p);
}

}  // namespace ge::brush::app
