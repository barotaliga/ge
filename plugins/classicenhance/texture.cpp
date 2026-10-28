// The texture enhancer (texture.hpp): the upscale, then the maps read off its result.
//
// The upscale's kernel at an output texel: the structure tensor J (the 3 × 3 mean of the source
// luminance's gradient products, interpolated bilinearly to the texel) gives the edge's normal
// e1 and its strength, the coherence a = (λ1 − λ2) / (λ1 + λ2). Across the edge the ellipse's
// radius shrinks to 0.45 texels as a nears 1; along it the radius grows to 1.6, so a texel is
// built from its own side of an edge, from the texels that continue the edge. A flat area
// (a near 0) gets a round kernel of radius 0.8. Weights fall off as (1 − q / 9)^4 in the
// ellipse's squared distance q, zero past 3 radii.
//
// ge:summary The texture enhancer: kernel-regression upscale, then height, normal, roughness, glow
// ge:keywords upscale kernel-regression structure-tensor coherence grain unsharp height normal
// ge:keywords roughness emissive material name
// ge:skill classicenhance
// ge:test tests/classicenhance_texture_test.cpp
#include "classicenhance/texture.hpp"

#include <algorithm>
#include <cmath>

#include "classicenhance/planes.hpp"
#include "core/noise.hpp"
#include "core/parallel.hpp"

