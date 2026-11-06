// Particle effects as data: the text read, a burst or a trail thrown (effect.hpp).
//
// ge:summary The effect text read into a book, faults by line; bursts and trails thrown on a seed
// ge:keywords parse_effects throw_effect throw_trail effect-text fault palette srgb
// ge:skill render
// ge:test tests/particles_effect_test.cpp
#include "particles/effect.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <utility>

#include "core/libm/libm.hpp"
#include "core/prng.hpp"

namespace ge::particles {

namespace {

constexpr double kGravity = 9.81;  // m/s²: the render pool's (render/particles.hpp)

std::vector<std::string_view> words_of(std::string_view line) {
  std::vector<std::string_view> out;
  size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r')) i++;
    size_t j = i;
    while (j < line.size() && line[j] != ' ' && line[j] != '\t' && line[j] != '\r') j++;
    if (j > i) out.push_back(line.substr(i, j - i));
    i = j;
  }
  return out;
}

bool number(std::string_view w, double& out) {
  const char* end = w.data() + w.size();
  const auto r = std::from_chars(w.data(), end, out);
  return r.ec == std::errc() && r.ptr == end && std::isfinite(out);
}

bool good_name(std::string_view n) {
  if (n.empty()) return false;
  for (const char c : n)
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-'))
      return false;
  return true;
}

// How many numbers each key takes (lo, hi), and how one is written.
struct Key {
  std::string_view name;
  int lo, hi;
  std::string_view example;
  bool emitter;  // only inside an emitter
};
constexpr Key kKeys[] = {
    {"light", 4, 5, "light 300 1 0.8 0.5 0.5", false},
    {"burst", 1, 1, "burst 64", false},
    {"trail", 1, 1, "trail 3", false},
    {"box", 1, 1, "box 16", true},
    {"ball", 1, 1, "ball 16", true},
    {"speed", 2, 2, "speed 0 256", true},
    {"along", 2, 2, "along 40 120", true},
    {"rise", 2, 2, "rise 10 30", true},
    {"life", 2, 2, "life 0.3 0.8", true},
    {"size", 2, 2, "size 2 3", true},
    {"fall", 1, 1, "fall 40", true},
    {"drag", 1, 1, "drag 1", true},
    {"colour", 3, 6, "colour 1 0.6 0.2", true},
    {"palette", 1, 2, "palette 104 111", true},
    {"shine", 1, 1, "shine 1", true},
    {"smoke", 2, 2, "smoke 0.4 0.6", true},
    {"cool", 1, 2, "cool 0.3", true},
    {"grow", 1, 1, "grow 2", true},
    {"streak", 1, 1, "streak 0.02", true},
    {"bounce", 1, 1, "bounce 0.4", true},
};

const Key* key_of(std::string_view w) {
  for (const Key& k : kKeys)
    if (k.name == w) return &k;
  return nullptr;
}

std::string at_line(std::string_view file, int line) {
  return std::string(file) + ":" + std::to_string(line) + ": ";
}

bool positive_range(const Range& r) { return r.lo >= 0 && r.hi >= r.lo; }

double to_linear(double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); }

void palette_rgb(std::span<const uint8_t> pal, int index, float out[3]) {
  index = std::clamp(index, 0, 255);
  for (size_t k = 0; k < 3; ++k) {
    const double c =
        pal.size() >= 768 ? pal[static_cast<size_t>(index) * 3 + k] / 255.0 : index / 255.0;
    out[k] = static_cast<float>(to_linear(c));
  }
}

double pick(core::Rng& r, const Range& range) { return core::range(r, range.lo, range.hi); }

// A way picked evenly over the sphere.
void any_way(core::Rng& r, double out[3]) {
  const double y = core::range(r, -1, 1);
  const double a = core::range(r, 0, 6.283185307179586);
  const double s = std::sqrt(std::max(0.0, 1 - y * y));
  out[0] = s * core::libm::cos(a);
  out[1] = y;
  out[2] = s * core::libm::sin(a);
}

