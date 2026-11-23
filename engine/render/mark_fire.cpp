// A hot mark's flames and then its smoke: the envelope, the flames on their spots, the wisps.
//
// ge:summary A hot mark's flames and smoke: the envelope, gas flames on fixed spots, wisps
// ge:keywords render marks burn smoke flames mark_fire_births mark_flames smoulders
// ge:skill render detail
// ge:test tests/render_mark_fire_test.cpp
#include "render/mark_fire.hpp"

#include <algorithm>
#include <cmath>

namespace ge::render {

namespace {

double rnd(core::Rng& r, double lo, double hi) { return core::range(r, lo, hi); }

double smooth(double a, double b, double x) {
  if (b <= a) return x >= b ? 1 : 0;
  const double t = std::clamp((x - a) / (b - a), 0.0, 1.0);
  return t * t * (3 - 2 * t);
}

// Births owed by `debt` after `rate` a second over `dt`: the whole ones, the rest carried.
int owed(double& debt, double rate, double dt) {
  debt += std::max(rate, 0.0) * dt;
  const int n = static_cast<int>(debt);
  debt -= n;
  return std::min(n, 16);
}

// A point on one of slot `i`'s triangles where its mask's `ember` (`fire`) or `cover` holds, `lift`
// m out of the surface; false when `tries` found none.
bool point_on(const detail::DecalPool& pool, uint32_t i, bool fire, double lift, int tries,
              core::Rng& r, double out[3]) {
  const detail::Decal& d = pool.decals[i];
  const auto verts = detail::decal_verts(pool, i);
  const int tris = static_cast<int>(verts.size() / 3);
  if (tris <= 0) return false;
  for (int attempt = 0; attempt < tries; ++attempt) {
    const size_t t = static_cast<size_t>(core::int_in(r, 0, tris - 1)) * 3;
    double a = rnd(r, 0, 1);
    double b = rnd(r, 0, 1);
    if (a + b > 1) a = 1 - a, b = 1 - b;
    const double c = 1 - a - b;
    const detail::DecalVertex &p = verts[t], &q = verts[t + 1], &w = verts[t + 2];
    const double u = c * p.u + a * q.u + b * w.u, v = c * p.v + a * q.v + b * w.v;
    const double fade = c * p.fade + a * q.fade + b * w.fade;
    const detail::MaskSample m = detail::mask_at(d.kind, d.variant, u, v);
    const double keep = (fire ? m.ember : m.cover) * fade;
    if (rnd(r, 0, 1) >= keep) continue;
    out[0] = c * p.pos.x + a * q.pos.x + b * w.pos.x + d.normal.x * lift;
    out[1] = c * p.pos.y + a * q.pos.y + b * w.pos.y + d.normal.y * lift;
    out[2] = c * p.pos.z + a * q.pos.z + b * w.pos.z + d.normal.z * lift;
    return true;
  }
  return false;
}

double glow_of(const detail::Decal& d) {
  return d.ember_s > 0 ? d.ember_s : detail::decal_spec(d.kind).ember_s;
}

double area_of(const detail::Decal& d) {
  return d.size_m * (d.length_m > 0 ? d.length_m : d.size_m) * 0.75;
}

}  // namespace

const MarkFireLook& mark_fire_look() {
  static const MarkFireLook look{};
  return look;
}

bool smoulders(detail::DecalKind kind) {
  return kind == detail::DecalKind::burn_line || kind == detail::DecalKind::melt;
}

MarkFireLevel mark_fire_level(const MarkFireLook& look, double age_s, double glow_s) {
  MarkFireLevel l;
  if (glow_s <= 0 || age_s < 0) return l;
  const double x = age_s / glow_s;
  l.fire = 1 - std::clamp(x / look.fire_until, 0.0, 1.0);
  const double up = smooth(look.smoke_from, look.smoke_peak, x);
  const double down =
      1 - std::clamp((x - look.smoke_peak) / (look.smoke_until - look.smoke_peak), 0.0, 1.0);
  l.smoke = up * down * down;
  return l;
}

size_t mark_flames(const MarkFireLook& look, const detail::DecalPool& pool, double now_s,
                   std::vector<Flame>& out) {
  const size_t before = out.size();
  for (uint32_t i = 0; i < pool.capacity; ++i) {
    const detail::Decal& d = pool.decals[i];
    if (!d.live || !smoulders(d.kind) || d.normal.y < -0.5) continue;
    const double fire = mark_fire_level(look, now_s - d.born_s, glow_of(d)).fire;
    if (fire <= 0) continue;
    const double area = area_of(d);
    const int spots = std::clamp(static_cast<int>(std::lround(area / look.flame_area_m2)), 1,
                                 look.flames_per_mark);
    const double tall = std::min(
        look.flame_height_m * std::sqrt(std::max(area / look.area_m2, 1.0)), look.flame_tallest_m);
    for (int k = 0; k < spots; ++k) {
      if (static_cast<int>(out.size() - before) >= look.most_flames) return out.size() - before;
      // The spot's own place and strength, the same every frame for this mark.
      core::Rng r =
          core::make_rng(d.serial * 2654435761u + static_cast<uint32_t>(k) * 40503u + 11u);
      const double strength = k == 0 ? 1.0 : rnd(r, 0.45, 0.95);
      // The weak ones go out first: each burns while the fire is over its cut.
      const double cut = 0.75 * (1 - strength);
      if (fire <= cut) continue;
      const double left = (fire - cut) / (1 - cut);
      const double h = tall * strength * (0.35 + 0.65 * std::sqrt(left));
      const double radius = 0.22 * h;
      double base[3];
      if (!point_on(pool, i, true, 0.6 * radius, 12, r, base)) {
        if (k > 0) continue;  // the first always burns: at the mark's middle
        base[0] = d.at.x + d.normal.x * 0.6 * radius;
        base[1] = d.at.y + d.normal.y * 0.6 * radius;
        base[2] = d.at.z + d.normal.z * 0.6 * radius;
      }
      Flame f = candle_flame(base, d.serial * 7u + static_cast<uint32_t>(k));
      f.height_m = static_cast<float>(h);
      f.radius_m = static_cast<float>(radius);
      f.temperature_k =
          static_cast<float>(look.flame_cool_k + (look.flame_k - look.flame_cool_k) * left);
      f.blue = 0;  // soot and molten metal, no gas's blue base
      f.depth = 0.3f;
      f.rise_mps = 0.6f;
      f.puff_hz = flame_puff_hz(2.0f * f.radius_m);
      out.push_back(f);
    }
  }
  return out.size() - before;
}

size_t mark_fire_births(MarkFire& s, const MarkFireLook& look, const detail::DecalPool& pool,
                        double now_s, double dt, std::vector<Particle>& out) {
  if (dt <= 0 || dt > look.max_dt_s) return 0;
  const size_t before = out.size();
  if (s.serial.size() != pool.capacity) {
    s.serial.assign(pool.capacity, 0);
    s.debt.assign(pool.capacity, 0);
  }
  core::Rng& r = s.rng;
  for (uint32_t i = 0; i < pool.capacity; ++i) {
    const detail::Decal& d = pool.decals[i];
    if (!d.live || !smoulders(d.kind)) continue;
    if (s.serial[i] != d.serial) {  // a new mark in the slot: nothing owed yet
      s.serial[i] = d.serial;
      s.debt[i] = 0;
    }
    const MarkFireLevel lv = mark_fire_level(look, now_s - d.born_s, glow_of(d));
    if (lv.smoke <= 0) continue;
    const double scale = std::min(area_of(d) / look.area_m2, look.area_cap);
    const int wisps = owed(s.debt[i], look.smoke_per_s * scale * lv.smoke, dt);
    const double n[3] = {d.normal.x, d.normal.y, d.normal.z};
    // Thin grey wisps rising off it and spreading as they go.
    for (int k = 0; k < wisps && static_cast<int>(out.size() - before) < look.most; ++k) {
      double p[3];
      if (!point_on(pool, i, false, 0.01, 4, r, p)) continue;
      Particle q{};
      const double out_v = rnd(r, 0.04, 0.1);
      const double up_v = rnd(r, 0.18, 0.36);
      for (int c = 0; c < 3; ++c) {
        q.p[c] = static_cast<float>(p[c]);
        q.v[c] = static_cast<float>(n[c] * out_v + (c == 1 ? up_v : 0));
      }
      q.v[0] += static_cast<float>(rnd(r, -0.05, 0.05));
      q.v[2] += static_cast<float>(rnd(r, -0.05, 0.05));
      q.life = static_cast<float>(rnd(r, 1.4, 2.4));
      q.size = static_cast<float>(rnd(r, 0.08, 0.14));
      q.grow = static_cast<float>(rnd(r, 0.2, 0.35));
      q.drag = 1.2f;
      q.fall = -0.04f;  // warm: it rises
      q.albedo = static_cast<float>(look.smoke_albedo * rnd(r, 0.85, 1.15));
      q.opacity = static_cast<float>(look.smoke_opacity * rnd(r, 0.7, 1.0));
      q.swirl = 0.15f;
      out.push_back(q);
    }
  }
  return out.size() - before;
}

}  // namespace ge::render