namespace ge::classicenhance {

namespace {

// The kernel's radii in source texels: round in a flat area, and across and along a clean edge.
constexpr float kFlat = 0.6f, kAcross = 0.35f, kAlong = 1.2f;

bool has(std::string_view name, std::string_view word) {
  return name.find(word) != std::string_view::npos;
}

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
  return out;
}

uint32_t name_seed(std::string_view name) {
  uint32_t h = 2166136261u;
  for (const char c : name) h = (h ^ uint8_t(c)) * 16777619u;
  return h & 0xffffff;
}

struct Rgb {
  Plane r, g, b;
};

float luma(float r, float g, float b) { return 0.2126f * r + 0.7152f * g + 0.0722f * b; }

// The 4 × 4 anisotropic kernel regression of `src` (and the glow mask) at `scale`.
void upscale(const Rgb& src, const Plane& glow, int scale, Rgb& out, Plane& out_glow) {
  const int w = src.r.w, h = src.r.h;
  Plane lum(w, h);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) lum.at(x, y) = luma(src.r.at(x, y), src.g.at(x, y), src.b.at(x, y));
  }
  Plane jxx(w, h), jxy(w, h), jyy(w, h);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      const float gx = 0.5f * (lum.wrap(x + 1, y) - lum.wrap(x - 1, y));
      const float gy = 0.5f * (lum.wrap(x, y + 1) - lum.wrap(x, y - 1));
      jxx.at(x, y) = gx * gx;
      jxy.at(x, y) = gx * gy;
      jyy.at(x, y) = gy * gy;
    }
  }
  jxx = blur(jxx, 1, 1);
  jxy = blur(jxy, 1, 1);
  jyy = blur(jyy, 1, 1);

  const int ow = w * scale, oh = h * scale;
  out = {Plane(ow, oh), Plane(ow, oh), Plane(ow, oh)};
  out_glow = Plane(ow, oh);
  core::parallel_bands(size_t(oh), core::default_workers(), [&](size_t y0, size_t y1) {
    for (int oy = int(y0); oy < int(y1); oy++) {
      const double sy = (oy + 0.5) / scale - 0.5;
      const int iy = int(std::floor(sy));
      const float fy = float(sy - iy);
      for (int ox = 0; ox < ow; ox++) {
        const double sx = (ox + 0.5) / scale - 0.5;
        const int ix = int(std::floor(sx));
        const float fx = float(sx - ix);
        auto bil = [&](const Plane& p) {
          return (1 - fy) * ((1 - fx) * p.wrap(ix, iy) + fx * p.wrap(ix + 1, iy)) +
                 fy * ((1 - fx) * p.wrap(ix, iy + 1) + fx * p.wrap(ix + 1, iy + 1));
        };
        const float a = bil(jxx), b = bil(jxy), c = bil(jyy);
        const float tr = a + c;
        const float disc = std::sqrt(std::max(0.0f, 0.25f * (a - c) * (a - c) + b * b));
        const float l1 = 0.5f * tr + disc, l2 = 0.5f * tr - disc;
        const float coh = tr > 1e-7f ? (l1 - l2) / (l1 + l2 + 1e-7f) : 0.0f;
        // e1, the gradient's direction: the eigenvector of l1.
        float ex = b, ey = l1 - a;
        if (std::fabs(ex) + std::fabs(ey) < 1e-9f) {
          ex = a >= c ? 1.0f : 0.0f;
          ey = a >= c ? 0.0f : 1.0f;
        }
        const float en = 1.0f / std::sqrt(ex * ex + ey * ey);
        ex *= en;
        ey *= en;
        const float across = kFlat - (kFlat - kAcross) * coh, along = kFlat + (kAlong - kFlat) * coh;
        const float ia = 1.0f / (across * across), il = 1.0f / (along * along);
        float sw = 0, sr = 0, sg = 0, sb = 0, sm = 0;
        for (int dy = -1; dy <= 2; dy++) {
          for (int dx = -1; dx <= 2; dx++) {
            const float px = float(dx) - fx, py = float(dy) - fy;
            const float u = px * ex + py * ey, v = -px * ey + py * ex;
            const float q = u * u * ia + v * v * il;
            if (q >= 9.0f) continue;
            const float t = 1.0f - q / 9.0f;
            const float wgt = t * t * t * t;
            sw += wgt;
            sr += wgt * src.r.wrap(ix + dx, iy + dy);
            sg += wgt * src.g.wrap(ix + dx, iy + dy);
            sb += wgt * src.b.wrap(ix + dx, iy + dy);
            sm += wgt * glow.wrap(ix + dx, iy + dy);
          }
        }
        if (sw <= 0) {  // a kernel too thin to reach a texel: the nearest one
          const int nx = int(std::lround(sx)), ny = int(std::lround(sy));
          out.r.at(ox, oy) = src.r.wrap(nx, ny);
          out.g.at(ox, oy) = src.g.wrap(nx, ny);
          out.b.at(ox, oy) = src.b.wrap(nx, ny);
          out_glow.at(ox, oy) = glow.wrap(nx, ny);
          continue;
        }
        out.r.at(ox, oy) = sr / sw;
        out.g.at(ox, oy) = sg / sw;
        out.b.at(ox, oy) = sb / sw;
        out_glow.at(ox, oy) = sm / sw;
      }
    }
  });
}

// Iterative back-projection: the upscale is boxed back down to the source's size, and what that
// misses of the source is spread back up (bilinearly, wrapping) and added, `rounds` times, so the
// result agrees with the source texel for texel when averaged and gains the contrast a smooth
// kernel loses.
void back_project(Rgb& big, const Rgb& src, int scale, int rounds) {
  const int w = src.r.w, h = src.r.h;
  const float inv = 1.0f / float(scale * scale);
  for (int round = 0; round < rounds; round++) {
    for (int c = 0; c < 3; c++) {
      Plane& out = c == 0 ? big.r : c == 1 ? big.g : big.b;
      const Plane& in = c == 0 ? src.r : c == 1 ? src.g : src.b;
      Plane err(w, h);
      for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
          float sum = 0;
          for (int j = 0; j < scale; j++) {
            for (int i = 0; i < scale; i++) sum += out.at(x * scale + i, y * scale + j);
          }
          err.at(x, y) = in.at(x, y) - sum * inv;
        }
      }
      for (int oy = 0; oy < out.h; oy++) {
        const double sy = (oy + 0.5) / scale - 0.5;
        const int iy = int(std::floor(sy));
        const float fy = float(sy - iy);
        for (int ox = 0; ox < out.w; ox++) {
          const double sx = (ox + 0.5) / scale - 0.5;
          const int ix = int(std::floor(sx));
          const float fx = float(sx - ix);
          out.at(ox, oy) += (1 - fy) * ((1 - fx) * err.wrap(ix, iy) + fx * err.wrap(ix + 1, iy)) +
                            fy * ((1 - fx) * err.wrap(ix, iy + 1) + fx * err.wrap(ix + 1, iy + 1));
        }
      }
    }
  }
}

