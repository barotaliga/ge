// Wading through lava and slime (liquid_wade.hpp): the waves a viscosity makes, the tracks stamped
// onto the bodies, and the surface's CPU twin (the vendored libm's exp and sin, so a lab's numbers
// and pictures of it are the same bits everywhere).
//
// ge:summary Wading: the viscosity's waves, the tracks stamped each frame, the CPU twin
// ge:keywords render liquid wade wading stamps tracks furrow rings bow wade_liquids wade_height
// ge:skill render water
// ge:test tests/render_liquid_wade_test.cpp
#include "render/liquid_wade.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"
#include "render/liquid.hpp"

namespace ge::render {

namespace {

namespace libm = core::libm;

// Water's (a viscosity of 1): what every rule scales.
constexpr double kSpeed = 0.7;       // m/s: a ripple 0.3 m long on water
constexpr double kWavelength = 0.3;  // m
constexpr double kRing = 1.6;        // s
constexpr double kFurrow = 0.45;     // s
constexpr double kHeal = 1.5;        // s
constexpr double kDepth = 0.06;      // m
constexpr double kAmp = 0.016;       // m
constexpr int kFadeOldest = 8;       // the oldest stamps of a full body faded in by rank
constexpr double kTwoPi = 6.283185307179586;

void put(float* out, double a, double b, double c, double d) {
  out[0] = static_cast<float>(a);
  out[1] = static_cast<float>(b);
  out[2] = static_cast<float>(c);
  out[3] = static_cast<float>(d);
}

// The nearest point of the segment p0 -> p1 (times t0 -> t1) to (x, z): its distance, its time.
void nearest(double x, double z, double x0, double z0, double t0, double x1, double z1, double t1,
             double& d, double& t) {
  const double ex = x1 - x0, ez = z1 - z0;
  const double len2 = ex * ex + ez * ez;
  double u = len2 > 1e-8 ? ((x - x0) * ex + (z - z0) * ez) / len2 : 0;
  u = std::clamp(u, 0.0, 1.0);
  const double px = x0 + u * ex - x, pz = z0 + u * ez - z;
  d = std::sqrt(px * px + pz * pz);
  t = t0 + u * (t1 - t0);
}

// Whether the feet are in body `slot`: under its level, over its floor, in its box.
bool in_body(const Liquids::Slot& slot, const double feet[3]) {
  const LiquidGpuBody& b = slot.staged;
  return !slot.hidden && feet[0] > b.box[0] && feet[0] < b.box[2] && feet[2] > b.box[1] &&
         feet[2] < b.box[3] && feet[1] < b.info[0] && feet[1] > slot.floor - 0.5;
}

// How hard a body `height_m` tall with its feet `depth_m` under the level stirs the surface: in to
// its waist and deeper fully; a swimmer under it half, fading out a body's height further down.
double strength_of(double depth_m, double height_m) {
  const double share = depth_m / std::max(height_m, 0.1);
  if (share < 1) return std::min(1.0, 2 * share);
  return 0.5 * std::clamp(2 - share, 0.0, 1.0);
}

}  // namespace

WadeWaves wade_waves(const WadeLook& look, bool crust) {
  const double v = std::max(look.viscosity, 0.1);
  const double root = std::sqrt(v), quarter = std::sqrt(root);
  WadeWaves w;
  w.speed_mps = kSpeed / root;
  w.wavelength_m = kWavelength * root;
  w.ring_s = kRing / quarter;
  w.furrow_s = kFurrow * v;
  w.heal_s = crust ? kHeal * v : 0;
  w.depth_m = kDepth * quarter;
  w.amp_m = kAmp / quarter;
  w.life_s = std::max({4 * w.ring_s, 4 * w.furrow_s, 3 * w.heal_s});
  return w;
}

void wade_liquids(Liquids& liquids, LiquidWading& wading, std::span<const LiquidWader> waders,
                  double t_s, bool on) {
  const bool jump = wading.last_t >= 0 && (t_s < wading.last_t || t_s - wading.last_t > 1);
  wading.last_t = t_s;
  wading.stamps.resize(liquids.bodies.size());
  if (!on || jump) {
    wading.tracks.clear();
    for (auto& s : wading.stamps) s.clear();
  }
  // A track's newest stamp drawn on no more.
  auto drop_head = [&](const LiquidWading::Track& tr) {
    if (tr.body < 0 || tr.body >= static_cast<int>(wading.stamps.size())) return;
    for (WadeStamp& s : wading.stamps[static_cast<size_t>(tr.body)])
      if (s.track == tr.id) s.head = false;
  };
  for (auto& tr : wading.tracks) tr.seen = false;
  if (on) {
    for (const LiquidWader& w : waders) {
      int body = -1;
      for (size_t i = 0; i < liquids.bodies.size() && body < 0; ++i)
        if (in_body(liquids.bodies[i], w.feet)) body = static_cast<int>(i);
      auto tr = std::find_if(wading.tracks.begin(), wading.tracks.end(),
                             [&](const LiquidWading::Track& k) { return k.id == w.id; });
      if (tr == wading.tracks.end()) {
        wading.tracks.push_back({w.id, -1, w.feet[0], w.feet[2], t_s, false});
        tr = wading.tracks.end() - 1;
      }
      tr->seen = true;
      if (body < 0) {
        drop_head(*tr);
        tr->body = -1;
        continue;
      }
      const Liquids::Slot& slot = liquids.bodies[static_cast<size_t>(body)];
      const double dx = w.feet[0] - tr->x, dz = w.feet[2] - tr->z;
      const bool fresh = tr->body != body;
      const double span = fresh ? 0 : t_s - tr->t;
      if (!fresh && dx * dx + dz * dz < slot.wade.spacing_m * slot.wade.spacing_m && span < kIdleS)
        continue;
      drop_head(*tr);
      WadeStamp s;
      s.x = w.feet[0];
      s.z = w.feet[2];
      s.t = t_s;
      const double moved = std::sqrt(dx * dx + dz * dz);
      s.strength = strength_of(slot.staged.info[0] - w.feet[1], w.height_m);
      // Stamped by the clock, not by a stride: the less it moved, the smaller (a body standing
      // still sways, a swell from it now and then).
      if (!fresh && moved < slot.wade.spacing_m)
        s.strength *= 0.4 + 0.6 * moved / slot.wade.spacing_m;
      if (span > 0 && !fresh) {
        s.vx = dx / span;
        s.vz = dz / span;
        s.span_s = span;
      }
      s.head = true;
      s.track = w.id;
      std::vector<WadeStamp>& list = wading.stamps[static_cast<size_t>(body)];
      if (list.size() >= static_cast<size_t>(kLiquidWades)) list.erase(list.begin());
      list.push_back(s);
      tr->body = body;
      tr->x = w.feet[0];
      tr->z = w.feet[2];
      tr->t = t_s;
    }
  }
  for (const auto& tr : wading.tracks)
    if (!tr.seen) drop_head(tr);
  std::erase_if(wading.tracks, [](const LiquidWading::Track& k) { return !k.seen; });
  // Every body's stamps to its uniform, the ones past their life dropped.
  for (size_t i = 0; i < liquids.bodies.size(); ++i) {
    LiquidGpuBody& b = liquids.bodies[i].staged;
    std::vector<WadeStamp>& list = wading.stamps[i];
    const double life = b.wade2[1];
    std::erase_if(list, [&](const WadeStamp& s) { return t_s - s.t > life; });
    const size_t n = list.size();
    for (size_t k = 0; k < n; ++k) {
      const WadeStamp& s = list[k];
      // A full body's oldest fade in by rank, so the track's tail goes before it is dropped.
      const double fade = n >= static_cast<size_t>(kLiquidWades)
                              ? std::min(1.0, double(k + 1) / double(kFadeOldest))
                              : 1.0;
      put(b.wades[k], s.x, s.z, s.t, s.strength * fade);
      put(b.wade_v[k], s.vx, s.vz, s.span_s, s.head ? 1 : 0);
    }
    b.wade0[0] = static_cast<float>(n);
  }
}

WadeAt wade_height(std::span<const WadeStamp> stamps, const WadeLook& look, bool crust, double x,
                   double z, double t_s, bool rings) {
  const WadeWaves w = wade_waves(look, crust);
  const double leg = look.leg_m;
  double trough = 0, lip = 0, sum = 0, bow = 0;
  WadeAt out;
  for (const WadeStamp& s : stamps) {
    const double age = t_s - s.t;
    if (age < 0 || age > w.life_s || s.strength <= 0) continue;
    const double speed = std::sqrt(s.vx * s.vx + s.vz * s.vz);
    const double r = std::sqrt((x - s.x) * (x - s.x) + (z - s.z) * (z - s.z));
    const double reach =
        w.speed_mps * age + 2 * w.wavelength_m + 3 * leg + speed * (s.span_s + kHeadMostS);
    if (r > reach) continue;
    // The furrow: the nearer of the track behind the stamp and, for the newest, ahead of it.
    double d = 0, tt = 0;
    nearest(x, z, s.x - s.vx * s.span_s, s.z - s.vz * s.span_s, s.t - s.span_s, s.x, s.z, s.t, d,
            tt);
    const double ahead = std::min(age, kHeadMostS);
    const double hx = s.x + s.vx * ahead, hz = s.z + s.vz * ahead;
    if (s.head) {
      double d2 = 0, t2 = 0;
      nearest(x, z, s.x, s.z, s.t, hx, hz, s.t + ahead, d2, t2);
      if (d2 < d) d = d2, tt = t2;
    }
    const double at = std::max(t_s - tt, 0.0);
    const double rho2 = (d / leg) * (d / leg);
    const double f =
        -w.depth_m * s.strength * (1 - rho2) * libm::exp(-rho2) * libm::exp(-at / w.furrow_s);
    if (f < 0)
      trough = std::min(trough, f);
    else
      lip = std::max(lip, f);
    out.furrow = std::max(out.furrow, s.strength * libm::exp(-rho2) * libm::exp(-at / w.furrow_s));
    if (crust)
      out.open = std::max(out.open, s.strength * libm::exp(-2 * rho2) * libm::exp(-at / w.heal_s));
    // The rings out of the stamp.
    if (rings) {
      const double u = r - w.speed_mps * age;
      const double env = libm::exp(-u * u / (w.wavelength_m * w.wavelength_m));
      sum += w.amp_m * s.strength * libm::exp(-age / w.ring_s) * env *
             libm::sin(kTwoPi * u / w.wavelength_m) / std::sqrt(1 + r / leg);
    }
    // The bow piled up ahead of a body moving on.
    if (s.head && speed > 0.2) {
      const double bx = hx + s.vx / speed * leg * 1.2, bz = hz + s.vz / speed * leg * 1.2;
      const double q2 = ((x - bx) * (x - bx) + (z - bz) * (z - bz)) / (leg * leg);
      bow += 0.6 * w.depth_m * s.strength * std::min(speed / 2, 1.0) * libm::exp(-q2) *
             libm::exp(-std::max(age - kHeadMostS, 0.0) / 0.15);
    }
  }
  out.height_m = trough + lip + sum + bow;
  return out;
}

}  // namespace ge::render
