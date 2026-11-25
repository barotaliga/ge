// Spent cases: their kinds, bounce, model and landing sounds (shells.hpp).
//
// ge:summary Spent cases: sizes, bounce shapes, the palette-coloured model, the landing sounds
// ge:keywords classicenhance shells casing model sound modes palette
// ge:skill classicenhance
// ge:test tests/classicenhance_shells_test.cpp
#include "classicenhance/shells.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/prng.hpp"

namespace ge::classicenhance {

namespace {

namespace lm = ge::core::libm;

constexpr int kSides = 8;               // round the case
constexpr int kSkinW = 32, kSkinH = 8;  // columns 0..29 along it, 30..31 its mouth
constexpr int kBodyColumns = 30;
constexpr int kLastPlain = 223;  // the palette's glowing end starts after this

struct Rgb {
  int r, g, b;
};

// The palette's nearest plain colour to `c`.
uint8_t nearest(std::span<const uint8_t> palette, Rgb c) {
  int best = 0;
  int64_t best_d = INT64_MAX;
  const int n = std::min<int>(kLastPlain + 1, static_cast<int>(palette.size() / 3));
  for (int i = 0; i < n; ++i) {
    const int dr = palette[size_t(i) * 3] - c.r, dg = palette[size_t(i) * 3 + 1] - c.g,
              db = palette[size_t(i) * 3 + 2] - c.b;
    // Weighted toward green as the eye is.
    const int64_t d = int64_t(dr) * dr * 3 + int64_t(dg) * dg * 4 + int64_t(db) * db * 2;
    if (d < best_d) {
      best_d = d;
      best = i;
    }
  }
  return static_cast<uint8_t>(best);
}

// The colour of the case's skin at column `x` (0..kBodyColumns-1 along it from its head; past that
// its mouth) and row `y` (round it).
Rgb casing_colour(Casing c, int x, int y) {
  const bool lit = y == 2 || y == 3;  // a band of shine along it
  const Rgb brass = lit ? Rgb{214, 178, 96} : Rgb{176, 136, 62};
  const Rgb brass_dark = {120, 88, 38};
  if (x >= kBodyColumns) return c == Casing::hull ? Rgb{92, 22, 18} : Rgb{54, 40, 22};
  if (x == 0) return brass_dark;  // the rim's edge
  if (c != Casing::hull) {
    // Brass, darker toward its mouth where the powder burnt it.
    if (x > kBodyColumns - 4) return c == Casing::heavy ? Rgb{110, 80, 40} : Rgb{140, 104, 48};
    return c == Casing::heavy ? Rgb{brass.r - 20, brass.g - 20, brass.b - 10} : brass;
  }
  // The hull: brass for its head's fifth, red plastic past it, its crimp darker.
  if (x < kBodyColumns / 5) return brass;
  if (x > kBodyColumns - 3) return Rgb{110, 26, 22};
  return lit ? Rgb{190, 48, 40} : Rgb{150, 34, 28};
}

// A decaying sine: amplitude `a`, frequency `f` Hz, decay time `tau` s (to 1/e), at `t` s.
double mode(double a, double f, double tau, double t) {
  return a * lm::exp(-t / tau) * lm::sin(2 * core::kPi * f * t);
}

}  // namespace

CasingSize casing_size(Casing c) {
  switch (c) {
    case Casing::hull:
      return {3.2f, 0.5f, "hull"};
    case Casing::brass:
      return {2.4f, 0.3f, "brass"};
    case Casing::heavy:
      return {5.0f, 0.5f, "heavy"};
  }
  return {3.2f, 0.5f, "hull"};
}

std::span<const detail::DebrisShape> casing_shapes() {
  static const std::array<detail::DebrisShape, kCasings> shapes = [] {
    std::array<detail::DebrisShape, kCasings> s{};
    for (int k = 0; k < kCasings; ++k) {
      const CasingSize z = casing_size(static_cast<Casing>(k));
      detail::DebrisShape& d = s[size_t(k)];
      d.radius_m = z.radius_u * 0.025;
      d.life_s = 12;
      d.fade_s = 2;
    }
    // Plastic bounces less than brass; the heavy case is heavy.
    s[size_t(Casing::hull)].restitution = 0.25;
    s[size_t(Casing::hull)].tumble = 30;
    s[size_t(Casing::brass)].restitution = 0.4;
    s[size_t(Casing::brass)].tumble = 60;
    s[size_t(Casing::heavy)].restitution = 0.3;
    s[size_t(Casing::heavy)].tumble = 25;
    return s;
  }();
  return {shapes.data(), shapes.size()};
}

morph::Model casing_model(Casing c, std::span<const uint8_t> palette) {
  const CasingSize z = casing_size(c);
  morph::Model m;
  morph::Skin skin;
  skin.width = kSkinW;
  skin.height = kSkinH;
  skin.indices.resize(size_t(kSkinW) * kSkinH);
  for (int y = 0; y < kSkinH; ++y)
    for (int x = 0; x < kSkinW; ++x)
      skin.indices[size_t(y) * kSkinW + size_t(x)] = nearest(palette, casing_colour(c, x, y));
  m.skins.push_back(std::move(skin));
  m.skin_width = kSkinW;
  m.skin_height = kSkinH;
  morph::Frame frame;
  frame.name = "case";
  const float half = z.length_u / 2;
  // Rings along it: its head, the hull's brass edge, its mouth.
  const float head_share = c == Casing::hull ? 0.2f : 0.5f;
  const float ring_x[3] = {-half, -half + z.length_u * head_share, half};
  const auto vertex = [&](float x, float y, float zz, float s, float t) {
    frame.positions.insert(frame.positions.end(), {x, y, zz});
    m.st.insert(m.st.end(), {s, t});
    m.source_vertex.push_back(static_cast<int>(m.source_vertex.size()));
    return static_cast<uint32_t>(m.source_vertex.size() - 1);
  };
  uint32_t ring_at[3] = {};
  for (int k = 0; k < 3; ++k) {
    const float s = (ring_x[k] + half) / z.length_u * (float(kBodyColumns) / kSkinW);
    ring_at[k] = static_cast<uint32_t>(m.source_vertex.size());
    for (int j = 0; j <= kSides; ++j) {  // the seam's vertex twice, for its own coordinate
      const double a = 2 * core::kPi * j / kSides;
      vertex(ring_x[k], float(z.radius_u * lm::cos(a)), float(z.radius_u * lm::sin(a)), s,
             (float(j) + 0.5f) / (kSides + 1));
    }
  }
  for (int k = 0; k + 1 < 3; ++k)
    for (uint32_t j = 0; j < uint32_t(kSides); ++j) {
      const uint32_t a0 = ring_at[k] + j, a1 = a0 + 1, b0 = ring_at[k + 1] + j, b1 = b0 + 1;
      m.indices.insert(m.indices.end(), {a0, b1, b0, a0, a1, b1});
    }
  // The caps: the head flat brass, the mouth dark.
  const uint32_t head = vertex(-half, 0, 0, 0.5f / kSkinW, 0.5f);
  const uint32_t mouth = vertex(half, 0, 0, (kSkinW - 1.5f) / kSkinW, 0.5f);
  for (uint32_t j = 0; j < uint32_t(kSides); ++j) {
    m.indices.insert(m.indices.end(), {head, ring_at[0] + j + 1, ring_at[0] + j});
    m.indices.insert(m.indices.end(), {mouth, ring_at[2] + j, ring_at[2] + j + 1});
  }
  frame.min = {-half, -z.radius_u, -z.radius_u};
  frame.max = {half, z.radius_u, z.radius_u};
  m.frames.push_back(std::move(frame));
  m.radius = half;
  return m;
}

audio::Pcm casing_sound(Casing c, detail::SurfaceClass on, int variant) {
  using detail::SurfaceClass;
  audio::Pcm out;
  out.rate = kCasingRate;
  const double length_s = c == Casing::heavy ? 0.45 : 0.3;
  out.samples.assign(size_t(length_s * kCasingRate), 0.0f);
  core::Rng rng = core::make_rng(0x5eed0000u + uint32_t(c) * 977u + uint32_t(on) * 131u +
                                 uint32_t(std::abs(variant) % kCasingVariants) * 7919u);
  // How much of the case's ring the surface lets out, and the surface's own knock.
  double ring = 1, knock = 0.5, knock_f = 900, knock_tau = 0.006, thud = 0.2;
  bool metal = false;
  switch (on) {
    case SurfaceClass::metal:
      metal = true;
      knock = 0.4;
      knock_f = 1400;
      break;
    case SurfaceClass::wood:
      ring = 0.55;
      knock = 0.8;
      knock_f = 420;
      knock_tau = 0.012;
      thud = 0.35;
      break;
    case SurfaceClass::dirt:
    case SurfaceClass::grass:
    case SurfaceClass::sand:
    case SurfaceClass::liquid:
      ring = 0.12;
      knock = 0.5;
      knock_f = 260;
      knock_tau = 0.01;
      thud = 0.5;
      break;
    default:  // stone, tile and the rest: hard
      break;
  }
  // The case's own modes: a free tube's partials (brass rings; the plastic hull's head rings
  // briefly under its knock).
  const double f0 = (c == Casing::brass   ? 3300
                     : c == Casing::heavy ? 1700
                                          : 2400) *
                    core::range(rng, 0.94, 1.06);
  const double partial[4] = {1, 2.76, 5.40, 8.93};
  const double tau0 = c == Casing::hull ? 0.03 : c == Casing::heavy ? 0.22 : 0.14;
  const double ring_gain = (c == Casing::hull ? 0.35 : 1.0) * ring;
  double amp[4];
  for (int k = 0; k < 4; ++k) amp[k] = ring_gain * core::range(rng, 0.5, 1.0) / (1 + k * 0.6);
  // A second strike a moment after the first (the other end coming down), quieter.
  const double second_s = core::range(rng, 0.008, 0.03);
  const double second = core::range(rng, 0.3, 0.6);
  for (size_t i = 0; i < out.samples.size(); ++i) {
    const double t = double(i) / kCasingRate;
    double v = 0;
    for (int strike = 0; strike < 2; ++strike) {
      const double ts = t - (strike == 0 ? 0 : second_s);
      if (ts < 0) continue;
      const double g = strike == 0 ? 1 : second;
      for (int k = 0; k < 4; ++k) v += g * mode(amp[k], f0 * partial[k], tau0 / (1 + k * 0.7), ts);
      v += g * mode(knock, knock_f, knock_tau, ts);
      v += g * mode(thud, 140, 0.015, ts);
      if (metal) v += g * mode(0.25, 760, 0.07, ts) + g * mode(0.15, 1930, 0.05, ts);
    }
    // The click of the touch: a short burst of noise.
    if (t < 0.002) v += core::range(rng, -0.6, 0.6) * (1 - t / 0.002);
    out.samples[i] = static_cast<float>(v);
  }
  float peak = 0;
  for (const float s : out.samples) peak = std::max(peak, std::abs(s));
  if (peak > 0)
    for (float& s : out.samples) s *= 0.6f / peak;
  // The last 5 ms faded out, so it ends in silence.
  const size_t fade = kCasingRate / 200;
  for (size_t i = 0; i < fade && i < out.samples.size(); ++i)
    out.samples[out.samples.size() - 1 - i] *= float(i) / float(fade);
  return out;
}

std::string casing_sound_name(Casing c, detail::SurfaceClass on, int variant) {
  return "ce/shell/" + std::string(casing_size(c).name) + "/" +
         std::string(detail::surface_name(on)) + "/" +
         std::to_string(std::abs(variant) % kCasingVariants);
}

bool parse_casing_sound(std::string_view name, Casing* c, detail::SurfaceClass* on, int* variant) {
  constexpr std::string_view kPrefix = "ce/shell/";
  if (!name.starts_with(kPrefix)) return false;
  name.remove_prefix(kPrefix.size());
  const size_t a = name.find('/');
  if (a == std::string_view::npos) return false;
  const size_t b = name.find('/', a + 1);
  if (b == std::string_view::npos) return false;
  const std::string_view kind = name.substr(0, a), surface = name.substr(a + 1, b - a - 1),
                         number = name.substr(b + 1);
  bool found = false;
  for (int k = 0; k < kCasings && !found; ++k)
    if (casing_size(static_cast<Casing>(k)).name == kind) {
      *c = static_cast<Casing>(k);
      found = true;
    }
  if (!found) return false;
  found = false;
  for (int k = 0; k < detail::kSurfaceClasses && !found; ++k)
    if (detail::surface_name(static_cast<detail::SurfaceClass>(k)) == surface) {
      *on = static_cast<detail::SurfaceClass>(k);
      found = true;
    }
  if (!found || number.size() != 1 || number[0] < '0' || number[0] >= char('0' + kCasingVariants))
    return false;
  *variant = number[0] - '0';
  return true;
}

}  // namespace ge::classicenhance
