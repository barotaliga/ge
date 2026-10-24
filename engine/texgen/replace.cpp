// The matcher's measure, its library of recipes, and the recoloured bake.
//
// ge:summary Likeness measure, the recipe matcher and the recoloured replacement bake
// ge:keywords replace match likeness colour-transfer mean spread full-bright resize
// ge:skill textures
// ge:test tests/texgen_replace_test.cpp
#include "texgen/replace.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"
#include "texgen/bake.hpp"
#include "texgen/library.hpp"
#include "texgen/preview.hpp"

namespace ge::texgen {
namespace {

double luma_at(const lab::Picture& p, int x, int y) {
  const size_t i = (static_cast<size_t>(y) * p.width + x) * 4;
  return (0.299 * p.rgba[i] + 0.587 * p.rgba[i + 1] + 0.114 * p.rgba[i + 2]) / 255.0;
}

// The seed a texture's replacement bakes under. An animated texture's frames ("+0name", "+1name")
// share one, so the frames of a replaced animation do not flicker between variants.
uint32_t name_seed(std::string_view name) {
  if (name.size() > 2 && name[0] == '+') name.remove_prefix(2);
  uint32_t h = 2166136261u;
  for (const char c : name) h = (h ^ static_cast<uint8_t>(c)) * 16777619u;
  return h;
}

// The smallest power of two at least `n`, 16..1024.
int pow2_at_least(uint32_t n) {
  int s = 16;
  while (s < static_cast<int>(n) && s < 1024) s *= 2;
  return s;
}

// The strongest repeat along one axis: the lag (2 texels up to half the tile) where the luma's
// wrapped autocorrelation peaks, as a share of the tile, and the correlation there.
void repeat_of(const std::vector<double>& luma, int w, int h, bool along_x, double* period,
               double* strength) {
  const int n = along_x ? w : h;
  double mean = 0;
  for (const double v : luma) mean += v;
  mean /= static_cast<double>(luma.size());
  double var = 0;
  for (const double v : luma) var += (v - mean) * (v - mean);
  *period = 0.5;
  *strength = 0;
  if (var < 1e-9 || n < 8) return;
  std::vector<double> corr(static_cast<size_t>(n / 2 + 2), 0.0);
  for (int lag = 1; lag <= n / 2 + 1 && lag < n; ++lag) {
    double sum = 0;
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        const int x2 = along_x ? (x + lag) % w : x, y2 = along_x ? y : (y + lag) % h;
        sum += (luma[static_cast<size_t>(y) * w + x] - mean) *
               (luma[static_cast<size_t>(y2) * w + x2] - mean);
      }
    }
    corr[lag] = sum / var;
  }
  // The first local peak that is near the best: a pattern repeating every 8 also correlates at
  // 16, 24 and 32, and the shortest of them is its period.
  double best = 0;
  for (int lag = 2; lag <= n / 2; ++lag) best = std::max(best, corr[lag]);
  for (int lag = 2; lag <= n / 2; ++lag) {
    if (corr[lag] >= corr[lag - 1] && corr[lag] >= corr[lag + 1] && corr[lag] >= 0.8 * best) {
      *period = static_cast<double>(lag) / n;
      *strength = std::max(0.0, corr[lag]);
      return;
    }
  }
}

}  // namespace

Likeness likeness_of(const lab::Picture& p) {
  Likeness l;
  l.look = look_of(p);
  if (p.width <= 0 || p.height <= 0) return l;
  size_t across = 0, down = 0;
  for (int y = 0; y < p.height; ++y) {
    for (int x = 0; x < p.width; ++x) {
      const double c = luma_at(p, x, y);
      if (std::fabs(c - luma_at(p, (x + 1) % p.width, y)) > 0.08) ++across;
      if (std::fabs(c - luma_at(p, x, (y + 1) % p.height)) > 0.08) ++down;
    }
  }
  const double n = static_cast<double>(p.width) * p.height;
  l.across = static_cast<double>(across) / n;
  l.down = static_cast<double>(down) / n;
  std::vector<double> luma(static_cast<size_t>(p.width) * p.height);
  for (int y = 0; y < p.height; ++y)
    for (int x = 0; x < p.width; ++x) luma[static_cast<size_t>(y) * p.width + x] = luma_at(p, x, y);
  repeat_of(luma, p.width, p.height, true, &l.period_x, &l.repeat_x);
  repeat_of(luma, p.width, p.height, false, &l.period_y, &l.repeat_y);
  return l;
}

double likeness_distance(const Likeness& a, const Likeness& b) {
  const double look = look_distance(a.look, b.look);
  // Which way the lines run, as the difference of the two shares over their sum: courses of
  // bricks step down far more than across, planks the other way.
  auto lean = [](const Likeness& l) { return (l.down - l.across) / (l.down + l.across + 0.02); };
  const double d = (lean(a) - lean(b)) / 0.3;
  // A repeat counts as much as both textures show one: two noises have no period to compare.
  auto rep = [](double pa, double pb, double sa, double sb) {
    return std::fabs(core::libm::log2(pa / pb)) * std::min(sa, sb) / 0.3;
  };
  const double rx = rep(a.period_x, b.period_x, a.repeat_x, b.repeat_x);
  const double ry = rep(a.period_y, b.period_y, a.repeat_y, b.repeat_y);
  // How strongly each repeats at all: a grid against a noise.
  const double rs = (a.repeat_x + a.repeat_y - b.repeat_x - b.repeat_y) / 0.6;
  return std::sqrt((look * look * 7 + d * d + rx * rx + ry * ry + rs * rs) / 11);
}

