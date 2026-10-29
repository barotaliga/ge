// Wakes and rings: the impulse ring's far field, a hull's trail, the sum at a point.
//
// ge:summary Wakes: the ring's far field, the hull's trail, the sum at a point
// ge:keywords water wake ring kelvin cauchy-poisson trail emit_ring wake_at
// ge:skill water
// ge:test tests/water_wake_test.cpp
#include "water/wake.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "water/spectrum.hpp"

namespace ge::water {

namespace {

namespace lm = core::libm;
constexpr double kPi = 3.14159265358979323846;
constexpr double kRoot2 = 1.41421356237309504880;

// The share a ring keeps at `tau` s old: whole, then faded over the last third of its life.
double kept(const Wakes& wakes, double tau) {
  return 1 - core::smoothstep(wakes.life * (2.0 / 3.0), wakes.life, tau);
}

// The crater's stages: it opens and closes over `open` s, then its jet rises and falls.
struct Stages {
  double open = 0, jet = 0, height = 0;
};
Stages stages(const Ring& ring) {
  Stages st;
  st.open = 2 * std::sqrt(ring.crater / kGravity);
  st.height = 0.6 * ring.depth;
  st.jet = 2 * std::sqrt(2 * st.height / kGravity);
  return st;
}

// Where a point (ex, ez) m from a ring's point lies against its lean: along it from the
// crater's centre and across it, both in m (along the plain radius, and 0 across, without one).
struct Frame {
  double lean = 0, ux = 0, uz = 0, along = 0, across = 0;
};
Frame frame_of(const Ring& ring, double ex, double ez) {
  Frame f;
  f.lean = core::hypot(ring.lean_x, ring.lean_z);
  if (f.lean <= 0) {
    f.along = std::sqrt(ex * ex + ez * ez);
    return f;
  }
  f.ux = ring.lean_x / f.lean;
  f.uz = ring.lean_z / f.lean;
  f.along = ex * f.ux + ez * f.uz - 0.5 * ring.reach;
  f.across = ez * f.ux - ex * f.uz;
  return f;
}

// The crater and the jet at (ex, ez), `tau`: the height and d height / dt.
struct Near {
  double h = 0, dt = 0;
};
Near near_field(const Ring& ring, double ex, double ez, double tau) {
  Near out;
  if (ring.crater <= 0 || tau <= 0) return out;
  const Stages st = stages(ring);
  const Frame fr = frame_of(ring, ex, ez);
  if (tau < st.open) {
    // The cavity: an ellipse drawn out `reach` along the lean, its rim piled up ahead.
    const double qa = fr.along / (ring.crater + 0.5 * ring.reach), qc = fr.across / ring.crater;
    const double rho2 = qa * qa + qc * qc, g = lm::exp(-rho2);
    const double ahead = fr.lean > 0 ? 1 + fr.lean * qa / std::sqrt(rho2 + 0.0625) : 1;
    const double f = (1 - rho2) * g * ahead;
    const double w = kPi / st.open;
    out.h = -ring.depth * lm::sin(w * tau) * f;
    out.dt = -ring.depth * w * lm::cos(w * tau) * f;
  } else if (tau < st.open + st.jet) {
    const double wide = ring.crater / 3;
    const double q2 = (fr.along * fr.along + fr.across * fr.across) / (wide * wide);
    const double g = lm::exp(-q2);
    const double w = kPi / st.jet;
    out.h = st.height * lm::sin(w * (tau - st.open)) * g;
    out.dt = st.height * w * lm::cos(w * (tau - st.open)) * g;
  }
  return out;
}

// The crater's slope at (ex, ez), by central differences a thousandth of the crater apart.
void near_slope(const Ring& ring, double ex, double ez, double tau, double& dx, double& dz) {
  const double s = 1e-3 * ring.crater;
  dx = (near_field(ring, ex + s, ez, tau).h - near_field(ring, ex - s, ez, tau).h) / (2 * s);
  dz = (near_field(ring, ex, ez + s, tau).h - near_field(ring, ex, ez - s, tau).h) / (2 * s);
}

}  // namespace

double crater_height(const Ring& ring, double dx, double dz, double tau) {
  return near_field(ring, dx, dz, tau).h;
}

double crater_time(const Ring& ring) {
  if (ring.crater <= 0) return 0;
  const Stages st = stages(ring);
  return st.open + st.jet;
}

void emit_ring(Wakes& wakes, const Ring& ring) {
  std::vector<Ring>& r = wakes.rings;
  size_t old = 0;
  while (old < r.size() && ring.t0 - r[old].t0 >= wakes.life) ++old;
  if (r.size() - old + 1 > static_cast<size_t>(kMaxRings)) old = r.size() + 1 - kMaxRings;
  if (old > 0) r.erase(r.begin(), r.begin() + static_cast<std::ptrdiff_t>(old));
  r.push_back(ring);
}

void wake_trail(Wakes& wakes, Trail& trail, int body, double x, double z, double vx, double vz,
                double length, double beam, double draught, double t, double interval,
                double strength) {
  const double speed = core::hypot(vx, vz);
  const double transverse = 2 * kPi * speed * speed / kGravity;
  const double shortest = transverse / 8 > wakes.shortest ? transverse / 8 : wakes.shortest;
  const bool first = trail.last < -1e299;
  if (!first && t - trail.last < interval && core::hypot(x - trail.x, z - trail.z) < shortest / 3) {
    return;
  }
  const double gap = first ? 0 : t - trail.last;
  trail.last = t;
  trail.x = x;
  trail.z = z;
  if (speed <= 0 || first) return;
  Ring ring;
  ring.x = x;
  ring.z = z;
  ring.t0 = t;
  ring.volume = strength * beam * draught * speed * gap;
  ring.size = 0.1 * length;
  ring.shortest = shortest;
  ring.body = body;
  emit_ring(wakes, ring);
}

namespace {

// A ring's amplitude at `rs` (its softened distance) where its phase is `phase` and its local
// wavenumber `k`: the far field, its source's spectrum, the waves shorter than kept damped, its
// front faded in, its age faded out.
double amplitude(const Wakes& wakes, const Ring& ring, double rs, double phase, double k,
                 double tau) {
  const double floor = wakes.shortest > ring.shortest ? wakes.shortest : ring.shortest;
  const double keep = core::smoothstep(floor, 2 * floor, 2 * kPi / k) *
                      core::smoothstep(0.25 * kPi, 1.25 * kPi, phase);
  if (keep <= 0) return 0;
  const double ks = k * ring.size;
  return ring.volume * k / (kRoot2 * kPi * rs) * lm::exp(-0.25 * ks * ks) * keep * kept(wakes, tau);
}

}  // namespace

namespace {

// One ring's far field at (ex, ez) from its point, `tau` s on: the height, its slope and the
// water's velocity (its crater aside).
WakeAt far_field(const Wakes& wakes, const Ring& ring, double ex, double ez, double tau) {
  WakeAt out;
  const Frame fr = frame_of(ring, ex, ez);
  if (fr.lean > 0) {
    // The waves are set off from the drawn-out crater's centre.
    ex -= 0.5 * ring.reach * fr.ux;
    ez -= 0.5 * ring.reach * fr.uz;
  }
  const double rs = std::sqrt(ex * ex + ez * ez + ring.size * ring.size);
  const double phase = kGravity * tau * tau / (4 * rs);
  const double k = phase / rs;
  const double a = amplitude(wakes, ring, rs, phase, k, tau);
  if (a == 0) return out;
  const double c = lm::cos(phase), s = lm::sin(phase);
  // The phase falls outward (d phase / dr = -k), so the slope is a k sin(phase) outward; the
  // water moves outward under a crest at the wave's frequency, omega = sqrt(g k).
  const double omega = kGravity * tau / (2 * rs);
  const double ox = ex / rs, oz = ez / rs;
  out.height = a * c;
  out.dx = a * k * s * ox;
  out.dz = a * k * s * oz;
  out.u = omega * a * c * ox;
  out.w = omega * a * c * oz;
  out.v = -omega * a * s;
  if (fr.lean > 0) {
    // Tipped forward by 1 + lean . e / rs, its own slope carried by the height's.
    const double dot = ex * ring.lean_x + ez * ring.lean_z;
    const double m = 1 + dot / rs;
    const double mx = ring.lean_x / rs - dot * ex / (rs * rs * rs);
    const double mz = ring.lean_z / rs - dot * ez / (rs * rs * rs);
    out.dx = out.dx * m + out.height * mx;
    out.dz = out.dz * m + out.height * mz;
    out.height *= m;
    out.u *= m;
    out.w *= m;
    out.v *= m;
  }
  return out;
}

}  // namespace

double ring_height(const Wakes& wakes, const Ring& ring, double dx, double dz, double tau) {
  if (tau <= 0) return 0;
  const double far = far_field(wakes, ring, dx, dz, tau).height;
  return ring.crater > 0 ? far + near_field(ring, dx, dz, tau).h : far;
}

WakeAt wake_at(const Wakes& wakes, int body, double x, double z, double t) {
  WakeAt out;
  for (const Ring& ring : wakes.rings) {
    if (ring.body != body) continue;
    const double tau = t - ring.t0;
    if (tau <= 0) continue;
    const double ex = x - ring.x, ez = z - ring.z;
    const WakeAt far = far_field(wakes, ring, ex, ez, tau);
    if (ring.crater > 0) {
      const Near n = near_field(ring, ex, ez, tau);
      double sx = 0, sz = 0;
      near_slope(ring, ex, ez, tau, sx, sz);
      out.height += far.height + n.h;
      out.dx += far.dx + sx;
      out.dz += far.dz + sz;
      out.v += far.v + n.dt;
    } else {
      out.height += far.height;
      out.dx += far.dx;
      out.dz += far.dz;
      out.v += far.v;
    }
    out.u += far.u;
    out.w += far.w;
  }
  return out;
}

}  // namespace ge::water
