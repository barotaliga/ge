// The map picture's floors (map_floors.hpp).
#include <algorithm>
#include <cmath>
#include <vector>

#include "brush/map.hpp"
#include "shooters_edge/labs/map_floors.hpp"
#include "ui/paint.hpp"

namespace shooters_edge::labs {

namespace {
namespace br = ge::brush;
namespace ba = ge::brush::app;
namespace ui = ge::ui;
}  // namespace






double wrap(double v, double n) {
  const double r = std::fmod(v, n);
  return r < 0 ? r + n : r;
}

namespace {

// A texel of the atlas's level `mip` at the vertex's texture rectangle (mip 0 texels).
void texel(const ba::Atlas& a, int mip, const float rect[4], double s, double t, float out[3]) {
  if (a.levels.empty()) {
    out[0] = out[1] = out[2] = 0.5f;
    return;
  }
  mip = std::min<int>(mip, int(a.levels.size()) - 1);
  const double k = 1.0 / double(1 << mip);
  const uint32_t aw = std::max<uint32_t>(1, a.width >> mip);
  const uint32_t ah = std::max<uint32_t>(1, a.height >> mip);
  const double rw = std::max(1.0, rect[2] * k), rh = std::max(1.0, rect[3] * k);
  const auto x = uint32_t(rect[0] * k + wrap(s * k, rw));
  const auto y = uint32_t(rect[1] * k + wrap(t * k, rh));
  const uint8_t* p = &a.levels[size_t(mip)][(size_t(std::min(y, ah - 1)) * aw + std::min(x, aw - 1)) * 4];
  out[0] = p[0] / 255.0f;
  out[1] = p[1] / 255.0f;
  out[2] = p[2] / 255.0f;
}

// The light at the lightmap atlas's uv, bilinear, 0..~2 (128 the light as baked).
void light_at(const ba::Atlas& a, double u, double v, float out[3]) {
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
    const auto at = [&](int xx, int yy) { return L[(size_t(yy) * a.width + size_t(xx)) * 4 + size_t(c)]; };
    const double top = at(x0, y0) * (1 - fx) + at(x1, y0) * fx;
    const double bot = at(x0, y1) * (1 - fx) + at(x1, y1) * fx;
    out[c] = float((top * (1 - fy) + bot * fy) / 128.0);
  }
}

}  // namespace