Matcher make_matcher(const std::string& dir) {
  Matcher m;
  for (const std::string& path : list_recipes(dir)) {
    ParseResult parsed = load_recipe(path);
    if (!parsed.ok()) continue;
    Recipe r = parsed.recipe;
    const int full = r.size;
    r.size = 64;
    // Two seeds, averaged, so the measure is the recipe's and not one variant's.
    Likeness sum;
    for (uint32_t seed : {1u, 2u}) {
      const Likeness l =
          likeness_of(map_picture(bake(r, {r.name, seed, {}}).maps, MapKind::albedo, 64));
      sum.look.brightness += l.look.brightness / 2;
      sum.look.contrast += l.look.contrast / 2;
      sum.look.saturation += l.look.saturation / 2;
      sum.look.warmth += l.look.warmth / 2;
      sum.look.grain += l.look.grain / 2;
      sum.look.edges += l.look.edges / 2;
      sum.look.colours += l.look.colours / 2;
      sum.across += l.across / 2;
      sum.down += l.down / 2;
      sum.period_x += (l.period_x - 0.5) / 2;  // the default 0.5 counted once
      sum.period_y += (l.period_y - 0.5) / 2;
      sum.repeat_x += l.repeat_x / 2;
      sum.repeat_y += l.repeat_y / 2;
    }
    r.size = full;
    m.entries.push_back({r, sum});
  }
  return m;
}

std::optional<Replacement> replace_texture(const Matcher& m, std::string_view name,
                                           const lab::Picture& original,
                                           std::span<const uint8_t> lights) {
  const int w = original.width, h = original.height;
  if (w <= 0 || h <= 0 || m.entries.empty()) return std::nullopt;
  if (name.starts_with('*') || name.starts_with("sky")) return std::nullopt;
  // Matched on structure, then recoloured: the match ranks by likeness with the
  // colour numbers left out of the look, so a brown brick still finds the brick recipe.
  const Likeness want = likeness_of(original);
  const Matcher::Entry* best = nullptr;
  double best_d = 1e9;
  for (const Matcher::Entry& e : m.entries) {
    Likeness a = want, b = e.likeness;
    a.look.brightness = b.look.brightness;
    a.look.saturation = b.look.saturation;
    a.look.warmth = b.look.warmth;
    const double d = likeness_distance(a, b);
    if (d < best_d) {
      best_d = d;
      best = &e;
    }
  }
  if (best == nullptr || best_d > m.max_distance) return std::nullopt;

  Recipe r = best->recipe;
  r.size = pow2_at_least(static_cast<uint32_t>(std::max(w, h)));
  const Maps maps = bake(r, {r.name, name_seed(name), {}}).maps;
  lab::Picture out = lab::resized(map_picture(maps, MapKind::albedo, r.size), w, h);

  // Each channel's mean and spread moved onto the original's; the spread's ratio kept within
  // a factor of two so a flat original does not flatten the bake to nothing.
  const size_t n = static_cast<size_t>(w) * h;
  for (int c = 0; c < 3; ++c) {
    double mo = 0, mb = 0, so = 0, sb = 0;
    for (size_t i = 0; i < n; ++i) {
      mo += original.rgba[i * 4 + c];
      mb += out.rgba[i * 4 + c];
    }
    mo /= static_cast<double>(n);
    mb /= static_cast<double>(n);
    for (size_t i = 0; i < n; ++i) {
      so += (original.rgba[i * 4 + c] - mo) * (original.rgba[i * 4 + c] - mo);
      sb += (out.rgba[i * 4 + c] - mb) * (out.rgba[i * 4 + c] - mb);
    }
    so = std::sqrt(so / static_cast<double>(n));
    sb = std::sqrt(sb / static_cast<double>(n));
    const double k = std::clamp(sb > 1e-6 ? so / sb : 1.0, 0.5, 2.0);
    for (size_t i = 0; i < n; ++i) {
      const double v = mo + (out.rgba[i * 4 + c] - mb) * k;
      out.rgba[i * 4 + c] = static_cast<uint8_t>(std::clamp(std::floor(v + 0.5), 0.0, 255.0));
    }
  }
  if (lights.size() == n) {
    for (size_t i = 0; i < n; ++i) {
      if (lights[i] == 0) continue;
      for (int c = 0; c < 4; ++c) out.rgba[i * 4 + c] = original.rgba[i * 4 + c];
    }
  }
  for (size_t i = 0; i < n; ++i) out.rgba[i * 4 + 3] = 255;
  return Replacement{std::move(out), best->recipe.name, best_d};
}

std::optional<Replacement> replace_indexed(const Matcher& m, std::string_view name, uint32_t w,
                                           uint32_t h, std::span<const uint8_t> indexed,
                                           std::span<const uint8_t> palette, int first_bright) {
  const size_t n = static_cast<size_t>(w) * h;
  if (indexed.size() < n || palette.size() < 768) return std::nullopt;
  lab::Picture p = lab::create_picture(static_cast<int>(w), static_cast<int>(h), {0, 0, 0});
  std::vector<uint8_t> lights(n, 0);
  for (size_t i = 0; i < n; ++i) {
    const size_t k = indexed[i];
    for (int c = 0; c < 3; ++c) p.rgba[i * 4 + c] = palette[k * 3 + c];
    lights[i] = static_cast<int>(k) >= first_bright ? 1 : 0;
  }
  return replace_texture(m, name, p, lights);
}

}  // namespace ge::texgen