// One speck of `m` at `p` (engine metres), the effect's way `way`.
Spark speck(const Emitter& m, const double p[3], const double way[3], const Throw& t,
            core::Rng& r) {
  Spark s{};
  const double u = t.metres;
  double off[3] = {0, 0, 0};
  if (m.shape == Shape::box) {
    for (double& o : off) o = core::range(r, -m.extent, m.extent);
  } else if (m.shape == Shape::ball) {
    double w[3];
    any_way(r, w);
    const double d = m.extent * std::cbrt(core::next(r));
    for (int k = 0; k < 3; ++k) off[k] = w[k] * d;
  }
  double out[3];
  any_way(r, out);
  const double speed = pick(r, m.speed), along = pick(r, m.along), rise = pick(r, m.rise);
  for (int k = 0; k < 3; ++k) {
    s.p[k] = static_cast<float>(p[k] + off[k] * u);
    s.v[k] = static_cast<float>((out[k] * speed + way[k] * along + (k == 1 ? rise : 0)) * u);
  }
  s.life = static_cast<float>(std::max(0.01, pick(r, m.life)));
  s.size = static_cast<float>(pick(r, m.size) * u);
  s.drag = static_cast<float>(m.drag);
  s.fall = static_cast<float>(m.falls_set ? m.fall * u / kGravity : 1.0);
  s.streak = static_cast<float>(m.streak);
  s.grow = static_cast<float>(m.grow * u);
  s.floor_y = static_cast<float>(t.floor_y);
  s.bounce = static_cast<float>(m.bounce);
  if (m.palette[0] >= 0) {
    const int n = m.palette[1] - m.palette[0] + 1;
    const int i = m.palette[0] + std::min(n - 1, static_cast<int>(core::next(r) * n));
    palette_rgb(t.palette, i, s.tint);
  } else {
    const double c = core::next(r);
    for (int k = 0; k < 3; ++k)
      s.tint[k] = static_cast<float>(m.colour[0][k] + (m.colour[1][k] - m.colour[0][k]) * c);
  }
  s.cool_s = static_cast<float>(m.cool);
  if (m.smoke) {
    s.albedo = static_cast<float>(m.albedo);
    s.opacity = static_cast<float>(m.opacity);
    s.kelvin = 0;
  } else {
    s.albedo = static_cast<float>(m.albedo);  // the grey it cools toward
    s.opacity = 1;
    s.kelvin = static_cast<float>(-m.shine);
  }
  return s;
}

}  // namespace

const Effect* Book::find(std::string_view name) const {
  for (const Effect& e : effects)
    if (e.name == name) return &e;
  return nullptr;
}

