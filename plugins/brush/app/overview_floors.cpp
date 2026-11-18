// A classic BSP map seen from above (overview_floors.hpp).
//
// ge:summary A brush map's top floor a pixel in its texture and light, for the overview
// ge:keywords overview map-floors top-floor liquid
// ge:skill brush map-design
#include "brush/app/overview_floors.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "brush/map.hpp"

namespace ge::brush::app {

namespace {
namespace ov = ge::overview;
}  // namespace

namespace {

// A texel of the atlas's level `mip` at the vertex's texture rectangle (mip 0 texels).
void texel(const Atlas& a, int mip, const float rect[4], double s, double t, float out[3]) {
  if (a.levels.empty()) {
    out[0] = out[1] = out[2] = 0.5f;
    return;
  }
  mip = std::min<int>(mip, int(a.levels.size()) - 1);
  const double k = 1.0 / double(1 << mip);
  const uint32_t aw = std::max<uint32_t>(1, a.width >> mip);
  const uint32_t ah = std::max<uint32_t>(1, a.height >> mip);
  const double rw = std::max(1.0, rect[2] * k), rh = std::max(1.0, rect[3] * k);
  const auto x = uint32_t(rect[0] * k + ov::wrap(s * k, rw));
  const auto y = uint32_t(rect[1] * k + ov::wrap(t * k, rh));
  const uint8_t* p =
      &a.levels[size_t(mip)][(size_t(std::min(y, ah - 1)) * aw + std::min(x, aw - 1)) * 4];
  out[0] = p[0] / 255.0f;
  out[1] = p[1] / 255.0f;
  out[2] = p[2] / 255.0f;
}

// The light at the lightmap atlas's uv, bilinear, 0..~2 (128 the light as baked).
void light_at(const Atlas& a, double u, double v, float out[3]) {
  if (a.levels.empty() || a.width == 0) {
    out[0] = out[1] = out[2] = 1;
    return;
  }
  const double x = u * a.width - 0.5, y = v * a.height - 0.5;
  const int x0 = std::clamp(int(std::floor(x)), 0, int(a.width) - 1);
  const int y0 = std::clamp(int(std::floor(y)), 0, int(a.height) - 1);
  const int x1 = std::min(x0 + 1, int(a.width) - 1), y1 = std::min(y0 + 1, int(a.height) - 1);
  const double fx = std::clamp(x - x0, 0.0, 1.0), fy = std::clamp(y - y0, 0.0, 1.0);
  const auto& L = a.levels[0];
  for (int c = 0; c < 3; ++c) {
    const auto at = [&](int xx, int yy) {
      return L[(size_t(yy) * a.width + size_t(xx)) * 4 + size_t(c)];
    };
    const double top = at(x0, y0) * (1 - fx) + at(x1, y0) * fx;
    const double bot = at(x0, y1) * (1 - fx) + at(x1, y1) * fx;
    out[c] = float((top * (1 - fy) + bot * fy) / 128.0);
  }
}

}  // namespace

ov::Floors overview_floors(const Scene& scene, const ov::Frame& fr, int ss) {
  ov::Floors f = ov::empty_floors(fr, ss);
  // The texture's level: a texel a unit, `scale * ss` samples a unit.
  const double per = fr.scale * ss;
  int mip = 0;
  while (mip < 3 && per * (1 << (mip + 1)) <= 1.0) ++mip;
  const double k = 1 / kUnitM;
  for (size_t i = 0; i + 2 < scene.indices.size(); i += 3) {
    const DrawVertex* v[3] = {&scene.vertices[scene.indices[i]],
                              &scene.vertices[scene.indices[i + 1]],
                              &scene.vertices[scene.indices[i + 2]]};
    const uint32_t kind = v[0]->kind;
    if (kind == kSky || kind == kHidden) continue;
    if (v[0]->nrm[1] + v[1]->nrm[1] + v[2]->nrm[1] < 0.45) continue;  // walls and ceilings
    const bool liquid = kind == kWater || kind == kSlime || kind == kLava;
    double sx[3], sy[3], z[3];
    for (int j = 0; j < 3; ++j) {
      sx[j] = double(fr.px(v[j]->pos[0] * k)) * ss;
      sy[j] = double(fr.py(v[j]->pos[2] * k)) * ss;
      z[j] = v[j]->pos[1] * k;
    }
    const double area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
    if (std::fabs(area) < 1e-9) continue;
    const int x0 = std::max(0, int(std::floor(std::min({sx[0], sx[1], sx[2]}))));
    const int x1 = std::min(f.w - 1, int(std::ceil(std::max({sx[0], sx[1], sx[2]}))));
    const int y0 = std::max(0, int(std::floor(std::min({sy[0], sy[1], sy[2]}))));
    const int y1 = std::min(f.h - 1, int(std::ceil(std::max({sy[0], sy[1], sy[2]}))));
    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) {
        const double px = x + 0.5, py = y + 0.5;
        double b[3];
        b[0] = ((sx[1] - px) * (sy[2] - py) - (sx[2] - px) * (sy[1] - py)) / area;
        b[1] = ((sx[2] - px) * (sy[0] - py) - (sx[0] - px) * (sy[2] - py)) / area;
        b[2] = 1 - b[0] - b[1];
        if (b[0] < -1e-6 || b[1] < -1e-6 || b[2] < -1e-6) continue;
        const auto h = float(b[0] * z[0] + b[1] * z[1] + b[2] * z[2]);
        const size_t at = size_t(y) * size_t(f.w) + size_t(x);
        float* top = liquid ? &f.wet[at] : &f.top[at];
        if (h <= *top) continue;
        *top = h;
        const double s = b[0] * v[0]->st[0] + b[1] * v[1]->st[0] + b[2] * v[2]->st[0];
        const double t = b[0] * v[0]->st[1] + b[1] * v[1]->st[1] + b[2] * v[2]->st[1];
        float c[3], l[3];
        texel(scene.textures, mip, v[0]->rect, s, t, c);
        if (liquid || kind == kTele) {
          l[0] = l[1] = l[2] = 1;
        } else {
          light_at(scene.light, b[0] * v[0]->lm[0] + b[1] * v[1]->lm[0] + b[2] * v[2]->lm[0],
                   b[0] * v[0]->lm[1] + b[1] * v[1]->lm[1] + b[2] * v[2]->lm[1], l);
        }
        float* out = liquid ? &f.wrgb[at * 3] : &f.rgb[at * 3];
        for (int q = 0; q < 3; ++q) out[q] = std::min(1.0f, c[q] * (0.5f + 1.25f * l[q]));
        if (liquid) f.wkind[at] = uint8_t(kind);
      }
    }
  }
  return f;
}

ov::Frame overview_frame(const Scene& scene, int max_px) {
  const double k = 1 / kUnitM;
  const double lo[3] = {scene.mins[0] * k, scene.mins[2] * k, scene.mins[1] * k};
  const double hi[3] = {scene.maxs[0] * k, scene.maxs[2] * k, scene.maxs[1] * k};
  return ov::frame_for(lo, hi, max_px);
}

ov::Mesh overview_box(const Scene& scene) {
  ov::Mesh out;
  if (scene.textures.levels.empty()) return out;
  const double k = 1 / kUnitM;
  for (const DrawVertex& v : scene.vertices) {
    out.pos.insert(out.pos.end(), {float(v.pos[0] * k), float(v.pos[2] * k), float(v.pos[1] * k)});
    out.st.insert(out.st.end(), {v.st[0], v.st[1]});
    out.rect.insert(out.rect.end(), {v.rect[0], v.rect[1], v.rect[2], v.rect[3]});
  }
  out.idx = scene.indices;
  out.tw = int(scene.textures.width);
  out.th = int(scene.textures.height);
  out.tex = scene.textures.levels[0];
  return out;
}

}  // namespace ge::brush::app
