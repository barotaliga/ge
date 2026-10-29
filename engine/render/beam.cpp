// The beam's presets and knobs, its level by strike, the channel's walk, its lights and burns.
//
// ge:summary Lightning beam definitions: presets, knobs, level, channel walk, lights, burns
// ge:keywords render beam lightning presets knobs strike channel walk fork lights burns ember
// ge:skill render
// ge:test tests/render_beam_test.cpp
#include "render/beam.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"
#include "core/prng.hpp"

namespace ge::render {

namespace {

namespace libm = core::libm;
constexpr double kTau = 6.28318530717958647692;
// No fork leaves the channel this near its source: by the eye a short fork fills the view.
constexpr double kForkFrom = 1.5;

// A stream for one draw site: the shot's seed, the strike and a purpose mixed into one seed.
core::Rng stream(uint32_t seed, int64_t strike, uint32_t purpose) {
  uint32_t h = seed * 0x9e3779b1u;
  h ^= static_cast<uint32_t>(strike) * 0x85ebca77u + 0x165667b1u;
  h ^= static_cast<uint32_t>(static_cast<uint64_t>(strike) >> 32) * 0xc2b2ae3du;
  h ^= purpose * 0x27d4eb2fu;
  h ^= h >> 15;
  h *= 0x2c1b3c6du;
  h ^= h >> 12;
  return core::make_rng(h);
}

double kick(core::Rng& r) { return core::next(r) * 2 - 1; }

struct Frame3 {
  double dir[3], u[3], v[3];
  double len;
};

// The beam's axis and two directions across it, the same way every time.
bool axis_of(const BeamShot& shot, Frame3& f) {
  double d[3];
  for (int k = 0; k < 3; ++k) d[k] = shot.to[k] - shot.from[k];
  f.len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  if (f.len < 1e-3) return false;
  for (int k = 0; k < 3; ++k) f.dir[k] = d[k] / f.len;
  const double up[3] = {0, 1, 0}, side[3] = {1, 0, 0};
  const double* a = std::abs(f.dir[1]) > 0.9 ? side : up;
  f.u[0] = a[1] * f.dir[2] - a[2] * f.dir[1];
  f.u[1] = a[2] * f.dir[0] - a[0] * f.dir[2];
  f.u[2] = a[0] * f.dir[1] - a[1] * f.dir[0];
  const double n = std::sqrt(f.u[0] * f.u[0] + f.u[1] * f.u[1] + f.u[2] * f.u[2]);
  for (double& c : f.u) c /= n;
  f.v[0] = f.dir[1] * f.u[2] - f.dir[2] * f.u[1];
  f.v[1] = f.dir[2] * f.u[0] - f.dir[0] * f.u[2];
  f.v[2] = f.dir[0] * f.u[1] - f.dir[1] * f.u[0];
  return true;
}

bool push(BeamShape& out, const float a[3], const float b[3], float wa, float wb, float glow,
          float sa, float sb) {
  if (out.count >= kMaxBeamSegments) return false;
  BeamSegment& s = out.seg[out.count++];
  for (int k = 0; k < 3; ++k) {
    s.a[k] = a[k];
    s.b[k] = b[k];
  }
  s.width_a = wa;
  s.width_b = wb;
  s.glow = glow;
  s.s_a = sa;
  s.s_b = sb;
  return true;
}

// A fork off point `p` (relative to the origin) at `s` along the beam: a short walk heading off
// the beam's way and out to one side, thinning and dimming to its tip.
void fork_from(const BeamLook& look, const Frame3& f, const float p[3], float s, float width,
               core::Rng& r, BeamShape& out) {
  const double ang = core::next(r) * kTau;
  const double ca = libm::cos(ang), sa = libm::sin(ang);
  double head[3];
  for (int k = 0; k < 3; ++k) head[k] = f.dir[k] * 0.55 + (f.u[k] * ca + f.v[k] * sa) * 0.85;
  const double hn = std::sqrt(head[0] * head[0] + head[1] * head[1] + head[2] * head[2]);
  const double step = std::max(0.05, look.step_m * 0.45);
  const int steps = std::clamp(static_cast<int>(std::ceil(look.fork_m / step)), 2, 12);
  float at[3] = {p[0], p[1], p[2]};
  for (int i = 1; i <= steps; ++i) {
    double next[3];
    for (int k = 0; k < 3; ++k) {
      next[k] = at[k] + (head[k] / hn) * step +
                (f.u[k] * kick(r) + f.v[k] * kick(r) + f.dir[k] * kick(r)) * step * 0.6;
    }
    const float b[3] = {static_cast<float>(next[0]), static_cast<float>(next[1]),
                        static_cast<float>(next[2])};
    const float t0 = static_cast<float>(i - 1) / static_cast<float>(steps);
    const float t1 = static_cast<float>(i) / static_cast<float>(steps);
    if (!push(out, at, b, width * (1 - 0.7f * t0), width * (1 - 0.7f * t1), 0.55f * (1 - t0),
              s + static_cast<float>(step) * static_cast<float>(i - 1),
              s + static_cast<float>(step) * static_cast<float>(i))) {
      return;
    }
    for (int k = 0; k < 3; ++k) at[k] = b[k];
  }
}

}  // namespace

std::string_view beam_look_name(int index) {
  static constexpr std::string_view kNames[kBeamLooks] = {"arc", "bolt", "braid"};
  return index >= 0 && index < kBeamLooks ? kNames[index] : std::string_view{};
}

bool beam_look(std::string_view name, BeamLook& out) {
  BeamLook l;  // the defaults are the arc
  if (name == "arc") {
    l.step_m = 0.5f;
    l.wander = 0.14f;
    l.pull = 0.5f;
    l.fork_chance = 0.03f;
    l.fork_m = 0.25f;
    l.strike_hz = 30;
    l.core_m = 0.06f;
    l.glow_m = 0.26f;
    l.glow_cd = 120;
    l.flicker = 0.25f;
    l.shimmer = 0.4f;
  } else if (name == "bolt") {
    l.step_m = 0.4f;
    l.wander = 0.45f;
    l.pull = 0.32f;
    l.fork_chance = 0.06f;
    l.fork_m = 0.6f;
    l.strike_hz = 14;
    l.core_m = 0.04f;
    l.glow_m = 0.22f;
    l.core_cd = 9e4f;
    l.glow_cd = 100;
    l.core_rgb[0] = 0.92f;
    l.core_rgb[1] = 0.92f;
    l.core_rgb[2] = 1.0f;
    l.glow_rgb[0] = 0.6f;
    l.glow_rgb[1] = 0.55f;
    l.glow_rgb[2] = 1.0f;
    l.flash = 1.4f;
    l.flash_s = 0.02f;
    l.flicker = 0.45f;
    l.shimmer = 0.2f;
    l.light_rgb[0] = 0.7f;
    l.light_rgb[1] = 0.68f;
    l.light_rgb[2] = 1.0f;
  } else if (name == "braid") {
    l.step_m = 0.12f;  // fine enough for the helix: a quarter turn takes three steps
    l.wander = 0.16f;
    l.pull = 0.25f;
    l.filaments = 3;
    l.braid_m = 0.05f;
    l.twist = 0.7f;
    l.spin_hz = 2.5f;
    l.fork_chance = 0.01f;
    l.fork_m = 0.35f;
    l.strike_hz = 40;
    l.core_m = 0.022f;
    l.glow_m = 0.3f;
    l.core_cd = 5e4f;
    l.glow_cd = 110;
    l.core_rgb[0] = 0.8f;
    l.core_rgb[1] = 0.95f;
    l.core_rgb[2] = 1.0f;
    l.glow_rgb[0] = 0.3f;
    l.glow_rgb[1] = 0.65f;
    l.glow_rgb[2] = 1.0f;
    l.flicker = 0.2f;
    l.shimmer = 0.55f;
    l.light_rgb[0] = 0.45f;
    l.light_rgb[1] = 0.75f;
    l.light_rgb[2] = 1.0f;
  } else {
    return false;
  }
  out = l;
  return true;
}

const BeamKnob* beam_knobs() {
  static constexpr BeamKnob kKnobs[kBeamKnobs] = {
      {"step_m", &BeamLook::step_m, 0.05f, 4},
      {"wander", &BeamLook::wander, 0, 2},
      {"pull", &BeamLook::pull, 0, 1},
      {"filaments", &BeamLook::filaments, 1, kBeamFilaments},
      {"braid_m", &BeamLook::braid_m, 0, 1},
      {"twist", &BeamLook::twist, 0, 10},
      {"spin_hz", &BeamLook::spin_hz, -20, 20},
      {"fork_chance", &BeamLook::fork_chance, 0, 0.5f},
      {"fork_m", &BeamLook::fork_m, 0.1f, 5},
      {"strike_hz", &BeamLook::strike_hz, 0, 120},
      {"core_m", &BeamLook::core_m, 0.002f, 0.5f},
      {"glow_m", &BeamLook::glow_m, 0.01f, 3},
      {"core_cd", &BeamLook::core_cd, 0, 1e7f},
      {"glow_cd", &BeamLook::glow_cd, 0, 1e6f},
      {"flash", &BeamLook::flash, 0, 4},
      {"flash_s", &BeamLook::flash_s, 0.001f, 0.2f},
      {"flicker", &BeamLook::flicker, 0, 1},
      {"shimmer", &BeamLook::shimmer, 0, 1},
      {"attack_s", &BeamLook::attack_s, 0.001f, 1},
      {"release_s", &BeamLook::release_s, 0.001f, 2},
      {"light_cd", &BeamLook::light_cd, 0, 1e6f},
      {"light_every_m", &BeamLook::light_every_m, 0.5f, 50},
      {"hit_cd", &BeamLook::hit_cd, 0, 1e6f},
      {"root_cd", &BeamLook::root_cd, 0, 1e6f},
      {"scorch_m", &BeamLook::scorch_m, 0.05f, 4},
      {"scorch_every_s", &BeamLook::scorch_every_s, 0.02f, 10},
      {"ember_s", &BeamLook::ember_s, 0, 20},
      {"ember_cd", &BeamLook::ember_cd, 0, 1e5f},
  };
  return kKnobs;
}

bool set_beam_knob(BeamLook& look, std::string_view name, float value) {
  const BeamKnob* k = beam_knobs();
  for (int i = 0; i < kBeamKnobs; ++i) {
    if (k[i].name == name) {
      look.*(k[i].field) = std::clamp(value, k[i].lo, k[i].hi);
      return true;
    }
  }
  return false;
}

int64_t beam_strike(const BeamLook& look, const BeamShot& shot, double t_s) {
  if (look.strike_hz <= 0 || t_s <= shot.born_s) return 0;
  return static_cast<int64_t>(std::floor((t_s - shot.born_s) * look.strike_hz));
}

float beam_level(const BeamLook& look, const BeamShot& shot, double t_s) {
  if (t_s < shot.born_s) return 0;
  const double age = t_s - shot.born_s;
  double level = std::min(1.0, age / std::max(1e-4, static_cast<double>(look.attack_s)));
  if (shot.end_s >= 0 && t_s > shot.end_s) {
    const double gone = (t_s - shot.end_s) / std::max(1e-4, static_cast<double>(look.release_s));
    if (gone >= 1) return 0;
    level *= (1 - gone) * (1 - gone);
  }
  const int64_t k = beam_strike(look, shot, t_s);
  core::Rng r = stream(shot.seed, k, 1);
  level *= 1 + look.flicker * kick(r);
  const double into = look.strike_hz > 0 ? age - static_cast<double>(k) / look.strike_hz : age;
  level *= 1 + look.flash * libm::exp(-into / std::max(1e-4, static_cast<double>(look.flash_s)));
  return static_cast<float>(std::max(0.0, level));
}

void build_beam(const BeamLook& look, const BeamShot& shot, double t_s, BeamShape& out) {
  out.count = 0;
  for (int k = 0; k < 3; ++k) out.origin[k] = shot.from[k];
  out.level = beam_level(look, shot, t_s);
  out.strike = beam_strike(look, shot, t_s);
  Frame3 f;
  if (out.level <= 0 || !axis_of(shot, f)) return;
  const double step = std::max(0.05, static_cast<double>(look.step_m));
  const int n = std::max(2, static_cast<int>(std::ceil(f.len / step)));
  const double ds = f.len / n;
  const int fils = std::clamp(static_cast<int>(std::lround(look.filaments)), 1, kBeamFilaments);
  // Several filaments share the beam's light: each a little thinner, the glow theirs together.
  const float width = fils > 1 ? 0.75f : 1.0f;
  // The ends ease in over this far, so the channel leaves the source and lands on the hit.
  const double ease = std::min(f.len * 0.5, 1.5 * step);
  const double spin = static_cast<double>(look.spin_hz) * (t_s - shot.born_s);
  for (int fil = 0; fil < fils; ++fil) {
    core::Rng r = stream(shot.seed, out.strike, 16 + static_cast<uint32_t>(fil));
    const double phase = kTau * fil / fils;
    double ox = 0, oy = 0;
    float prev[3] = {0, 0, 0};
    for (int i = 1; i <= n; ++i) {
      const double s = ds * i;
      ox = (ox + kick(r) * look.wander * ds) * (1 - look.pull);
      oy = (oy + kick(r) * look.wander * ds) * (1 - look.pull);
      const double e = i == n ? 0 : std::min(1.0, std::min(s, f.len - s) / std::max(ease, 1e-6));
      double hx = 0, hy = 0;
      if (fils > 1 && look.braid_m > 0) {
        const double a = kTau * (look.twist * s + spin) + phase;
        hx = look.braid_m * libm::cos(a);
        hy = look.braid_m * libm::sin(a);
      }
      float p[3];
      for (int k = 0; k < 3; ++k) {
        p[k] = static_cast<float>(f.dir[k] * s + f.u[k] * (ox + hx) * e + f.v[k] * (oy + hy) * e);
      }
      if (!push(out, prev, p, width, width, 1.0f, static_cast<float>(s - ds),
                static_cast<float>(s))) {
        return;
      }
      if (i < n - 1 && s > kForkFrom && core::chance(r, look.fork_chance)) {
        fork_from(look, f, p, static_cast<float>(s), width * 0.6f, r, out);
      }
      for (int k = 0; k < 3; ++k) prev[k] = p[k];
    }
  }
}

int beam_lights(const BeamLook& look, const BeamShot& shot, double t_s, LocalLight* out) {
  const float level = beam_level(look, shot, t_s);
  Frame3 f;
  if (level <= 0 || !axis_of(shot, f)) return 0;
  const int along =
      std::clamp(static_cast<int>(std::lround(f.len / std::max(0.5f, look.light_every_m))), 1,
                 kBeamLights - 2);
  auto lamp = [&](LocalLight& l, const double at[3], float cd, float radius, const float rgb[3]) {
    l = LocalLight{};
    for (int k = 0; k < 3; ++k) {
      l.position[k] = at[k];
      l.colour[k] = rgb[k];
    }
    l.candela = cd;
    l.radius_m = radius;
    // Its light falls to half a lux at the end of its reach.
    l.range_m = std::clamp(std::sqrt(cd / 0.5f), 2.0f, 40.0f);
    l.flicker = Flicker::steady;
  };
  int n = 0;
  if (look.root_cd > 0) {
    // The arc's root at the muzzle: what lights the thing that fires it.
    const double at[3] = {shot.from[0] + f.dir[0] * 0.06, shot.from[1] + f.dir[1] * 0.06,
                          shot.from[2] + f.dir[2] * 0.06};
    lamp(out[n++], at, look.root_cd * level, 0.05f, look.light_rgb);
  }
  const float share = look.light_cd * level / static_cast<float>(along);
  const float soft = static_cast<float>(f.len / along * 0.5);
  for (int i = 0; i < along; ++i) {
    const double s = f.len * (i + 0.5) / along;
    const double at[3] = {shot.from[0] + f.dir[0] * s, shot.from[1] + f.dir[1] * s,
                          shot.from[2] + f.dir[2] * s};
    lamp(out[n++], at, share, soft, look.light_rgb);
  }
  if (look.hit_cd > 0) {
    // A little off the surface, so the wall round the hit takes it and not the wall alone.
    const double at[3] = {shot.to[0] + shot.normal[0] * 0.15, shot.to[1] + shot.normal[1] * 0.15,
                          shot.to[2] + shot.normal[2] * 0.15};
    const float warm[3] = {0.5f + 0.5f * look.light_rgb[0], 0.5f + 0.5f * look.light_rgb[1],
                           0.5f + 0.5f * look.light_rgb[2]};
    lamp(out[n++], at, look.hit_cd * level, 0.1f, warm);
  }
  return n;
}

bool beam_burns(const BeamLook& look, const BeamShot& shot, double t_s, BurnTrail& trail,
                BeamBurn& out) {
  if (t_s < shot.born_s || (shot.end_s >= 0 && t_s > shot.end_s)) return false;
  double at[3] = {shot.to[0], shot.to[1], shot.to[2]};
  if (trail.any) {
    double d2 = 0;
    for (int k = 0; k < 3; ++k)
      d2 += (shot.to[k] - trail.last_at[k]) * (shot.to[k] - trail.last_at[k]);
    const double step = look.scorch_m / 3.0;
    if (d2 < step * step && t_s - trail.last_s < look.scorch_every_s) return false;
    // A hit that jumped further than two steps since the last burn: the next burn half a scorch
    // on toward it, so a caller asking again until this says no fills the gap.
    const double d = std::sqrt(d2);
    if (d > 2 * step) {
      for (int k = 0; k < 3; ++k) {
        at[k] = trail.last_at[k] + (shot.to[k] - trail.last_at[k]) * (1.5 * step / d);
      }
    }
  }
  core::Rng r = stream(shot.seed, trail.count, 7);
  out = BeamBurn{};
  for (int k = 0; k < 3; ++k) {
    out.at[k] = at[k];
    out.normal[k] = shot.normal[k];
    trail.last_at[k] = at[k];
  }
  out.size_m = look.scorch_m * (0.85 + 0.3 * core::next(r));
  out.spin_rad = core::next(r) * kTau;
  out.born_s = t_s;
  out.variant = static_cast<uint32_t>(core::next(r) * 1024);
  trail.any = true;
  trail.last_s = t_s;
  ++trail.count;
  return true;
}

float ember_level(const BeamLook& look, double age_s) {
  if (age_s < 0 || look.ember_s <= 0) return 0;
  const double x = age_s / look.ember_s;
  if (x >= 1) return 0;
  return static_cast<float>(look.ember_cd * (1 - x) * (1 - x) * (1 - x));
}

}  // namespace ge::render