bool parse_effects(std::string_view text, std::string_view file, Book& book, std::string& error) {
  std::vector<Effect> read;
  int line_no = 0;
  size_t at = 0;
  while (at <= text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string_view::npos) end = text.size();
    std::string_view line = text.substr(at, end - at);
    at = end + 1;
    line_no++;
    if (const size_t hash = line.find('#'); hash != std::string_view::npos)
      line = line.substr(0, hash);
    const std::vector<std::string_view> w = words_of(line);
    if (w.empty()) {
      if (end == text.size()) break;
      continue;
    }
    const std::string where = at_line(file, line_no);
    if (w[0] == "effect") {
      if (w.size() != 2 || !good_name(w[1])) {
        error = where + "`effect` takes one name of lower-case letters, digits, `_`, `.` or `-`; " +
                "next: write `effect explosion`";
        return false;
      }
      Effect e;
      e.name = std::string(w[1]);
      e.line = line_no;
      e.file = std::string(file);
      read.push_back(std::move(e));
      continue;
    }
    const Key* k = key_of(w[0]);
    if (k == nullptr) {
      error = where + "`" + std::string(w[0]) +
              "` is not a word an effect knows; next: one of effect, light, burst, trail, box, " +
              "ball, speed, along, rise, life, size, fall, drag, colour, palette, shine, smoke, " +
              "cool, grow, streak, bounce (engine/particles/effect.hpp)";
      return false;
    }
    if (read.empty()) {
      error = where + "`" + std::string(w[0]) + "` comes before any `effect <name>`; next: begin " +
              "the effect first";
      return false;
    }
    Effect& e = read.back();
    const int got = static_cast<int>(w.size()) - 1;
    if (got < k->lo || got > k->hi || (k->name == "colour" && got != 3 && got != 6)) {
      error = where + "`" + std::string(k->name) + "` takes " +
              (k->lo == k->hi ? std::to_string(k->lo)
                              : std::to_string(k->lo) + " to " + std::to_string(k->hi)) +
              " numbers, got " + std::to_string(got) + "; next: write `" + std::string(k->example) +
              "`";
      return false;
    }
    double n[6] = {0, 0, 0, 0, 0, 0};
    for (int i = 0; i < got; i++)
      if (!number(w[static_cast<size_t>(i) + 1], n[i])) {
        error = where + "`" + std::string(w[static_cast<size_t>(i) + 1]) +
                "` is not a number; next: write `" + std::string(k->example) + "`";
        return false;
      }
    const auto fault = [&](const std::string& what) {
      error = where;
      error += "`";
      error += k->name;
      error += "`: ";
      error += what;
      error += "; next: write `";
      error += k->example;
      error += "`";
      return false;
    };
    if (k->emitter && e.emitters.empty())
      return fault(
          "it belongs to an emitter, and the effect has none yet (begin one with "
          "`burst <count>` or `trail <spacing>`)");
    Emitter* m = e.emitters.empty() ? nullptr : &e.emitters.back();
    const Range range{n[0], n[1]};
    const std::string_view name = k->name;
    if (name == "light") {
      if (n[0] <= 0 || n[1] < 0 || n[2] < 0 || n[3] < 0 || (got == 5 && n[4] <= 0))
        return fault("a radius and a fade above 0, a colour not below it");
      e.light_radius = n[0];
      for (int i = 0; i < 3; i++) e.light_rgb[i] = static_cast<float>(n[i + 1]);
      if (got == 5) e.light_fade = n[4];
    } else if (name == "burst" || name == "trail") {
      Emitter fresh;
      fresh.trail = name == "trail";
      if (fresh.trail) {
        if (n[0] <= 0) return fault("the spacing must be above 0");
        fresh.spacing = n[0];
      } else {
        if (n[0] < 1 || n[0] > 65536 || n[0] != std::floor(n[0]))
          return fault("the count is a whole number from 1 to 65536");
        fresh.count = static_cast<int>(n[0]);
      }
      e.emitters.push_back(fresh);
    } else if (name == "box" || name == "ball") {
      if (n[0] < 0) return fault("the size must not be below 0");
      m->shape = name == "box" ? Shape::box : Shape::ball;
      m->extent = n[0];
    } else if (name == "speed" || name == "along" || name == "rise") {
      if (range.hi < range.lo) return fault("the least first, then the most");
      (name == "speed" ? m->speed : name == "along" ? m->along : m->rise) = range;
    } else if (name == "life" || name == "size") {
      if (!positive_range(range) || range.hi <= 0)
        return fault("the least first, then the most, neither below 0 and the most above it");
      (name == "life" ? m->life : m->size) = range;
    } else if (name == "fall") {
      m->fall = n[0];
      m->falls_set = true;
    } else if (name == "drag") {
      m->drag = n[0];
    } else if (name == "colour") {
      for (int i = 0; i < 6; i++)
        if (n[i] < 0) return fault("no colour below 0");
      for (int i = 0; i < 3; i++) {
        m->colour[0][i] = static_cast<float>(n[i]);
        m->colour[1][i] = static_cast<float>(got == 6 ? n[i + 3] : n[i]);
      }
      m->palette[0] = m->palette[1] = -1;
    } else if (name == "palette") {
      const double last = got == 2 ? n[1] : n[0];
      if (n[0] < 0 || last > 255 || last < n[0] || n[0] != std::floor(n[0]) ||
          last != std::floor(last))
        return fault("entries are whole numbers 0 to 255, the first not after the last");
      m->palette[0] = static_cast<int>(n[0]);
      m->palette[1] = static_cast<int>(last);
    } else if (name == "shine") {
      if (n[0] <= 0) return fault("the light must be above 0");
      m->smoke = false;
      m->shine = n[0];
    } else if (name == "smoke") {
      if (n[0] < 0 || n[0] > 1 || n[1] <= 0 || n[1] > 1)
        return fault("a whiteness 0 to 1 and an opacity above 0, up to 1");
      m->smoke = true;
      m->albedo = n[0];
      m->opacity = n[1];
    } else if (name == "cool") {
      if (n[0] < 0 || (got == 2 && (n[1] < 0 || n[1] > 1)))
        return fault("seconds not below 0, and a grey 0 to 1");
      m->cool = n[0];
      if (got == 2) m->albedo = n[1];
    } else if (name == "grow") {
      m->grow = n[0];
    } else if (name == "streak") {
      if (n[0] < 0) return fault("seconds not below 0");
      m->streak = n[0];
    } else if (name == "bounce") {
      if (n[0] < 0 || n[0] > 1) return fault("a share 0 to 1");
      m->bounce = n[0];
    }
    if (end == text.size()) break;
  }
  for (const Effect& e : read)
    if (e.emitters.empty() && e.light_radius <= 0) {
      error = at_line(file, e.line) + "the effect `" + e.name +
              "` throws nothing; next: give it a `burst <count>` or `trail <spacing>` emitter, " +
              "or a `light`";
      return false;
    }
  for (Effect& e : read) {
    std::erase_if(book.effects, [&](const Effect& old) { return old.name == e.name; });
    book.effects.push_back(std::move(e));
  }
  return true;
}

