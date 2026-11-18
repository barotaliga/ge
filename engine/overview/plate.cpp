// The picture of a level from above (plate.hpp).
//
// ge:summary A level's floors from above shaded on the CPU, its liquids a layer, its roofs kept
// ge:keywords overview map-floors water-layer roof
// ge:skill map-design ui
#include "overview/plate.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "ui/paint.hpp"

namespace ge::overview {

namespace ui = ge::ui;

double wrap(double v, double n) {
  const double r = std::fmod(v, n);
  return r < 0 ? r + n : r;
}

Floors empty_floors(const Frame& fr, int ss) {
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
  return f;
}

// The floors shaded and brought down to the picture: the void as a blueprint, a soft shade where
// anything stands over a floor, a low sun from the north-west, the lip of every drop lit.
ui::Image shade_floors(const Floors& f, const Frame& fr, WaterLayer* layer) {
  const int W = fr.width, H = fr.height, ss = f.ss;
  const double unit = fr.scale * ss;  // samples a unit
  const auto top = [&](int x, int y) -> float {
    x = std::clamp(x, 0, f.w - 1);
    y = std::clamp(y, 0, f.h - 1);
    return f.top[size_t(y) * size_t(f.w) + size_t(x)];
  };
  const float span = float(std::max(1.0, fr.hi[2] - fr.lo[2]));
  std::vector<float> shaded(size_t(f.w) * size_t(f.h) * 4, 0);
  std::vector<float> liquid;  // with a layer: each sample's liquid colour and cover
  if (layer != nullptr) liquid.assign(shaded.size(), 0);
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
        const float tint[3] = {k == kLava    ? 1.0f
                               : k == kSlime ? 0.35f
                                             : 0.18f,
                               k == kLava    ? 0.38f
                               : k == kSlime ? 0.72f
                                             : 0.48f,
                               k == kLava    ? 0.08f
                               : k == kSlime ? 0.22f
                                             : 0.86f};
        const float a = k == kLava ? 0.85f : 0.42f;
        for (int q = 0; q < 3; ++q) {
          const float wc = f.wrgb[at * 3 + size_t(q)] * 0.45f + tint[q] * 0.55f;
          if (layer != nullptr) {
            c[q] *= 0.8f;  // the layer's own `a` over it makes the same colour
            liquid[at * 4 + size_t(q)] = wc;
          } else {
            c[q] = c[q] * (1 - a) * 0.8f + wc * a;
          }
        }
        if (layer != nullptr) liquid[at * 4 + 3] = a;
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
  if (layer != nullptr) {
    layer->image = ui::make_image(W, H, {0, 0, 0, 0});
    layer->height.assign(size_t(W) * size_t(H), kNone);
    for (int Y = 0; Y < H; ++Y)
      for (int X = 0; X < W; ++X) {
        float acc[3] = {0, 0, 0}, cover = 0, high = kNone;
        for (int j = 0; j < ss; ++j)
          for (int i = 0; i < ss; ++i) {
            const size_t at = size_t(Y * ss + j) * size_t(f.w) + size_t(X * ss + i);
            const float* l = &liquid[at * 4];
            for (int q = 0; q < 3; ++q) acc[q] += l[q] * l[3];
            cover += l[3];
            if (l[3] > 0) high = std::max(high, f.wet[at]);
          }
        if (cover <= 0) continue;
        const size_t o = size_t(Y) * size_t(W) + size_t(X);
        uint8_t* to = &layer->image.rgba[o * 4];
        for (int q = 0; q < 3; ++q)
          to[q] = uint8_t(std::lround(std::clamp(acc[q] / cover, 0.0f, 1.0f) * 255));
        to[3] = uint8_t(std::lround(std::clamp(cover / float(ss * ss), 0.0f, 1.0f) * 255));
        layer->height[o] = high;
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
      for (int q = 0; q < 3; ++q)
        img.rgba[i * 4 + size_t(q)] = uint8_t(img.rgba[i * 4 + size_t(q)] * 0.35);
  return img;
}

float Picture::liquid_surface(double x, double y) const {
  const int X = int(std::floor(frame.px(x))), Y = int(std::floor(frame.py(y)));
  if (X < 0 || Y < 0 || X >= frame.width || Y >= frame.height || liquid.height.empty())
    return kNone;
  return liquid.height[size_t(Y) * size_t(frame.width) + size_t(X)];
}

ui::Color Picture::liquid_tint(double x, double y, double z) const {
  const int X = int(std::floor(frame.px(x))), Y = int(std::floor(frame.py(y)));
  if (X < 0 || Y < 0 || X >= frame.width || Y >= frame.height || liquid.height.empty())
    return {1, 1, 1, 1};
  const size_t at = size_t(Y) * size_t(frame.width) + size_t(X);
  const float h = liquid.height[at];
  if (h <= kNone || z >= double(h)) return {1, 1, 1, 1};
  // Seen through the liquid: its colour laid over (a little more than the floor's, so a figure
  // reads as under it) and the liquid showing through.
  const uint8_t* c = &liquid.image.rgba[at * 4];
  const float a = std::min(0.9f, float(c[3]) / 255.0f * 1.5f);
  const auto mixed = [&](uint8_t q) { return 1 - a + a * (0.35f + 0.65f * float(q) / 255.0f); };
  return {mixed(c[0]), mixed(c[1]), mixed(c[2]), 1 - 0.35f * a};
}

bool Picture::covered(double x, double y, double z, double cover_units) const {
  const int X = int(std::floor(frame.px(x))), Y = int(std::floor(frame.py(y)));
  if (X < 0 || Y < 0 || X >= frame.width || Y >= frame.height || roof.empty()) return false;
  const float r = roof[size_t(Y) * size_t(frame.width) + size_t(X)];
  return r > kNone && double(r) > z + cover_units;
}

Frame frame_for(const double lo[3], const double hi[3], int max_px) {
  Frame fr;
  for (int q = 0; q < 3; ++q) fr.lo[q] = lo[q], fr.hi[q] = hi[q];
  fr.pad = 24;
  const double size = std::max(256, max_px);
  fr.scale =
      (size - 2 * fr.pad) / std::max(1.0, std::max(fr.hi[0] - fr.lo[0], fr.hi[1] - fr.lo[1]));
  fr.width = int(std::ceil((fr.hi[0] - fr.lo[0]) * fr.scale)) + 2 * fr.pad;
  fr.height = int(std::ceil((fr.hi[1] - fr.lo[1]) * fr.scale)) + 2 * fr.pad;
  return fr;
}

void picture_from_floors(const Floors& floors, const Frame& fr, Picture* out) {
  out->frame = fr;
  out->image = shade_floors(floors, fr, &out->liquid);
  out->roof.assign(size_t(fr.width) * size_t(fr.height), kNone);
  for (int y = 0; y < fr.height; ++y)
    for (int x = 0; x < fr.width; ++x) {
      float high = kNone;
      for (int j = 0; j < floors.ss; ++j)
        for (int i = 0; i < floors.ss; ++i)
          high = std::max(
              high,
              floors.top[size_t(y * floors.ss + j) * size_t(floors.w) + size_t(x * floors.ss + i)]);
      out->roof[size_t(y) * size_t(fr.width) + size_t(x)] = high;
    }
  const uint8_t* v0 = out->image.rgba.data();
  out->void_colour = {v0[0] / 255.0f, v0[1] / 255.0f, v0[2] / 255.0f, 1};
  // The liquids' pixels, one in every 3 x 3.
  out->water.clear();
  out->water_kind.clear();
  for (int y = 1; y < fr.height; y += 3)
    for (int x = 1; x < fr.width; x += 3) {
      const size_t at = size_t(y * floors.ss) * size_t(floors.w) + size_t(x * floors.ss);
      // Only where the liquid is the top surface: never glinting through a floor built over it.
      if (at < floors.wkind.size() && floors.wkind[at] != 0 && floors.wet[at] > kNone &&
          (floors.top[at] <= kNone || floors.wet[at] >= floors.top[at])) {
        out->water.push_back({uint16_t(x), uint16_t(y)});
        out->water_kind.push_back(floors.wkind[at]);
      }
    }
}

ui::Rect fit_rect(const Picture& p, ui::Rect area) {
  if (p.image.empty() || area.w <= 0 || area.h <= 0) return {area.x, area.y, 0, 0};
  const float k = std::min(area.w / float(p.image.width), area.h / float(p.image.height));
  const float w = float(p.image.width) * k, h = float(p.image.height) * k;
  return {area.x + (area.w - w) / 2, area.y + (area.h - h) / 2, w, h};
}

}  // namespace ge::overview