// Sharpening at the output's scale, the halo clamp, and grain in proportion to the source's
// local contrast.
void finish(Rgb& img, const Rgb& src, const Plane& src_lum, int scale, const EnhanceOptions& o,
            uint32_t seed) {
  const int ow = img.r.w, oh = img.r.h;
  const int radius = std::max(1, scale / 2);
  for (Plane* p : {&img.r, &img.g, &img.b}) {
    const Plane soft = blur(*p, radius, 2);
    for (size_t i = 0; i < p->v.size(); i++) {
      p->v[i] += float(o.sharpen) * (p->v[i] - soft.v[i]);
    }
  }
  // No halos: each texel is held between the darkest and brightest of the 2 × 2 source texels
  // round it, so back-projection and sharpening steepen an edge without ringing past it.
  for (int c = 0; c < 3; c++) {
    Plane& p = c == 0 ? img.r : c == 1 ? img.g : img.b;
    const Plane& s = c == 0 ? src.r : c == 1 ? src.g : src.b;
    for (int oy = 0; oy < oh; oy++) {
      const int iy = int(std::floor((oy + 0.5) / scale - 0.5));
      for (int ox = 0; ox < ow; ox++) {
        const int ix = int(std::floor((ox + 0.5) / scale - 0.5));
        const float a = s.wrap(ix, iy), b = s.wrap(ix + 1, iy), d = s.wrap(ix, iy + 1),
                    e = s.wrap(ix + 1, iy + 1);
        const float lo = std::min(std::min(a, b), std::min(d, e));
        const float hi = std::max(std::max(a, b), std::max(d, e));
        p.at(ox, oy) = std::clamp(p.at(ox, oy), lo, hi);
      }
    }
  }
  // The source's local contrast: the deviation of each texel from its 3 × 3 mean.
  const Plane mean = blur(src_lum, 1, 1);
  Plane dev(src_lum.w, src_lum.h);
  for (size_t i = 0; i < dev.v.size(); i++) dev.v[i] = std::fabs(src_lum.v[i] - mean.v[i]);
  const Plane contrast = blur(dev, 1, 1);
  for (int oy = 0; oy < oh; oy++) {
    for (int ox = 0; ox < ow; ox++) {
      const float c = contrast.wrap(ox / scale, oy / scale);
      const float n = float(core::hash2(ox, oy, seed) * 2.0 - 1.0);
      const float add = float(o.grain) * c * n;
      const float l = luma(img.r.at(ox, oy), img.g.at(ox, oy), img.b.at(ox, oy));
      // Grain changes brightness, not hue: each channel scales by the same factor.
      const float k = l > 1e-4f ? std::max(0.0f, (l + add) / l) : 1.0f;
      img.r.at(ox, oy) = std::clamp(img.r.at(ox, oy) * k, 0.0f, 1.0f);
      img.g.at(ox, oy) = std::clamp(img.g.at(ox, oy) * k, 0.0f, 1.0f);
      img.b.at(ox, oy) = std::clamp(img.b.at(ox, oy) * k, 0.0f, 1.0f);
    }
  }
}

assets::Image grey(const Plane& p) {
  assets::Image img{p.w, p.h, std::vector<uint8_t>(size_t(p.w) * size_t(p.h) * 4, 255)};
  for (size_t i = 0; i < p.v.size(); i++) {
    const auto v = uint8_t(std::clamp(p.v[i] * 255.0f + 0.5f, 0.0f, 255.0f));
    img.rgba[4 * i] = img.rgba[4 * i + 1] = img.rgba[4 * i + 2] = v;
  }
  return img;
}