// Every face that looks up drawn straight down into `f` (`ss` samples a pixel each way).
Floors draw_floors(const ba::Scene& scene, const Frame& fr, int ss) {
  Floors f;
  f.ss = ss;
  f.w = fr.width * ss;
  f.h = fr.height * ss;
  const size_t n = size_t(f.w) * size_t(f.h);
  f.top.assign(n, kNone);
  f.rgb.assign(n * 3, 0);
  f.wet.assign(n, kNone);
  f.wrgb.assign(n * 3, 0);
  f.wkind.assign(n, 0);
  // The texture's level: a texel a unit, `scale * ss` samples a unit.
  const double per = fr.scale * ss;
  int mip = 0;
  while (mip < 3 && per * (1 << (mip + 1)) <= 1.0) ++mip;
  const double k = 1 / br::kUnitM;
  for (size_t i = 0; i + 2 < scene.indices.size(); i += 3) {
    const ba::DrawVertex* v[3] = {&scene.vertices[scene.indices[i]],
                                  &scene.vertices[scene.indices[i + 1]],
                                  &scene.vertices[scene.indices[i + 2]]};
    const uint32_t kind = v[0]->kind;
    if (kind == ba::kSky || kind == ba::kHidden) continue;
    if (v[0]->nrm[1] + v[1]->nrm[1] + v[2]->nrm[1] < 0.45) continue;  // walls and ceilings
    const bool liquid = kind == ba::kWater || kind == ba::kSlime || kind == ba::kLava;
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
        if (liquid || kind == ba::kTele) {
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

// The floors shaded and brought down to the picture: the void as a blueprint, a soft shade where
// anything stands over a floor, a low sun from the north-west, the lip of every drop lit.
ui::Image shade_floors(const Floors& f, const Frame& fr) {
  const int W = fr.width, H = fr.height, ss = f.ss;
  const double unit = fr.scale * ss;  // samples a unit
  const auto top = [&](int x, int y) -> float {
    x = std::clamp(x, 0, f.w - 1);
    y = std::clamp(y, 0, f.h - 1);
    return f.top[size_t(y) * size_t(f.w) + size_t(x)];
  };
  const float span = float(std::max(1.0, fr.hi[2] - fr.lo[2]));
  std::vector<float> shaded(size_t(f.w) * size_t(f.h) * 4, 0);
  constexpr int kDirs = 8;
  const double dx[kDirs] = {1, 0.7071, 0, -0.7071, -1, -0.7071, 0, 0.7071};
  const double dy[kDirs] = {0, 0.7071, 1, 0.7071, 0, -0.7071, -1, -0.7071};
  for (int y = 0; y < f.h; ++y) {
    for (int x = 0; x < f.w; ++x) {
      const size_t at = size_t(y) * size_t(f.w) + size_t(x);
      const float h = f.top[at];
      if (h == kNone && f.wet[at] == kNone) continue;
      const float base = h == kNone ? f.wet[at] - 64 : h;
      // What stands over this floor round it: a wall (no floor) stands 64 units over it.
      double occ = 0;
      for (int d = 0; d < kDirs; ++d) {
        for (const double r : {6.0, 14.0, 28.0}) {
          const float n = top(x + int(dx[d] * r * unit), y + int(dy[d] * r * unit));
          const double over = n == kNone ? 64.0 : double(n - base);
          occ += std::clamp(over / (r * 2.2), 0.0, 1.0);
        }
      }
      occ /= kDirs * 3;
      // The sun, low in the north-west: a wall 40 units over the floor, a ledge its own height.
      bool shadow = false;
      for (double r = 4; r <= 72 && !shadow; r += 4) {
        const float n = top(x - int(0.7071 * r * unit), y - int(0.7071 * r * unit));
        const double over = n == kNone ? 40.0 : double(n - base);
        if (over > r * 0.75 + 2) shadow = true;
      }
      // The lip of a drop: a floor a step or more over a neighbour, lit along its edge.
      bool lip = false;
      for (int d = 0; d < kDirs && !lip; d += 2) {
        const float n = top(x + int(dx[d] * 2 * ss), y + int(dy[d] * 2 * ss));
        lip = h != kNone && n != kNone && h - n > 14;
      }
      const double rise = std::clamp((base - fr.lo[2]) / span, 0.0, 1.0);
      double lightness = (0.8 + 0.3 * rise) * (1 - 0.62 * occ) * (shadow ? 0.66 : 1.0);
      float c[3] = {0.1f, 0.1f, 0.1f};
      if (h != kNone)
        for (int q = 0; q < 3; ++q) c[q] = f.rgb[at * 3 + size_t(q)];
      for (int q = 0; q < 3; ++q) c[q] = float(c[q] * lightness);
      if (lip)
        for (int q = 0; q < 3; ++q) c[q] = c[q] * 0.8f + 0.12f;
      // A liquid over the floor: its colour over a darkened floor, a cool sheen.
      if (f.wet[at] != kNone && f.wet[at] > h) {
        const uint8_t k = f.wkind[at];
        const float tint[3] = {k == ba::kLava ? 1.0f : k == ba::kSlime ? 0.35f : 0.18f,
                               k == ba::kLava ? 0.38f : k == ba::kSlime ? 0.72f : 0.48f,
                               k == ba::kLava ? 0.08f : k == ba::kSlime ? 0.22f : 0.86f};
        const float a = k == ba::kLava ? 0.85f : 0.42f;
        for (int q = 0; q < 3; ++q) {
          const float wc = f.wrgb[at * 3 + size_t(q)] * 0.45f + tint[q] * 0.55f;
          c[q] = c[q] * (1 - a) * 0.8f + wc * a;
        }
      }
      for (int q = 0; q < 3; ++q) shaded[at * 4 + size_t(q)] = std::clamp(c[q], 0.0f, 1.0f);
      shaded[at * 4 + 3] = 1;
    }
  }
  // Down to the picture, over the blueprint: a grid of 128 units (512 the stronger), the plan's
  // own shadow under it.
  ui::Image img = ui::make_image(W, H, ui::hex(0x0e131b));
  const auto mask = [&](int X, int Y) {
    if (X < 0 || Y < 0 || X >= W || Y >= H) return 0.0f;
    float s = 0;
    for (int j = 0; j < ss; ++j)
      for (int i = 0; i < ss; ++i)
        s += shaded[(size_t(Y * ss + j) * size_t(f.w) + size_t(X * ss + i)) * 4 + 3];
    return s / float(ss * ss);
  };
  for (int Y = 0; Y < H; ++Y) {
    for (int X = 0; X < W; ++X) {
      uint8_t* o = &img.rgba[(size_t(Y) * size_t(W) + size_t(X)) * 4];
      const double wx = fr.lo[0] + (X + 0.5 - fr.pad) / fr.scale;
      const double wy = fr.hi[1] - (Y + 0.5 - fr.pad) / fr.scale;
      const double g = 1 / fr.scale;
      const bool minor = wrap(wx, 128) < g || wrap(wy, 128) < g;
      const bool major = wrap(wx, 512) < g * 1.5 || wrap(wy, 512) < g * 1.5;
      float bg[3] = {0.055f, 0.075f, 0.105f};
      if (minor) bg[0] = 0.075f, bg[1] = 0.1f, bg[2] = 0.14f;
      if (major) bg[0] = 0.1f, bg[1] = 0.135f, bg[2] = 0.19f;
      // The plan's shadow, a few pixels down and right, soft.
      float sh = 0;
      for (int k = 0; k < 4; ++k) sh += mask(X - 3 - k * 2, Y - 3 - k * 2);
      for (float& q : bg) q *= 1 - 0.14f * sh;
      float acc[3] = {0, 0, 0}, cover = 0;
      for (int j = 0; j < ss; ++j) {
        for (int i = 0; i < ss; ++i) {
          const float* s = &shaded[(size_t(Y * ss + j) * size_t(f.w) + size_t(X * ss + i)) * 4];
          for (int q = 0; q < 3; ++q) acc[q] += s[q] * s[3];
          cover += s[3];
        }
      }
      const float a = cover / float(ss * ss);
      for (int q = 0; q < 3; ++q) {
        const float floor = cover > 0 ? acc[q] / cover : 0;
        o[q] = uint8_t(std::lround(std::clamp(floor * a + bg[q] * (1 - a), 0.0f, 1.0f) * 255));
      }
      o[3] = 255;
    }
  }
  // The walls' outline: a floor pixel beside the void, darkened into a crisp edge.
  std::vector<uint8_t> edge(size_t(W) * size_t(H), 0);
  for (int Y = 1; Y + 1 < H; ++Y)
    for (int X = 1; X + 1 < W; ++X) {
      const float m = mask(X, Y);
      if (m > 0.5f && (mask(X - 1, Y) < 0.5f || mask(X + 1, Y) < 0.5f || mask(X, Y - 1) < 0.5f ||
                       mask(X, Y + 1) < 0.5f))
        edge[size_t(Y) * size_t(W) + size_t(X)] = 1;
    }
  for (size_t i = 0; i < edge.size(); ++i)
    if (edge[i])
      for (int q = 0; q < 3; ++q) img.rgba[i * 4 + size_t(q)] = uint8_t(img.rgba[i * 4 + size_t(q)] * 0.35);
  return img;
}


}  // namespace shooters_edge::labs
