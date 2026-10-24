// The matcher's measure, its library of recipes, and the recoloured bake.
//
// ge:summary Likeness measure, the recipe matcher and the recoloured replacement bake
// ge:keywords replace match likeness colour-transfer mean spread full-bright resize
// ge:skill textures
// ge:test tests/texgen_replace_test.cpp
#include "texgen/replace.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
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

TextureRef colour_ref(std::string_view name, const lab::Picture& original) {
  TextureRef ref;
  ref.seed = name_seed(name);
  const size_t n = static_cast<size_t>(original.width) * original.height;
  if (n == 0) return ref;
  std::array<uint8_t, 3> tint{}, spread{};
  for (int c = 0; c < 3; ++c) {
    double mean = 0, var = 0;
    for (size_t i = 0; i < n; ++i) mean += original.rgba[i * 4 + c];
    mean /= static_cast<double>(n);
    for (size_t i = 0; i < n; ++i) {
      const double d = original.rgba[i * 4 + c] - mean;
      var += d * d;
    }
    tint[c] = static_cast<uint8_t>(std::clamp(std::floor(mean + 0.5), 0.0, 255.0));
    spread[c] = static_cast<uint8_t>(
        std::clamp(std::floor(std::sqrt(var / static_cast<double>(n)) + 0.5), 0.0, 255.0));
  }
  ref.tint = tint;
  ref.spread = spread;
  return ref;
}

std::vector<Candidate> nearest_refs(const Matcher& m, std::string_view name,
                                    const lab::Picture& original, int n, std::string_view klass) {
  // The structure's ranking first (cheap: no bake), then the `refine` nearest of it baked with
  // the texture's seed and colours and measured whole: a recipe whose own colours do not survive
  // the recolouring (a glow, a hue that clips) falls behind there.
  std::vector<Candidate> rough = rank_recipes(m, original, std::max(n, m.refine), klass);
  const TextureRef base = colour_ref(name, original);
  for (Candidate& c : rough) {
    TextureRef ref = base;
    ref.name = c.recipe;
    std::vector<Diagnostic> errors;
    c.distance = ref_distance(m, ref, original, errors);
  }
  std::stable_sort(rough.begin(), rough.end(),
                   [](const Candidate& x, const Candidate& y) { return x.distance < y.distance; });
  if (static_cast<int>(rough.size()) > n) rough.resize(static_cast<size_t>(std::max(0, n)));
  return rough;
}

TextureMatch match_texture(const Matcher& m, std::string_view name, const lab::Picture& original) {
  TextureMatch out;
  if (name.starts_with('*')) {
    out.kind = TextureKind::liquid;
    std::string_view stem = name.substr(1);
    while (!stem.empty() && stem.back() >= '0' && stem.back() <= '9') stem.remove_suffix(1);
    while (!stem.empty() && stem.front() >= '0' && stem.front() <= '9') stem.remove_prefix(1);
    out.liquid = std::string(stem);
    return out;
  }
  if (name.starts_with("sky")) {
    out.kind = TextureKind::sky;
    return out;
  }
  const int w = original.width, h = original.height;
  if (w <= 0 || h <= 0 || m.entries.empty()) return out;
  out.ref = colour_ref(name, original);
  const std::vector<Candidate> best = nearest_refs(m, name, original, 1);
  if (best.empty()) return out;
  out.ref.name = best[0].recipe;
  out.text = ref_text(out.ref);
  out.distance = best[0].distance;
  out.near = out.distance <= m.max_distance;
  return out;
}

TextureMatch match_rgb(const Matcher& m, std::string_view name, std::span<const uint8_t> rgb,
                       uint32_t w, uint32_t h) {
  const size_t n = static_cast<size_t>(w) * h;
  if (rgb.size() < n * 3) return match_texture(m, name, {});
  lab::Picture p = lab::create_picture(static_cast<int>(w), static_cast<int>(h), {0, 0, 0});
  for (size_t i = 0; i < n; ++i)
    for (int c = 0; c < 3; ++c) p.rgba[i * 4 + c] = rgb[i * 3 + c];
  return match_texture(m, name, p);
}

namespace {

lab::Picture from_indexed(uint32_t w, uint32_t h, std::span<const uint8_t> indexed,
                          std::span<const uint8_t> palette) {
  const size_t n = static_cast<size_t>(w) * h;
  if (indexed.size() < n || palette.size() < 768) return {};
  lab::Picture p = lab::create_picture(static_cast<int>(w), static_cast<int>(h), {0, 0, 0});
  for (size_t i = 0; i < n; ++i)
    for (int c = 0; c < 3; ++c) p.rgba[i * 4 + c] = palette[indexed[i] * 3u + c];
  return p;
}

}  // namespace