assets::Image colour(const Rgb& c, const Plane* mask) {
  const int w = c.r.w, h = c.r.h;
  assets::Image img{w, h, std::vector<uint8_t>(size_t(w) * size_t(h) * 4, 255)};
  for (size_t i = 0; i < c.r.v.size(); i++) {
    const float m = mask ? std::clamp(mask->v[i], 0.0f, 1.0f) : 1.0f;
    img.rgba[4 * i] = linear_to_srgb8(c.r.v[i] * m);
    img.rgba[4 * i + 1] = linear_to_srgb8(c.g.v[i] * m);
    img.rgba[4 * i + 2] = linear_to_srgb8(c.b.v[i] * m);
  }
  return img;
}

float base_roughness(Material m) {
  switch (m) {
    case Material::stone: return 0.85f;
    case Material::metal: return 0.42f;
    case Material::wood: return 0.7f;
    case Material::liquid: return 0.06f;
    case Material::sky: return 1.0f;
    case Material::other: return 0.75f;
  }
  return 0.75f;
}

}  // namespace

const char* material_name(Material m) {
  switch (m) {
    case Material::stone: return "stone";
    case Material::metal: return "metal";
    case Material::wood: return "wood";
    case Material::liquid: return "liquid";
    case Material::sky: return "sky";
    case Material::other: return "other";
  }
  return "other";
}

Material material_of(std::string_view raw) {
  const std::string name = lower(raw);
  if (!name.empty() && name[0] == '*') return Material::liquid;
  if (name.rfind("sky", 0) == 0) return Material::sky;
  for (const char* w : {"met", "tech", "comp", "light", "plat", "pipe", "rivet", "steel", "iron"}) {
    if (has(name, w)) return Material::metal;
  }
  for (const char* w : {"wood", "plank", "crate", "box"}) {
    if (has(name, w)) return Material::wood;
  }
  for (const char* w : {"brick", "bric", "rock", "stone", "city", "wall", "wiz", "floor", "ground",
                        "cop", "slab", "tile", "rune", "stair", "step", "cliff", "dirt"}) {
    if (has(name, w)) return Material::stone;
  }
  return Material::other;
}

bool never_drawn(std::string_view raw) {
  const std::string name = lower(raw);
  return name == "trigger" || name == "clip" || name == "hint" || name == "skip" ||
         name == "origin";
}

assets::Image to_rgba(std::span<const uint8_t> indexed, int width, int height,
                      const Palette& palette) {
  assets::Image img{width, height, std::vector<uint8_t>(size_t(width) * size_t(height) * 4, 255)};
  for (size_t i = 0; i < size_t(width) * size_t(height) && i < indexed.size(); i++) {
    const size_t p = size_t(indexed[i]) * 3;
    img.rgba[4 * i] = palette.rgb[p];
    img.rgba[4 * i + 1] = palette.rgb[p + 1];
    img.rgba[4 * i + 2] = palette.rgb[p + 2];
  }
  return img;
}