void throw_effect(const Effect& e, const Throw& t, std::vector<Spark>& out,
                  std::vector<Glow>* glows) {
  core::Rng r = core::make_rng(t.seed);
  for (const Emitter& m : e.emitters) {
    if (m.trail) continue;
    const int n = m.count * std::max(1, t.times);
    for (int i = 0; i < n; i++) out.push_back(speck(m, t.at, t.way, t, r));
  }
  if (glows != nullptr && e.light_radius > 0) {
    Glow g;
    for (int k = 0; k < 3; k++) {
      g.at[k] = t.at[k];
      g.rgb[k] = e.light_rgb[k];
    }
    g.radius_m = e.light_radius * t.metres;
    g.fade_s = e.light_fade;
    glows->push_back(g);
  }
}

void throw_trail(const Effect& e, const double from[3], const double to[3], const Throw& t,
                 std::vector<Spark>& out) {
  core::Rng r = core::make_rng(t.seed);
  const double d[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
  const double len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  double way[3] = {0, 1, 0};
  if (len > 1e-9)
    for (int k = 0; k < 3; k++) way[k] = d[k] / len;
  for (const Emitter& m : e.emitters) {
    if (!m.trail) continue;
    const double gap = m.spacing * t.metres;
    const int n = std::max(1, static_cast<int>(len / gap + 1e-9));  // a whole stretch counted whole
    for (int i = 0; i < n; i++) {
      const double s = (i + core::next(r)) / n;
      const double p[3] = {from[0] + d[0] * s, from[1] + d[1] * s, from[2] + d[2] * s};
      out.push_back(speck(m, p, way, t, r));
    }
  }
}

}  // namespace ge::particles
