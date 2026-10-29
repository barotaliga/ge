// Lightning's definitions: the slot clock and the flash dealt in a slot (its kind, its ends, its
// strokes), the light integrated in closed form, and the channel's stepped walk with its branches.
//
// ge:summary Lightning: the slot clock, the flash's draws, closed-form light, the channel's walk
// ge:keywords lightning flash slot stroke light_over build_channel stepped-walk branch
// ge:skill water
// ge:test tests/wind_lightning_test.cpp
#include "wind/lightning.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/prng.hpp"

namespace ge::wind {

namespace {

namespace lm = core::libm;

constexpr double kTau = 6.283185307179586;
constexpr double kLeaderGlow = 0.015;  // the stepped leader's power, in the unit
constexpr double kHalfReach = 4000;    // m, where a flash's light at a viewer is half
constexpr double kLastStroke = 1.2;    // s, no stroke after this into a flash
constexpr double kStep = 60;           // m, a ground channel's step
constexpr double kCloudStep = 80;      // m, a cloud channel's step

uint32_t word_of(double unit) { return static_cast<uint32_t>(unit * 4294967296.0); }

// A log-normal interval about `median` with log-spread `spread`, off four draws.
double lognormal(core::Rng& r, double median, double spread) {
  const double s = core::next(r) + core::next(r) + core::next(r) + core::next(r);
  return median * lm::exp(spread * (s - 2) / 0.5773502691896258);
}

struct V3 {
  double x = 0, y = 0, z = 0;
};
V3 add(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 mul(V3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double len(V3 a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }
V3 unit(V3 a) {
  const double l = len(a);
  return l > 0 ? mul(a, 1 / l) : V3{0, -1, 0};
}
// A uniform direction, by rejection from the cube.
V3 any_way(core::Rng& r) {
  for (;;) {
    const V3 v{2 * core::next(r) - 1, 2 * core::next(r) - 1, 2 * core::next(r) - 1};
    const double l = len(v);
    if (l > 0.05 && l <= 1) return mul(v, 1 / l);
  }
}

struct Branch {
  V3 at, dir;
  int steps;
  uint8_t order;
  float glow;
};

void put(Channel& out, V3 a, V3 b, float glow, uint8_t order) {
  if (out.count >= kMaxSegments) return;
  out.seg[out.count++] = {static_cast<float>(a.x),
                          static_cast<float>(a.y),
                          static_cast<float>(a.z),
                          static_cast<float>(b.x),
                          static_cast<float>(b.y),
                          static_cast<float>(b.z),
                          glow,
                          order};
}

}  // namespace

const char* flash_kind_name(FlashKind kind) {
  return kind == FlashKind::ground ? "ground" : "cloud";
}

bool flash_in_slot(const Storm& storm, int k, int64_t n, Flash& out) {
  const Cell& cell = storm.cell[k];
  const auto slot = static_cast<uint32_t>(n);
  const double t = (static_cast<double>(n) + storm_hash(cell.seed, 10, slot)) * kSlot;
  const CellAt c = cell_at(storm, k, t);
  if (c.flashes <= 0) return false;
  if (storm_hash(cell.seed, 11, slot) >= c.flashes / 60 * kSlot) return false;

  core::Rng r = core::make_rng(word_of(storm_hash(cell.seed, 12, slot)));
  Flash f;
  f.cell = k;
  f.t0 = t;
  f.seed = word_of(storm_hash(cell.seed, 13, slot));
  const double drift = std::sqrt(storm.vx * storm.vx + storm.vz * storm.vz);
  const double ahead = drift > 0 ? lm::atan2(storm.vx, storm.vz) : kTau * core::next(r);
  const double span = c.top - storm.base;
  const double g = storm.ground;
  f.kind = core::next(r) < c.in_cloud ? FlashKind::cloud : FlashKind::ground;
  const double at_r = cell.core * 0.6 * std::sqrt(core::next(r)), at_a = kTau * core::next(r);
  f.x = c.x + at_r * lm::sin(at_a);
  f.z = c.z + at_r * lm::cos(at_a);

  if (f.kind == FlashKind::cloud) {
    // Between the charge layers, level and long, spreading most often downwind into the anvil.
    f.y = g + storm.base + (0.55 + 0.2 * core::next(r)) * span;
    const double u = core::next(r);
    const double reach = 3000 + 9000 * u * u;
    const double way = ahead + (core::next(r) - 0.5) * 2.4;
    f.ex = f.x + reach * lm::sin(way);
    f.ez = f.z + reach * lm::cos(way);
    f.ey = f.y + (core::next(r) - 0.5) * 1500;
    const double glow_for = 0.2 + 0.6 * core::next(r);
    f.stroke[0] = {0, 0.25 + 0.2 * core::next(r), 0.01, glow_for, 0.04 + 0.04 * core::next(r)};
    const int pulses = 3 + static_cast<int>(8 * core::next(r));
    double at = 0;
    f.strokes = 1;
    for (int i = 0; i < pulses && f.strokes < kMaxStrokes; ++i) {
      at += glow_for * 0.85 / pulses * (0.5 + core::next(r));
      if (at >= glow_for) break;
      f.stroke[f.strokes++] = {at, 0.1 + 0.25 * core::next(r), 0.006, 0, 0};
    }
    f.length = glow_for + 0.05;
  } else {
    f.blue = core::next(r) < 0.04;
    if (f.blue) {
      // Out of the anvil's top, striking far out ahead of the storm.
      const double far = 8000 + 7000 * core::next(r);
      const double way = ahead + (core::next(r) - 0.5) * 0.8;
      f.y = g + storm.base + 0.85 * span;
      f.x = c.x + 0.5 * far * lm::sin(way);
      f.z = c.z + 0.5 * far * lm::cos(way);
      f.ex = c.x + far * lm::sin(way);
      f.ez = c.z + far * lm::cos(way);
    } else {
      // Out of the lower charge region, striking under the core.
      f.y = g + storm.base + (0.25 + 0.15 * core::next(r)) * span;
      const double gr = cell.core * 1.2 * std::sqrt(core::next(r)), ga = kTau * core::next(r);
      f.ex = c.x + gr * lm::sin(ga);
      f.ez = c.z + gr * lm::cos(ga);
    }
    f.ey = g;
    f.leader = 0.015 + 0.025 * core::next(r);
    const double first = 0.6 + 0.8 * core::next(r);
    f.stroke[0] = {f.leader, first, 0.002, 0, 0};
    f.strokes = 1;
    double at = f.leader;
    while (f.strokes < kMaxStrokes && core::next(r) < 0.75) {
      double gap = lognormal(r, 0.06, 0.7);
      gap = gap < 0.008 ? 0.008 : (gap > 0.3 ? 0.3 : gap);
      if (at + gap > kLastStroke) break;
      at += gap;
      f.stroke[f.strokes++] = {at, first * (0.25 + 0.5 * core::next(r)), 0.002, 0, 0};
    }
    if (core::next(r) < 0.4) {
      // A continuing current, most often after a subsequent stroke.
      const int i = f.strokes == 1 ? 0 : 1 + static_cast<int>(core::next(r) * (f.strokes - 1));
      const double u = core::next(r);
      f.stroke[i].hold = 0.04 + 0.26 * u * u;
      f.stroke[i].glow = 0.12 * f.stroke[i].energy;
    }
    double end = 0;
    for (int i = 0; i < f.strokes; ++i) {
      const double e = f.stroke[i].t + (f.stroke[i].hold > 0.02 ? f.stroke[i].hold : 0.02);
      if (e > end) end = e;
    }
    f.length = end;
  }
  out = f;
  return true;
}

int flashes_between(const Storm& storm, double t0, double t1, Flash* out, int cap) {
  int found = 0;
  for (int k = 0; k < storm.cells; ++k) {
    const Cell& cell = storm.cell[k];
    double from = t0 - kMaxFlash, to = t1;
    const double end = cell.born + life_of(cell.peak);
    if (from < cell.born) from = cell.born;
    if (to > end) to = end;
    if (to <= from) continue;
    const auto n0 = static_cast<int64_t>(std::floor(from / kSlot));
    const auto n1 = static_cast<int64_t>(std::floor(to / kSlot));
    for (int64_t n = n0; n <= n1; ++n) {
      Flash f;
      if (!flash_in_slot(storm, k, n, f)) continue;
      if (f.t0 >= t1 || f.t0 + f.length <= t0) continue;
      if (found < cap) {
        // Kept in order of start; a later cell's flash at the same moment goes after.
        int i = found;
        while (i > 0 && out[i - 1].t0 > f.t0) {
          out[i] = out[i - 1];
          --i;
        }
        out[i] = f;
      }
      ++found;
    }
  }
  return found;
}

double light_over(const Flash& f, double a, double b) {
  double sum = 0;
  if (f.kind == FlashKind::ground) {
    const double l0 = a > f.t0 ? a : f.t0, l1 = b < f.t0 + f.leader ? b : f.t0 + f.leader;
    if (l1 > l0) sum += kLeaderGlow * (l1 - l0);
  }
  for (int i = 0; i < f.strokes; ++i) {
    const Stroke& s = f.stroke[i];
    const double ts = f.t0 + s.t;
    if (b > ts && a - ts < 40 * s.tau) {
      const double u0 = a > ts ? a - ts : 0, u1 = b - ts;
      sum += s.energy / 60 * (lm::exp(-u0 / s.tau) - lm::exp(-u1 / s.tau));
    }
    if (s.hold > 0) {
      const double h0 = a > ts ? a : ts, h1 = b < ts + s.hold ? b : ts + s.hold;
      if (h1 > h0) sum += s.glow * (h1 - h0);
    }
  }
  return sum / (b - a);
}

double reach_of(double d) {
  const double q = d / kHalfReach;
  return 1 / (1 + q * q);
}

FlashLight light_of(const Flash& f, double a, double b) {
  FlashLight l;
  l.x = 0.5 * (f.x + f.ex);
  l.y = 0.5 * (f.y + f.ey);
  l.z = 0.5 * (f.z + f.ez);
  const double dx = f.ex - f.x, dy = f.ey - f.y, dz = f.ez - f.z;
  l.radius = 0.5 * std::sqrt(dx * dx + dy * dy + dz * dz) + 500;
  l.power = light_over(f, a, b);
  l.cloud = f.kind == FlashKind::cloud;
  return l;
}

double sky_light(const Flash* flashes, int n, double x, double y, double z, double a, double b) {
  double sum = 0;
  for (int i = 0; i < n; ++i) {
    const FlashLight l = light_of(flashes[i], a, b);
    if (l.power <= 0) continue;
    const double dx = l.x - x, dy = l.y - y, dz = l.z - z;
    sum += l.power * reach_of(std::sqrt(dx * dx + dy * dy + dz * dz));
  }
  return sum;
}

int brightest(const Flash* flashes, int n, double x, double y, double z, double a, double b,
              FlashLight* out, int cap) {
  int kept = 0;
  double seen[16] = {};
  if (cap > 16) cap = 16;
  if (cap <= 0) return 0;
  for (int i = 0; i < n; ++i) {
    const FlashLight l = light_of(flashes[i], a, b);
    if (l.power <= 0) continue;
    const double dx = l.x - x, dy = l.y - y, dz = l.z - z;
    const double s = l.power * reach_of(std::sqrt(dx * dx + dy * dy + dz * dz));
    int j = kept < cap ? kept : cap - 1;
    if (kept == cap && s <= seen[j]) continue;
    while (j > 0 && seen[j - 1] < s) {
      out[j] = out[j - 1];
      seen[j] = seen[j - 1];
      --j;
    }
    out[j] = l;
    seen[j] = s;
    if (kept < cap) ++kept;
  }
  return kept;
}

void build_channel(const Flash& f, Channel& out) {
  out.count = 0;
  core::Rng r = core::make_rng(f.seed);
  const bool cloud = f.kind == FlashKind::cloud;
  const double step = cloud ? kCloudStep : kStep;
  const double ground = cloud ? -1e30 : f.ey;
  Branch pending[64];
  int waiting = 0;

  // The main channel: a stepped walk drawn toward its end.
  const V3 end{f.ex, f.ey, f.ez};
  V3 p{f.x, f.y, f.z};
  V3 dir = unit(add(end, mul(p, -1)));
  for (int i = 0; i < 600 && out.count < kMaxSegments; ++i) {
    const V3 to = add(end, mul(p, -1));
    const double left = len(to);
    if (left <= step) {
      put(out, p, end, 1, 0);
      break;
    }
    V3 kick = any_way(r);
    if (cloud) kick.y *= 0.3;
    const double pull = left < 4 * step ? 1.5 : 0.35;
    dir = unit(add(add(mul(dir, 0.55), mul(unit(to), pull)), mul(kick, 0.42)));
    V3 q = add(p, mul(dir, step));
    if (q.y <= ground) q.y = ground;
    put(out, p, q, 1, 0);
    if (q.y <= ground) break;
    if (waiting < 64 && core::next(r) < (cloud ? 0.1 : 0.06)) {
      V3 bend = any_way(r);
      if (!cloud) bend.y -= 0.3;
      if (cloud) bend.y *= 0.3;
      pending[waiting++] = {q, unit(add(dir, mul(bend, 0.9))),
                            8 + static_cast<int>(40 * core::next(r)), 1, 0.45f};
    }
    p = q;
  }

  // The branches, and theirs: no end to reach, a pull downward on a ground flash.
  while (waiting > 0 && out.count < kMaxSegments) {
    const Branch b = pending[--waiting];
    V3 bp = b.at, bd = b.dir;
    for (int i = 0; i < b.steps && out.count < kMaxSegments; ++i) {
      V3 kick = any_way(r);
      if (cloud) kick.y *= 0.3;
      bd = unit(add(add(mul(bd, 0.75), mul(kick, 0.45)), V3{0, cloud ? 0.0 : -0.25, 0}));
      V3 q = add(bp, mul(bd, step * 0.8));
      if (q.y <= ground) q.y = ground;
      const float fade = 1.0f - 0.8f * static_cast<float>(i) / static_cast<float>(b.steps);
      put(out, bp, q, b.glow * fade, b.order);
      if (q.y <= ground) break;
      if (b.order < 2 && waiting < 64 && core::next(r) < 0.03) {
        pending[waiting++] = {q, unit(add(bd, mul(any_way(r), 0.9))),
                              4 + static_cast<int>(15 * core::next(r)), 2, 0.2f};
      }
      bp = q;
    }
  }
}

}  // namespace ge::wind