TextureMatch match_indexed(const Matcher& m, std::string_view name, uint32_t w, uint32_t h,
                           std::span<const uint8_t> indexed, std::span<const uint8_t> palette) {
  return match_texture(m, name, from_indexed(w, h, indexed, palette));
}

lab::Picture bake_ref(const Matcher& m, const TextureRef& ref, int w, int h,
                      std::vector<Diagnostic>& errors) {
  for (const Matcher::Entry& e : m.entries) {
    if (e.recipe.name != ref.name) continue;
    Recipe r = e.recipe;
    r.size = pow2_at_least(static_cast<uint32_t>(std::max(w, h)));
    // Baked uncoloured, resized, then recoloured, so the tint and spread hold at the size drawn.
    TextureRef plain = ref;
    plain.tint.reset();
    plain.spread.reset();
    const BakeResult b = bake(r, plain);
    if (!b.ok()) {
      errors.insert(errors.end(), b.errors.begin(), b.errors.end());
      return lab::create_picture(w, h, {128, 128, 128});
    }
    lab::Picture out = lab::resized(map_picture(b.maps, MapKind::albedo, r.size), w, h);
    recolour(out.rgba, ref.tint, ref.spread);
    return out;
  }
  std::string names;
  for (const Matcher::Entry& e : m.entries) names += " " + e.recipe.name;
  errors.push_back({0, "no recipe named `" + ref.name + "`; the library has:" + names});
  return lab::create_picture(w, h, {128, 128, 128});
}

lab::Picture bake_match(const Matcher& m, const TextureMatch& match, int w, int h) {
  std::vector<Diagnostic> ignored;
  return bake_ref(m, match.ref, w, h, ignored);
}

namespace {

constexpr double kRepeats = 0.25;  // an autocorrelation peak this strong is a repeat

std::string hex_colour(const std::array<uint8_t, 3>& c) {
  static const char* digits = "0123456789abcdef";
  std::string h = "#";
  for (const uint8_t b : c) {
    h += digits[b >> 4];
    h += digits[b & 15];
  }
  return h;
}

std::string count_text(double v) {
  return v <= 0 ? "-" : core::num::to_fixed(v, v < 10 && v != std::floor(v) ? 1 : 0);
}

}  // namespace

Evidence evidence_of(const lab::Picture& p) {
  Evidence e;
  e.width = p.width;
  e.height = p.height;
  if (p.width <= 0 || p.height <= 0) return e;
  const Likeness l = likeness_of(p);
  if (l.repeat_x >= kRepeats) {
    e.repeat_x = l.period_x * p.width;
    e.per_tile_x = 1 / l.period_x;
  }
  if (l.repeat_y >= kRepeats) {
    e.repeat_y = l.period_y * p.height;
    e.per_tile_y = 1 / l.period_y;
  }
  // A course of bricks steps going DOWN across its joints: horizontal lines.
  if (l.down + l.across < 0.04) e.lines = "flat";
  else if (l.down > 1.5 * l.across) e.lines = "horizontal";
  else if (l.across > 1.5 * l.down) e.lines = "vertical";
  else e.lines = "mixed";
  e.brightness = l.look.brightness;
  e.contrast = l.look.contrast;
  const TextureRef c = colour_ref("", p);
  if (c.tint) e.mean = *c.tint;
  return e;
}

std::string evidence_text(const Evidence& e) {
  std::string out = "repeat ";
  out += (e.repeat_x > 0 ? count_text(e.repeat_x) : "-") + "x" +
         (e.repeat_y > 0 ? count_text(e.repeat_y) : "-") + " px (" + count_text(e.per_tile_x) +
         "x" + count_text(e.per_tile_y) + " a tile), " + e.lines + " lines, contrast " +
         core::num::to_fixed(e.contrast, 2) + ", " + hex_colour(e.mean);
  return out;
}