Enhanced enhance_texture(std::string_view name, std::span<const uint8_t> indexed, int width,
                         int height, const Palette& palette, const EnhanceOptions& o) {
  Enhanced out;
  out.material = material_of(name);
  const int scale = std::clamp(o.scale, 1, 8);
  Rgb src{Plane(width, height), Plane(width, height), Plane(width, height)};
  Plane glow(width, height);
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      const size_t i = size_t(y) * size_t(width) + size_t(x);
      const uint8_t k = i < indexed.size() ? indexed[i] : 0;
      src.r.at(x, y) = srgb8_to_linear(palette.rgb[size_t(k) * 3]);
      src.g.at(x, y) = srgb8_to_linear(palette.rgb[size_t(k) * 3 + 1]);
      src.b.at(x, y) = srgb8_to_linear(palette.rgb[size_t(k) * 3 + 2]);
      glow.at(x, y) = k >= kFullbrightFirst ? 1.0f : 0.0f;
      if (k >= kFullbrightFirst) out.glows = true;
    }
  }
  Plane src_lum(width, height);
  for (size_t i = 0; i < src_lum.v.size(); i++) {
    src_lum.v[i] = luma(src.r.v[i], src.g.v[i], src.b.v[i]);
  }
  Rgb big;
  Plane big_glow;
  upscale(src, glow, scale, big, big_glow);
  back_project(big, src, scale, 1);
  finish(big, src, src_lum, scale, o, name_seed(name));
  out.albedo = colour(big, nullptr);
  if (out.material == Material::sky) return out;

  // Height: the luminance's band between a fine and a broad blur, stretched to 0..1.
  const int ow = big.r.w, oh = big.r.h;
  Plane lum(ow, oh);
  for (size_t i = 0; i < lum.v.size(); i++) lum.v[i] = luma(big.r.v[i], big.g.v[i], big.b.v[i]);
  const Plane fine = blur(lum, std::max(1, scale / 4), 1);
  const Plane mid = blur(lum, 2 * scale, 2);
  const Plane broad = blur(lum, 8 * scale, 2);
  Plane height_p(ow, oh);
  for (size_t i = 0; i < lum.v.size(); i++) {
    height_p.v[i] = 0.35f * (fine.v[i] - broad.v[i]) + 0.65f * (mid.v[i] - broad.v[i]);
  }
  const float lo = percentile(height_p, 0.02), hi = percentile(height_p, 0.98);
  const float span = hi - lo > 1e-6f ? hi - lo : 1.0f;
  const float flat = out.material == Material::liquid ? 0.3f : 1.0f;
  for (float& v : height_p.v) v = 0.5f + flat * (std::clamp((v - lo) / span, 0.0f, 1.0f) - 0.5f);
  out.height = grey(height_p);

  // Normal: the height's slope (Sobel, per output texel), the full height range standing
  // `relief` × 2 source texels deep.
  const float depth = float(o.relief) * 2.0f * float(scale);
  out.normal = {ow, oh, std::vector<uint8_t>(size_t(ow) * size_t(oh) * 4, 255)};
  for (int y = 0; y < oh; y++) {
    for (int x = 0; x < ow; x++) {
      auto hp = [&](int dx, int dy) { return height_p.wrap(x + dx, y + dy); };
      const float dx = (hp(1, -1) + 2 * hp(1, 0) + hp(1, 1)) - (hp(-1, -1) + 2 * hp(-1, 0) + hp(-1, 1));
      const float dy = (hp(-1, 1) + 2 * hp(0, 1) + hp(1, 1)) - (hp(-1, -1) + 2 * hp(0, -1) + hp(1, -1));
      // dy is down the picture; +Y of the normal is up it.
      float nx = -dx / 8.0f * depth, ny = dy / 8.0f * depth, nz = 1.0f;
      const float inv = 1.0f / std::sqrt(nx * nx + ny * ny + nz * nz);
      nx *= inv;
      ny *= inv;
      nz *= inv;
      const size_t i = (size_t(y) * size_t(ow) + size_t(x)) * 4;
      out.normal.rgba[i] = uint8_t(std::clamp((nx * 0.5f + 0.5f) * 255.0f + 0.5f, 0.0f, 255.0f));
      out.normal.rgba[i + 1] = uint8_t(std::clamp((ny * 0.5f + 0.5f) * 255.0f + 0.5f, 0.0f, 255.0f));
      out.normal.rgba[i + 2] = uint8_t(std::clamp((nz * 0.5f + 0.5f) * 255.0f + 0.5f, 0.0f, 255.0f));
    }
  }

  // Roughness: the material's base, rougher down in the crevices.
  const float base = base_roughness(out.material);
  Plane rough(ow, oh);
  for (size_t i = 0; i < rough.v.size(); i++) {
    rough.v[i] = std::clamp(base + 0.15f * (0.5f - height_p.v[i]), 0.03f, 1.0f);
  }
  out.roughness = grey(rough);
  out.emissive = colour(big, &big_glow);
  return out;
}

}  // namespace ge::classicenhance
