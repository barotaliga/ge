// The definitions behind ski_jump.hpp: the norm's elements, the profile laid out by arc and by
// height and resampled, the in-run's point mass, and the hill graded over a map.
//
// ge:summary R43 the ski jumping hill's profile, its gates and speeds, and its grading
// ge:keywords ski-jump hill-profile in-run landing-hill edge-speed set-ski-jump
// ge:skill mountain
// ge:test tests/mountain_ski_jump_test.cpp
#include "mountain/mapgen/ski_jump.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"

namespace ge::mountain::mapgen {

namespace {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace V = venue_rules::ski_jump;
namespace N = venue_rules::ski_jump::norm;
using ge::core::clamp;
using ge::core::hypot;
using ge::core::kPi;

constexpr double kRad = kPi / 180;
constexpr double kG = 9.81;
// The pen's step, m of plan, and the step the curves are integrated on, m of arc.
constexpr double kPenStep = 0.05;
constexpr double kArcStep = 0.01;

// A point of a profile laid out by arc: plan `x`, height `y`, slope `a` (rad, down positive).
struct Pt {
  double x, y, a;
};

// The IN-RUN's slope `e` m of arc above the edge, the straight's top `top` m above it.
double in_run_slope(const HillElements& el, double e, double top) {
  if (e <= el.t) return el.table;
  if (e <= el.t + el.curve) {
    const double u = e - el.t;
    return el.table + (u - u * u / (2 * el.curve)) / el.r1;
  }
  if (e <= top) return el.in_run;
  return num::max(0.0, el.in_run - (e - top) / V::roll);
}

double sample_arc(const std::vector<double>& arc, double q) {
  const double f = clamp(q / kPenStep, 0, double(arc.size() - 1));
  const size_t i = std::min(arc.size() - 2, size_t(f));
  return arc[i] + (arc[i + 1] - arc[i]) * (f - double(i));
}

SkiJumpProfile build(JumpHill hill) {
  const V::Hill& row = hill_row(hill);
  const HillElements el = hill_of(row);
  // THE IN-RUN, by arc up from the edge (at plan 0, height 0, for now).
  const double top = el.e1 + V::above;
  std::vector<Pt> back{{0, 0, el.table}};
  double e = 0;
  double x = 0;
  double y = 0;
  double platform = 0;
  double mark_curve = 0;
  double mark_straight = 0;
  double mark_table = 0;
  for (;;) {
    const double a = in_run_slope(el, e + kArcStep / 2, top);
    e += kArcStep;
    x -= libm::cos(a) * kArcStep;
    y += libm::sin(a) * kArcStep;
    if (e >= el.t && mark_table == 0) mark_table = x;
    if (e >= el.t + el.curve && mark_curve == 0) mark_curve = x;
    if (e >= top && mark_straight == 0) mark_straight = x;
    back.push_back({x, y, a});
    if (a <= 0) platform += kArcStep;
    if (platform >= V::platform) break;
  }
  // THE GATES: equally spaced from e₂ to e₁, never past the most height apart (read on the
  // straight's angle, the steepest they stand on).
  const double span = el.e1 - el.e2;
  const int count = int(std::ceil(span * libm::sin(el.in_run) / V::gate_height)) + 1;
  // THE LANDING HILL, over the horizontal from the edge.
  const double cx = el.n + el.r_l * libm::sin(el.slope);
  const double cy = -el.h + el.r_l * libm::cos(el.slope);
  const auto on_arc = [&](double a) {
    return Pt{cx - el.r_l * libm::sin(a), cy - el.r_l * libm::cos(a), a};
  };
  const Pt P = on_arc(el.slope_p);
  const Pt L = on_arc(el.slope_l);
  // The knoll: a cubic from the foot (0, −s) at the foot's slope to P at P's.
  const double k0 = -el.s;
  const double m0 = -libm::tan(el.slope_foot);
  const double m1 = -libm::tan(el.slope_p);
  const auto knoll = [&](double q) {
    const double u = q / P.x;
    const double u2 = u * u;
    const double u3 = u2 * u;
    const double h00 = 2 * u3 - 3 * u2 + 1;
    const double h10 = u3 - 2 * u2 + u;
    const double h01 = -2 * u3 + 3 * u2;
    const double h11 = u3 - u2;
    return h00 * k0 + h10 * P.x * m0 + h01 * P.y + h11 * P.x * m1;
  };
  // L to U: the radius growing evenly from r2l to r2 over the arc.
  const double turn = el.slope_l;
  const double grow = row.r2 - row.r2l;
  const double length_lu = turn * grow / libm::log(row.r2 / row.r2l);
  std::vector<Pt> fore;
  {
    double u = 0;
    double fx = L.x;
    double fy = L.y;
    while (u < length_lu) {
      // Turned by ∫ du / r(u) = (l / (r₂ − r₂L))·ln(r / r₂L) so far.
      const double radius = row.r2l + grow * (u + kArcStep / 2) / length_lu;
      const double a = num::max(0.0, turn - (length_lu / grow) * libm::log(radius / row.r2l));
      u += kArcStep;
      fx += libm::cos(a) * kArcStep;
      fy -= libm::sin(a) * kArcStep;
      fore.push_back({fx, fy, a});
    }
  }
  const Pt U = fore.back();
  const auto landing_at = [&](double q) {
    if (q <= 0) return k0;
    if (q <= P.x) return knoll(q);
    if (q <= L.x) return cy - std::sqrt(el.r_l * el.r_l - (q - cx) * (q - cx));
    if (q >= U.x) return U.y;
    // Within the transition: the laid-out points, by plan.
    size_t lo = 0;
    size_t hi = fore.size() - 1;
    while (hi - lo > 1) {
      const size_t mid = (lo + hi) >> 1;
      if (fore[mid].x < q) {
        lo = mid;
      } else {
        hi = mid;
      }
    }
    const Pt& a = fore[lo];
    const Pt& b = fore[hi];
    const double f = b.x > a.x ? (q - a.x) / (b.x - a.x) : 0;
    return a.y + (b.y - a.y) * f;
  };
  // RESAMPLED every kPenStep of plan from the tower's deck; the edge stands on a sample, so the
  // wall under it is one step wide.
  const Pt deck = back.back();
  const double start = -std::ceil(-deck.x / kPenStep) * kPenStep;
  const double edge = -start;
  const double end = edge + U.x + el.a + V::past;
  const auto n = size_t(std::ceil(end / kPenStep)) + 1;
  SkiJumpProfile p;
  p.hill = hill;
  p.elements = el;
  p.dx = kPenStep;
  p.y.assign(n, 0);
  size_t j = back.size() - 1;
  for (size_t i = 0; i < n; i++) {
    const double q = double(i) * kPenStep + start;
    if (q <= deck.x) {
      p.y[i] = deck.y;
    } else if (q < -kPenStep / 2) {
      while (j > 0 && back[j - 1].x <= q) j--;
      const Pt& a = back[j];
      const Pt& b = back[j == 0 ? 0 : j - 1];
      const double f = b.x > a.x ? (q - a.x) / (b.x - a.x) : 0;
      p.y[i] = a.y + (b.y - a.y) * f;
    } else if (q < kPenStep / 2) {
      p.y[i] = 0;
    } else {
      p.y[i] = landing_at(q);
    }
  }
  // THE GROUND UNDER THE RAISED IN-RUN: a slope up from the knoll's foot under the edge, never
  // over the in-run itself.
  VenueProfile::Under under;
  under.y.assign(n, 0);
  under.bank = V::raised::bank;
  under.half = V::raised::half;
  const double rise = libm::tan(V::raised::slope * kRad);
  for (size_t i = 0; i < n; i++) {
    const double q = double(i) * kPenStep + start;
    under.y[i] = q < -kPenStep / 2 ? num::min(p.y[i], k0 - rise * q) : p.y[i];
  }
  p.under = std::move(under);
  // THE DISTANCE: the arc along the landing profile from the knoll's foot.
  const auto steps = size_t(std::ceil((end - edge) / kPenStep)) + 1;
  p.arc.assign(steps, 0);
  double prev = k0;
  for (size_t i = 1; i < steps; i++) {
    const double yq = landing_at(double(i) * kPenStep);
    p.arc[i] = p.arc[i - 1] + hypot(kPenStep, yq - prev);
    prev = yq;
  }
  p.scale = el.w / sample_arc(p.arc, el.n);
  for (int i = 0; i < count; i++) {
    const double arc_e = el.e2 + span * i / (count - 1);
    const auto at = size_t(clamp(num::round(arc_e / kArcStep), 0, double(back.size() - 1)));
    p.gates.push_back({i + 1, back[at].x - start, back[at].y, arc_e});
  }
  p.straight = mark_straight - start;
  p.curve = mark_curve - start;
  p.table = mark_table - start;
  p.edge = edge;
  p.p = edge + P.x;
  p.k = edge + el.n;
  p.l = edge + L.x;
  p.u = edge + U.x;
  p.outrun = edge + U.x + el.a;
  p.finish = edge + U.x + V::fall_line;
  p.end = end;
  return p;
}

// The lowest the hill's surface stands within a quarter cell either side of a grid node, so the
// grid under it is cut to the same surface and never stands above it.
void cut_under(ge::core::Heightfield& ground, const VenueSurface& f) {
  const double cell = ground.cell;
  double x_lo = std::numeric_limits<double>::infinity();
  double x_hi = -x_lo;
  double z_lo = x_lo;
  double z_hi = -x_lo;
  for (const double a : {-cell, f.end + cell}) {
    for (const double b : {-f.half - cell, f.half + cell}) {
      const double x = f.x + f.fx * a + f.fz * b;
      const double z = f.z + f.fz * a - f.fx * b;
      x_lo = num::min(x_lo, x);
      x_hi = num::max(x_hi, x);
      z_lo = num::min(z_lo, z);
      z_hi = num::max(z_hi, z);
    }
  }
  const int c0 = int(clamp(std::floor((x_lo - ground.origin_x) / cell), 0, ground.cols - 1));
  const int c1 = int(clamp(std::ceil((x_hi - ground.origin_x) / cell), 0, ground.cols - 1));
  const int r0 = int(clamp(std::floor((z_lo - ground.origin_z) / cell), 0, ground.rows - 1));
  const int r1 = int(clamp(std::ceil((z_hi - ground.origin_z) / cell), 0, ground.rows - 1));
  for (int r = r0; r <= r1; r++) {
    for (int c = c0; c <= c1; c++) {
      const double x = ground.origin_x + c * cell;
      const double z = ground.origin_z + r * cell;
      const double along = (x - f.x) * f.fx + (z - f.z) * f.fz;
      const double across = (x - f.x) * f.fz - (z - f.z) * f.fx;
      if (along < -cell || along > f.end + cell || std::fabs(across) > f.half + cell) continue;
      double low = std::numeric_limits<double>::infinity();
      for (int i = -2; i <= 2; i++) {
        for (int k = -2; k <= 2; k++) {
          const double a = along + i * cell * 0.25;
          const double b = across + k * cell * 0.25;
          double on = 0;
          if (!f.covers(f.x + f.fx * a + f.fz * b, f.z + f.fz * a - f.fx * b, on)) continue;
          low = num::min(low, f.height_at(a));
        }
      }
      float& h = ground.data[size_t(r) * size_t(ground.cols) + size_t(c)];
      if (low < std::numeric_limits<double>::infinity()) {
        h = static_cast<float>(num::min(double(h), low - 0.1));
      }
    }
  }
}

}  // namespace

const venue_rules::ski_jump::Hill& hill_row(JumpHill hill) {
  return hill == JumpHill::large ? V::large : V::normal;
}

HillElements hill_of(const V::Hill& row) {
  HillElements el{};
  el.w = row.w;
  el.hs = row.hs;
  const double tilt = libm::atan(row.steepness);
  el.landing = N::landing[0] * row.speed + N::landing[1];
  el.r_l = num::round(el.landing * el.landing * row.w / N::radius / N::round) * N::round;
  el.slope = row.slope * kRad;
  el.slope_p = (row.slope + N::p[0] * row.table - N::p[1]) * kRad;
  el.slope_l = el.slope - N::l / el.landing;
  el.r1 = N::r1[0] * (row.speed + N::r1[1]) * (row.speed + N::r1[1]);
  const double turn = (row.in_run - row.table) * kRad;
  el.h = row.w * libm::sin(tilt) / N::chord;
  el.n = row.w * libm::cos(tilt) / N::chord;
  el.in_run = row.in_run * kRad;
  el.table = row.table * kRad;
  el.slope_foot = el.slope_p / N::foot;
  el.speed = row.speed;
  el.t = N::t[0] * (row.speed + N::t[1]);
  el.s = num::max(N::s[1], N::s[0] * row.w);
  // A curvature growing evenly from nothing to 1/r₁ turns the in-run by its length over 2·r₁.
  el.curve = 2 * el.r1 * turn;
  el.l1 = el.r_l * (el.slope_p - el.slope);
  el.l2 = N::l * el.r_l / el.landing;
  el.e1 = N::e1[0] - N::e1[1] * row.in_run + N::e1[2] * row.hs;
  el.e2 = N::e2[0] - N::e2[1] * row.in_run + N::e2[2] * row.hs;
  el.a = num::max(N::a[3], N::a[0] + N::a[1] * row.w - N::a[2] * row.w * row.w);
  return el;
}

const SkiJumpProfile& ski_jump_profile(JumpHill hill) {
  static const SkiJumpProfile large = build(JumpHill::large);
  static const SkiJumpProfile normal = build(JumpHill::normal);
  return hill == JumpHill::large ? large : normal;
}

double jump_distance_at(const SkiJumpProfile& p, double x) {
  if (x <= p.edge) return 0;
  return sample_arc(p.arc, x - p.edge) * p.scale;
}

double measured_distance(double d) { return std::floor(d * 2 + 1e-9) / 2; }

double edge_speed(const SkiJumpProfile& p, int gate, double head) {
  const HillElements& el = p.elements;
  namespace S = V::skier;
  const double k = 0.5 * S::air * S::crouch / S::mass;
  const double top = el.e1 + V::above;
  const auto g = size_t(std::clamp(gate, 0, int(p.gates.size()) - 1));
  double e = p.gates[g].e;
  double v = 0;
  constexpr double kDe = 0.02;
  while (e > 0) {
    const double a = in_run_slope(el, e - kDe / 2, top);
    // Curvature where the in-run bends (it turns him up into the table).
    const double u = e - el.t;
    const double kappa = u > 0 && u < el.curve ? (1 - u / el.curve) / el.r1 : 0;
    const double air = v + head;
    const double acc =
        kG * libm::sin(a) - S::friction * (kG * libm::cos(a) + v * v * kappa) - k * air * air;
    v = std::sqrt(num::max(0.0, v * v + 2 * acc * kDe));
    e -= kDe;
  }
  return v;
}

int design_gate(const SkiJumpProfile& p) {
  for (size_t i = 0; i < p.gates.size(); i++) {
    if (edge_speed(p, int(i)) >= p.elements.speed) return int(i);
  }
  return int(p.gates.size()) - 1;
}

Level set_ski_jump(const Level& level, JumpHill hill) {
  if (level.ski_jump && level.ski_jump->hill == hill) return level;
  const std::shared_ptr<const Level> original = original_of(level);
  const SkiJumpProfile& p = ski_jump_profile(hill);
  const HillElements& el = p.elements;
  const VenueRule rule{V::width, V::ease, V::margin, V::search, V::fit, V::arena};
  GradedVenue v = grade_venue(*original, p, rule);
  const auto y_at = [&](double d) { return v.y_at(p, d); };
  const VenueLine& line = v.line;
  Level out = std::move(v.level);
  Kicker take_off{
      "SJ",  line.x + v.fx * p.edge, line.z + v.fz * p.edge, y_at(p.edge),    line.heading,
      el.s,  p.edge - p.table,       p.l - p.edge,           N::in_run_width, true,
      p.edge};
  out.kickers.push_back(std::move(take_off));
  const auto across = [&](double s, double width) {
    return Checkpoint{line.x + v.fx * s, line.z + v.fz * s, y_at(s), line.heading, width, s,
                      GateColour::red};
  };
  const SkiJumpHill::Gate& top_gate = p.gates.back();
  const Checkpoint start = across(top_gate.x, 4);
  const Checkpoint finish = across(p.finish, V::width);
  out.checkpoints = {start, finish};
  out.spawn = {line.x + v.fx * top_gate.x, line.z + v.fz * top_gate.x, line.heading};
  out.grid = {out.spawn};
  // THE SURFACE: the hill's own profile inside its width, only the deck up the raised in-run.
  auto surface = std::make_shared<VenueSurface>();
  surface->x = line.x;
  surface->z = line.z;
  surface->heading = line.heading;
  surface->fx = v.fx;
  surface->fz = v.fz;
  surface->dx = p.dx;
  surface->y = p.y;
  surface->shift = line.shift;
  surface->half = V::width / 2;
  surface->end = p.end;
  surface->narrow_until = p.edge;
  surface->narrow_half = V::raised::half;
  cut_under(out.ground, *surface);
  out.venue = std::move(surface);
  SkiJumpHill course;
  course.base = original;
  course.hill = hill;
  course.w = el.w;
  course.hs = el.hs;
  course.from = top_gate.x;
  course.to = p.finish;
  course.vertical = start.y - finish.y;
  course.straight = p.straight;
  course.curve = p.curve;
  course.table = p.table;
  course.edge = p.edge;
  course.p = p.p;
  course.k = p.k;
  course.l = p.l;
  course.u = p.u;
  course.outrun = p.outrun;
  course.take_off = el.table;
  course.lip = el.s;
  for (const SkiJumpHill::Gate& g : p.gates)
    course.gates.push_back({g.number, g.x, y_at(g.x), g.e});
  course.design = design_gate(p);
  course.speed = el.speed;
  const V::Hill& row = hill_row(hill);
  course.metre = row.metre;
  course.gate_factor = row.gate;
  course.head_factor = row.head;
  course.tail_factor = row.tail;
  course.width = V::width;
  course.deck = V::raised::half;
  out.ski_jump = std::move(course);
  out.sun = level.sun;
  out.weather = level.weather;
  return out;
}

double ski_jump_height_at(const Level& level, double d) {
  return level.venue ? level.venue->height_at(d) : 0;
}

}  // namespace ge::mountain::mapgen