std::vector<std::string> evidence_mismatches(const Evidence& a, const Evidence& b) {
  std::vector<std::string> out;
  auto axis = [&](const char* way, double pa, double pb) {
    if (pa > 0 && pb > 0) {
      const double r = pa > pb ? pa / pb : pb / pa;
      if (r > 1.25)
        out.push_back(std::string("the pattern repeats ") + count_text(pa) + " times " + way +
                      " the original but " + count_text(pb) + " times " + way +
                      " ours: its pieces are " + (pb < pa ? "too big" : "too small") +
                      " (turn the recipe's rows, cols or scale)");
    } else if (pa > 0) {
      out.push_back(std::string("the original repeats ") + count_text(pa) + " times " + way +
                    " a tile; ours shows no repeat " + way);
    } else if (pb > 0) {
      out.push_back(std::string("ours repeats ") + count_text(pb) + " times " + way +
                    " a tile; the original shows no repeat " + way);
    }
  };
  axis("across", a.per_tile_x, b.per_tile_x);
  axis("down", a.per_tile_y, b.per_tile_y);
  if (a.lines != b.lines && a.lines != "mixed" && b.lines != "mixed")
    out.push_back("the original's lines are " + a.lines + ", ours " + b.lines);
  if (a.contrast > 0.01 && b.contrast > 0.01) {
    const double r = a.contrast / b.contrast;
    if (r > 1.6 || r < 1 / 1.6)
      out.push_back("contrast " + core::num::to_fixed(a.contrast, 2) + " in the original, " +
                    core::num::to_fixed(b.contrast, 2) + " in ours");
  }
  return out;
}

std::string texture_name_of_file(std::string_view stem) {
  if (stem.starts_with("liq_")) return "*" + std::string(stem.substr(4));
  if (stem.starts_with("anim_")) return "+" + std::string(stem.substr(5));
  return std::string(stem);
}

bool recipe_has_class(const Recipe& r, std::string_view klass) {
  return std::find(r.tags.begin(), r.tags.end(), klass) != r.tags.end();
}

std::vector<Candidate> rank_recipes(const Matcher& m, const lab::Picture& original, int n,
                                    std::string_view klass) {
  std::vector<Candidate> out;
  if (original.width <= 0 || original.height <= 0) return out;
  const Likeness want = likeness_of(original);
  for (const Matcher::Entry& e : m.entries) {
    if (!klass.empty() && !recipe_has_class(e.recipe, klass)) continue;
    Likeness a = want;
    a.look.brightness = e.likeness.look.brightness;
    a.look.saturation = e.likeness.look.saturation;
    a.look.warmth = e.likeness.look.warmth;
    out.push_back({e.recipe.name, likeness_distance(a, e.likeness)});
  }
  std::stable_sort(out.begin(), out.end(),
                   [](const Candidate& x, const Candidate& y) { return x.distance < y.distance; });
  if (static_cast<int>(out.size()) > n) out.resize(static_cast<size_t>(std::max(0, n)));
  return out;
}

double ref_distance(const Matcher& m, const TextureRef& ref, const lab::Picture& original,
                    std::vector<Diagnostic>& errors) {
  const size_t before = errors.size();
  const lab::Picture baked = bake_ref(m, ref, original.width, original.height, errors);
  if (errors.size() != before) return 1e9;
  return likeness_distance(likeness_of(original), likeness_of(baked));
}

std::optional<Replacement> replace_texture(const Matcher& m, std::string_view name,
                                           const lab::Picture& original,
                                           std::span<const uint8_t> lights) {
  TextureMatch match = match_texture(m, name, original);
  if (match.kind != TextureKind::surface || match.text.empty() || !match.near) return std::nullopt;
  const int w = original.width, h = original.height;
  lab::Picture out = bake_match(m, match, w, h);
  const size_t n = static_cast<size_t>(w) * h;
  if (lights.size() == n) {
    for (size_t i = 0; i < n; ++i) {
      if (lights[i] == 0) continue;
      for (int c = 0; c < 3; ++c) out.rgba[i * 4 + c] = original.rgba[i * 4 + c];
    }
  }
  for (size_t i = 0; i < n; ++i) out.rgba[i * 4 + 3] = 255;
  return Replacement{std::move(out), std::move(match)};
}

std::optional<Replacement> replace_indexed(const Matcher& m, std::string_view name, uint32_t w,
                                           uint32_t h, std::span<const uint8_t> indexed,
                                           std::span<const uint8_t> palette, int first_bright) {
  const lab::Picture p = from_indexed(w, h, indexed, palette);
  if (p.width == 0) return std::nullopt;
  const size_t n = static_cast<size_t>(w) * h;
  std::vector<uint8_t> lights(n, 0);
  for (size_t i = 0; i < n; ++i) lights[i] = indexed[i] >= first_bright ? 1 : 0;
  return replace_texture(m, name, p, lights);
}

}  // namespace ge::texgen
