// Snow over time (time.hpp): the schedules' integrals, the spell, the weathered column, the
// weathered sample and the groomer.
//
// ge:summary Snow over time's implementation: schedule integrals, weathering, sample, groom
// ge:keywords snow time schedule integral spell weathered filled drift crust groom sample
// ge:skill snow
// ge:test tests/snow_time_test.cpp
#include "snow/time.hpp"

#include <cmath>
#include <limits>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "snow/press.hpp"

namespace ge::snow {

namespace {

constexpr double kNever = -1e300;
constexpr double kAirDefault = -5;  // C, where a weather has no air schedule
constexpr double kKarman = 0.4;
constexpr double kAirDensity = 1.2;  // kg/m3
constexpr double kG = 9.81;          // m/s2
constexpr int kDriftSteps = 8;       // midpoints a wind piece is read at for the drift's integral
constexpr double kDay = 86400;       // s

// The value on schedule segment i (t[i] .. t[i + 1]) at x.
double on_segment(const Schedule& s, size_t i, double x) {
  const double w = s.t[i + 1] - s.t[i];
  if (w <= 0) return s.v[i + 1];
  return s.v[i] + (s.v[i + 1] - s.v[i]) * (x - s.t[i]) / w;
}

// Every straight piece of `s` over [a, b]: f(x0, x1, y0, y1).
template <class F>
void each_piece(const Schedule& s, double a, double b, F&& f) {
  if (b <= a) return;
  const size_t n = s.t.size() < s.v.size() ? s.t.size() : s.v.size();
  if (n == 0) {
    f(a, b, 0.0, 0.0);
    return;
  }
  if (a < s.t[0]) {
    const double e = b < s.t[0] ? b : s.t[0];
    f(a, e, s.v[0], s.v[0]);
    a = e;
    if (a >= b) return;
  }
  for (size_t i = 0; i + 1 < n; i++) {
    const double s0 = s.t[i], s1 = s.t[i + 1];
    if (s1 <= a) continue;
    if (s0 >= b) break;
    const double x0 = a > s0 ? a : s0, x1 = b < s1 ? b : s1;
    if (x1 > x0) f(x0, x1, on_segment(s, i, x0), on_segment(s, i, x1));
  }
  if (b > s.t[n - 1]) {
    const double x0 = a > s.t[n - 1] ? a : s.t[n - 1];
    f(x0, b, s.v[n - 1], s.v[n - 1]);
  }
}

// The integral of max(y, 0) over a straight piece w long from y0 to y1.
double positive_part(double w, double y0, double y1) {
  if (y0 >= 0 && y1 >= 0) return w * (y0 + y1) / 2;
  if (y0 <= 0 && y1 <= 0) return 0;
  return y0 > 0 ? w * y0 * y0 / (2 * (y0 - y1)) : w * y1 * y1 / (2 * (y1 - y0));
}

double air_at(const Weather& w, double t) {
  return w.air.t.empty() ? kAirDefault : value_at(w.air, t);
}

// The saltation flux, kg/m/s, a 10 m wind `u10` carries over loose dry snow.
double drift_flux(const Weather& w, double u10) {
  if (u10 <= 0) return 0;
  const double ustar = kKarman * u10 / core::libm::log(10 / w.roughness);
  const double ut = w.drift_threshold;
  if (ustar <= ut) return 0;
  return 0.68 * kAirDensity * ut * (ustar * ustar - ut * ut) / (kG * ustar);
}

// The density `rho` kg/m3 has reached after `dt` s settling toward `target` with time constant
// `tau`; denser snow is left as it is.
double settle(double rho, double target, double dt, double tau) {
  if (rho >= target) return rho;
  return target - (target - rho) * core::libm::exp(-dt / tau);
}

double tau_of(const Weather& w, const Spell& sp) {
  return w.settle_tau * core::libm::exp(-w.settle_warm * (sp.mean_air + 5));
}

uint16_t quantize(double v, double unit, uint16_t most) {
  const double q = core::num::round(v / unit);
  return q <= 0 ? 0 : q >= most ? most : static_cast<uint16_t>(q);
}

Snowpack pack_of(const Cell& c) {
  return {cell_depth(c), cell_mass(c), c.kind, (c.flags & kPressed) != 0};
}

}  // namespace

double value_at(const Schedule& s, double t) {
  const size_t n = s.t.size() < s.v.size() ? s.t.size() : s.v.size();
  if (n == 0) return 0;
  if (t <= s.t[0]) return s.v[0];
  if (t >= s.t[n - 1]) return s.v[n - 1];
  size_t i = 0;
  while (i + 2 < n && s.t[i + 1] < t) i++;
  return on_segment(s, i, t);
}

double integral(const Schedule& s, double t0, double t1) {
  double sum = 0;
  each_piece(s, t0, t1,
             [&](double x0, double x1, double y0, double y1) { sum += (x1 - x0) * (y0 + y1) / 2; });
  return sum;
}

double integral_above(const Schedule& s, double level, double t0, double t1) {
  double sum = 0;
  each_piece(s, t0, t1, [&](double x0, double x1, double y0, double y1) {
    sum += positive_part(x1 - x0, y0 - level, y1 - level);
  });
  return sum;
}

double integral_below(const Schedule& s, double level, double t0, double t1) {
  double sum = 0;
  each_piece(s, t0, t1, [&](double x0, double x1, double y0, double y1) {
    sum += positive_part(x1 - x0, level - y0, level - y1);
  });
  return sum;
}

double last_above(const Schedule& s, double level, double t) {
  const size_t n = s.t.size() < s.v.size() ? s.t.size() : s.v.size();
  if (value_at(s, t) > level) return t;
  if (n == 0 || t <= s.t[0]) return kNever;  // flat at v[0] <= level all the way back
  // Back through the segments that end at or before t: the last moment above is a crossing.
  size_t i = n - 1;
  while (i > 0 && s.t[i - 1] >= t) i--;
  for (; i > 0; i--) {
    const size_t k = i - 1;  // segment t[k] .. t[i]
    const double x1 = t < s.t[i] ? t : s.t[i];
    const double y0 = s.v[k], y1 = on_segment(s, k, x1);
    if (y1 > level) return x1;
    if (y0 > level) return s.t[k] + (y0 - level) / (y0 - y1) * (x1 - s.t[k]);
  }
  return kNever;
}

Spell spell(const Weather& w, double t0, double t1) {
  Spell sp;
  sp.t0 = t0;
  sp.t1 = t1;
  sp.air_end = air_at(w, t1);
  if (t1 <= t0) {
    sp.mean_air = sp.air_end;
    return sp;
  }
  sp.fallen = integral(w.fall, t0, t1);
  if (sp.fallen < 0) sp.fallen = 0;
  if (w.air.t.empty()) {
    sp.mean_air = kAirDefault;
  } else {
    sp.thaw = integral_above(w.air, 0, t0, t1);
    sp.mean_air = integral(w.air, t0, t1) / (t1 - t0);
  }
  if (!w.wind.t.empty()) {
    each_piece(w.wind, t0, t1, [&](double x0, double x1, double y0, double y1) {
      const double step = (x1 - x0) / kDriftSteps;
      for (int k = 0; k < kDriftSteps; k++) {
        const double f = (k + 0.5) / kDriftSteps;
        if (air_at(w, x0 + f * (x1 - x0)) >= 0) continue;  // wet snow does not drift
        sp.drifted += drift_flux(w, y0 + f * (y1 - y0)) * step;
      }
    });
  }
  return sp;
}

Snowpack weathered(const Weather& w, const Spell& sp, Snowpack pack) {
  const double dt = sp.t1 - sp.t0;
  if (dt <= 0) return pack;
  const double tau = tau_of(w, sp);
  const bool melting = sp.thaw > 0;
  const double target = melting || pack.kind == Kind::spring ? w.wet_density : w.settled_density;
  // The old snow settles; mass kept, so it sinks.
  if (pack.depth > 0 && pack.mass > 0) {
    const double rho = settle(pack.mass / pack.depth, target, dt, tau);
    pack.depth = pack.mass / rho;
  }
  // The new snow, settled over half the spell (it fell through it).
  double fresh_depth = 0;
  if (sp.fallen > 0) {
    const double rho = settle(w.fresh_density, target, dt / 2, tau);
    fresh_depth = sp.fallen / rho;
    pack.depth += fresh_depth;
    pack.mass += sp.fallen;
  }
  // The melt, at the column's density.
  if (melting && pack.mass > 0) {
    double melt = w.melt_factor * sp.thaw / kDay;
    if (melt > pack.mass) melt = pack.mass;
    pack.depth *= (pack.mass - melt) / pack.mass;
    pack.mass -= melt;
  }
  // The kind.
  const bool wet = melting && sp.air_end > 0;
  if (wet) {
    pack.kind = Kind::spring;
  } else if (melting || pack.kind == Kind::spring) {
    pack.kind = pack.pressed ? Kind::ice : Kind::crust;
  }
  if (!wet && fresh_depth >= w.powder_over) pack.kind = Kind::powder;
  if (pack.kind == Kind::powder && !pack.pressed && pack.depth > 0 &&
      pack.mass / pack.depth >= kind_row(Kind::settled).loose) {
    pack.kind = Kind::settled;
  }
  return pack;
}

Snowpack filled(const Weather& w, const Spell& sp, Snowpack pack, double drop) {
  if (drop <= 0 || sp.t1 <= sp.t0) return pack;
  const double tau = tau_of(w, sp);
  const double target = sp.thaw > 0 ? w.wet_density : w.settled_density;
  const double fresh_rho = settle(w.fresh_density, target, (sp.t1 - sp.t0) / 2, tau);
  const double by_fall = w.fill * sp.fallen / fresh_rho;  // m
  const double by_drift = sp.drifted / w.drift_density;   // m: what crossed a metre of hollow
  const double want = by_fall + by_drift;
  if (want <= 0) return pack;
  const double share = drop < want ? drop / want : 1.0;
  pack.depth += (by_fall + by_drift) * share;
  pack.mass += (by_fall * fresh_rho + by_drift * w.drift_density) * share;
  return pack;
}

uint16_t minute_of(double t) {
  const double m = std::floor(t / 60);
  return m <= 0 ? 0 : m >= 65535 ? 65535 : static_cast<uint16_t>(m);
}

Moment moment(const Layer& layer, double t) {
  Moment m;
  m.layer = &layer;
  m.t = t;
  if (layer.spec.weather) m.since_start = spell(*layer.spec.weather, 0, t);
  return m;
}

Snowpack cover_now(Moment& m, int c, int r) {
  const Layer& l = *m.layer;
  const CoverSample s = cover_at(l.spec.cover, sample_x(l, c), sample_z(l, r));
  const Snowpack pack{s.depth, s.depth * s.density, s.kind, false};
  return l.spec.weather ? weathered(*l.spec.weather, m.since_start, pack) : pack;
}

Snowpack column_now(Moment& m, int c, int r) {
  const Layer& l = *m.layer;
  const Cell* cell = cell_at(l, c, r);
  if (!cell || !(cell->flags & kTouched)) return cover_now(m, c, r);
  if (!l.spec.weather) return pack_of(*cell);
  const Weather& w = *l.spec.weather;
  if (cell->minute != m.minute) {
    m.minute = cell->minute;
    m.since_minute = spell(w, cell->minute * 60.0, m.t);
  }
  const Snowpack p = weathered(w, m.since_minute, pack_of(*cell));
  return filled(w, m.since_minute, p, cover_now(m, c, r).depth - p.depth);
}

void bring_to(Layer& layer, Moment& m, Cell& cell, int c, int r) {
  if (!layer.spec.weather) return;
  const uint16_t minute = minute_of(m.t);
  const Snowpack p = column_now(m, c, r);
  const uint16_t mass = quantize(p.mass, kMassUnit, kMaxMass);
  layer.mass_in = layer.mass_in + mass - cell.mass;
  cell.mass = mass;
  cell.depth_mm = quantize(p.depth, 0.001, kMaxDepthMm);
  cell.kind = p.kind;
  cell.minute = minute;
}

double crust_load(const Weather& w, double t) {
  if (w.air.t.empty()) return std::numeric_limits<double>::infinity();
  const double thaw = last_above(w.air, 0, t);
  if (thaw <= kNever) return std::numeric_limits<double>::infinity();
  if (thaw >= t) return 0;                                        // thawing now: wet, no lid
  const double frost = integral_below(w.air, 0, thaw, t) / 3600;  // C h
  const double h = w.crust_per_root * std::sqrt(frost);
  return w.crust_strength * h * h;
}

Column sample(const Layer& layer, double x, double z, double t) {
  if (!layer.spec.weather) return sample(layer, x, z);
  const Weather& w = *layer.spec.weather;
  Moment m = moment(layer, t);
  Column col;
  col.ground = ground_at(layer.spec.ground, x, z);
  const CoverSample cs = cover_at(layer.spec.cover, x, z);
  const Snowpack here = weathered(w, m.since_start, {cs.depth, cs.depth * cs.density, cs.kind});
  const double fx = (x - layer.spec.region.x0) / layer.spec.cell;
  const double fz = (z - layer.spec.region.z0) / layer.spec.cell;
  auto clamp_index = [](int i, int n) { return i < 0 ? 0 : i >= n ? n - 1 : i; };
  const int c0 = clamp_index(static_cast<int>(std::floor(fx)), layer.cols);
  const int r0 = clamp_index(static_cast<int>(std::floor(fz)), layer.rows);
  const int c1 = clamp_index(c0 + 1, layer.cols), r1 = clamp_index(r0 + 1, layer.rows);
  const int cs4[4] = {c0, c1, c0, c1}, rs4[4] = {r0, r0, r1, r1};
  bool any = false;
  for (int k = 0; k < 4; k++) {
    const Cell* cell = cell_at(layer, cs4[k], rs4[k]);
    any = any || (cell && (cell->flags & kTouched));
  }
  if (!any) {
    col.depth = here.depth;
    col.density = here.depth > 0 ? here.mass / here.depth : 0.0;
    col.kind = here.kind;
    col.packed = here.depth > 0 ? packed_from_density(here.kind, col.density) : 1.0;
    col.surface = col.ground + col.depth;
    return col;
  }
  const double tx = core::clamp(fx - c0, 0, 1), tz = core::clamp(fz - r0, 0, 1);
  const double wt[4] = {(1 - tx) * (1 - tz), tx * (1 - tz), (1 - tx) * tz, tx * tz};
  double depth = 0, mass = 0, packed = 0, best = -1;
  for (int k = 0; k < 4; k++) {
    const Snowpack p = column_now(m, cs4[k], rs4[k]);
    depth += wt[k] * p.depth;
    mass += wt[k] * p.mass;
    packed += wt[k] * (p.depth > 0 ? packed_from_density(p.kind, p.mass / p.depth) : 1.0);
    if (wt[k] > best) {
      best = wt[k];
      col.kind = p.kind;
    }
  }
  col.touched = true;
  col.depth = depth;
  col.density = depth > 0 ? mass / depth : 0.0;
  col.packed = packed;
  col.surface = col.ground + depth;
  col.drop = here.depth - depth;
  return col;
}

Groomed groom(Layer& layer, const Shape& swath, double t) {
  Groomed out;
  if (swath.half_width <= 0) return out;
  const uint16_t minute = minute_of(t);
  Moment m = moment(layer, minute * 60.0);
  const double cell = layer.spec.cell;
  // The swath's box in samples.
  double r = swath.half_width;
  if (swath.kind != ShapeKind::capsule && swath.half_length > r) r = swath.half_length;
  const double x0 = (swath.ax < swath.bx ? swath.ax : swath.bx) - r;
  const double x1 = (swath.ax > swath.bx ? swath.ax : swath.bx) + r;
  const double z0 = (swath.az < swath.bz ? swath.az : swath.bz) - r;
  const double z1 = (swath.az > swath.bz ? swath.az : swath.bz) + r;
  const int c0 = static_cast<int>(std::ceil((x0 - layer.spec.region.x0) / cell));
  const int c1 = static_cast<int>(std::floor((x1 - layer.spec.region.x0) / cell));
  const int r0 = static_cast<int>(std::ceil((z0 - layer.spec.region.z0) / cell));
  const int r1 = static_cast<int>(std::floor((z1 - layer.spec.region.z0) / cell));
  std::vector<int> picked;  // c, r pairs (a tile allocated later would move a pointer)
  uint64_t units = 0;
  double volume = 0;  // m of depth summed
  for (int rr = r0 < 0 ? 0 : r0; rr <= r1 && rr < layer.rows; rr++) {
    for (int cc = c0 < 0 ? 0 : c0; cc <= c1 && cc < layer.cols; cc++) {
      if (shape_u(swath, sample_x(layer, cc), sample_z(layer, rr)) > 1) continue;
      Cell* c = cell_for(layer, cc, rr);
      if (!c) {
        out.refused++;
        continue;
      }
      bring_to(layer, m, *c, cc, rr);  // no weather: the cell is already the snow
      units += c->mass;
      volume += cell_depth(*c);
      picked.push_back(cc);
      picked.push_back(rr);
    }
  }
  const auto n = static_cast<uint64_t>(picked.size() / 2);
  if (n == 0) return out;
  const double mean_density = volume > 0 ? static_cast<double>(units) * kMassUnit / volume : 0;
  const double groomed = kind_row(Kind::groomed).packed;
  const double rho = mean_density > groomed ? mean_density : groomed;
  const uint64_t base = units / n, rest = units % n;
  for (uint64_t i = 0; i < n; i++) {
    Cell* c = const_cast<Cell*>(cell_at(layer, picked[2 * i], picked[2 * i + 1]));
    const uint64_t u = base + (i < rest ? 1 : 0);
    c->mass = static_cast<uint16_t>(u);
    c->depth_mm = quantize(static_cast<double>(u) * kMassUnit / rho, 0.001, kMaxDepthMm);
    c->kind = Kind::groomed;
    c->flags = kTouched | kCorduroy;
    c->minute = minute;
  }
  out.samples = static_cast<int>(n);
  out.mass = static_cast<double>(units) * kMassUnit * cell * cell;
  return out;
}

}  // namespace ge::snow
