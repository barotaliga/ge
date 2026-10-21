// The station pads: pressed, the approach cut, the unload ramp, levelled again and groomed.
//
// ge:summary The pads pressed into the ground, the approach cut, the unload ramp, the grooming
// ge:keywords station-pad press-pads level-pad cut-approach unload-rise relevel-pads groom-pads
// ge:keywords drag-top
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include "mountain/mapgen/station_pad.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/resort.hpp"
#include "mountain/mapgen/resort_rules.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace RR = resort_rules;
using ge::core::hypot;
using ge::core::sample_field;
using ge::core::smoothstep;

namespace {

// The grid cells within `far` of [x0, x1] × [z0, z1], as `for (row…) for (col…)` bounds.
struct Box {
  int c0, c1, r0, r1;
};
Box box_of(const ge::core::Heightfield& g, double x0, double x1, double z0, double z1, double far) {
  const double cell = g.cell;
  return {
      static_cast<int>(num::max(0.0, std::floor((x0 - far - g.origin_x) / cell))),
      static_cast<int>(num::min(double(g.cols - 1), std::ceil((x1 + far - g.origin_x) / cell))),
      static_cast<int>(num::max(0.0, std::floor((z0 - far - g.origin_z) / cell))),
      static_cast<int>(num::min(double(g.rows - 1), std::ceil((z1 + far - g.origin_z) / cell)))};
}

// A chair's UNLOAD RAMP at (x, z), a share of its height: whole under the chair and the lane
// beside it up to the unload point, falling from it on up the line.
double unload_rise(const StationPad& p, PlanPoint at, double x, double z) {
  namespace U = RR::lift::unload;
  const double along = (x - at.x) * p.dx + (z - at.z) * p.dz;
  const double across = std::abs((x - at.x) * p.dz - (z - at.z) * p.dx);
  return (1 - smoothstep(0, U::reach, along)) *
         (1 - smoothstep(U::half, U::half + U::edge, across)) *
         smoothstep(-U::back - U::edge, -U::back, along);
}

// One pad eased into `ground` at its own surface, and a chair's ramp raised on it.
void level_pad(ge::core::Heightfield& ground, const StationPad& p, const OnRun* on_run) {
  const double r = p.r;
  const double reach = r + RR::lift::pad_blend;
  const double cell = ground.cell;
  const Box b = box_of(ground, p.x, p.x, p.z, p.z, reach);
  for (int row = b.r0; row <= b.r1; row++) {
    for (int col = b.c0; col <= b.c1; col++) {
      const double px = ground.origin_x + col * cell;
      const double pz = ground.origin_z + row * cell;
      const double d = hypot(px - p.x, pz - p.z);
      if (d >= reach || (on_run && (*on_run)(px, pz))) continue;
      const size_t i = static_cast<size_t>(row) * static_cast<size_t>(ground.cols) + size_t(col);
      const double w = 1 - smoothstep(r, reach, d);
      double h = double(ground.data[i]) * (1 - w) + pad_surface(p, px, pz) * w;
      if (p.unload) h += RR::lift::unload::height * unload_rise(p, *p.unload, px, pz);
      ground.data[i] = static_cast<float>(h);
    }
  }
}

// R26 — THE APPROACH to a top: under the last stretch of the line the ground cut down below the
// rope's way in by a carrier's hang and a margin, only ever cut, never over a run's snow.
void cut_approach(ge::core::Heightfield& ground, const StationPad& p, const OnRun* on_run) {
  namespace A = RR::lift::top::approach;
  const bool gondola = p.kind == LiftKind::gondola;
  const auto pick = [gondola](PerLift v) { return gondola ? v.gondola : v.chair; };
  const double reach = num::min(A::length, p.length / 2);
  const double half = pick(A::half);
  const double far = half + A::blend;
  const double b0 = gondola ? A::from.gondola : A::from.chair;
  const double bx = p.x - p.dx * reach;
  const double bz = p.z - p.dz * reach;
  // The rope's way in, wheel to tower, and what hangs under it.
  const double wheel = p.y + pick(A::wheel);
  const double tower = sample_field(ground, bx, bz) + pick(A::tower);
  const double under = pick(A::hang) + A::clear;
  const double cell = ground.cell;
  const Box b = box_of(ground, num::min(p.x, bx), num::max(p.x, bx), num::min(p.z, bz),
                       num::max(p.z, bz), far);
  for (int row = b.r0; row <= b.r1; row++) {
    for (int col = b.c0; col <= b.c1; col++) {
      const double x = ground.origin_x + col * cell;
      const double z = ground.origin_z + row * cell;
      // Back down the line from the top, and across it.
      const double back = (p.x - x) * p.dx + (p.z - z) * p.dz;
      const double v = std::abs((x - p.x) * p.dz - (z - p.z) * p.dx);
      if (back < b0 - A::ease || back > reach || v > far || (on_run && (*on_run)(x, z))) continue;
      const double rope = wheel + ((tower - wheel) * back) / reach;
      const double w = (1 - smoothstep(half, far, v)) * smoothstep(b0 - A::ease, b0, back);
      const size_t i = static_cast<size_t>(row) * static_cast<size_t>(ground.cols) + size_t(col);
      const double cut = rope - under;
      const double y = ground.data[i];
      if (y > cut) ground.data[i] = static_cast<float>(y - (y - cut) * w);
    }
  }
}

}  // namespace

PadShape pad_shape() { return {RR::lift::top::pad / 2, RR::lift::top::deck, RR::lift::top::lean}; }

double pad_surface(const StationPad& p, double x, double z) {
  const double v = std::abs((x - p.x) * p.dz - (z - p.z) * p.dx);
  return p.y - p.lean * num::max(0.0, num::min(v, p.r) - p.deck);
}

StationPad drag_top(const LiftPlan& l, const HeightAt& height) {
  namespace D = RR::lift::drag;
  const double len = num::max(1.0, hypot(l.top.x - l.bottom.x, l.top.z - l.bottom.z));
  const double dx = (l.top.x - l.bottom.x) / len;
  const double dz = (l.top.z - l.bottom.z) / len;
  const double x = l.top.x - dx * D::let_go;
  const double z = l.top.z - dz * D::let_go;
  return {D::rim, D::rim,       0,  l.id, LiftKind::drag,  x,
          z,      height(x, z), dx, dz,   len - D::let_go, std::nullopt};
}

std::vector<StationPad> drag_tops_of(std::span<const LiftPlan> lifts,
                                     std::span<const std::string> froms, const HeightAt& height) {
  std::vector<StationPad> out;
  for (const LiftPlan& l : lifts) {
    if (padded(l.kind) || std::find(froms.begin(), froms.end(), l.id) == froms.end()) continue;
    out.push_back(drag_top(l, height));
  }
  return out;
}

PlanPoint unload_point(PlanPoint bottom, PlanPoint top) {
  const double len = num::max(1.0, hypot(top.x - bottom.x, top.z - bottom.z));
  const double back = RR::lift::unload::at / len;
  return {top.x + (bottom.x - top.x) * back, top.z + (bottom.z - top.z) * back};
}

std::vector<StationPad> press_pads(ge::core::Heightfield& ground, std::span<const LiftPlan> lifts,
                                   const PadShape& shape) {
  const double r = shape.r;
  std::vector<StationPad> pads;
  for (const LiftPlan& l : lifts) {
    if (!padded(l.kind)) continue;
    const double x = l.top.x;
    const double z = l.top.z;
    // The level: the middle's height eased down toward the lowest of the pad's rim by all but
    // the pad's cut of the fill it would need.
    const double mid = sample_field(ground, x, z);
    double lo = mid;
    for (int a = 0; a < 16; a++) {
      const double t = ((double(a) / 16) * ge::core::kPi) * 2;
      lo = num::min(lo, sample_field(ground, x + libm::sin(t) * r, z + libm::cos(t) * r));
    }
    const double y = lo + (mid - lo) * RR::lift::pad_cut;
    const double len = num::max(1.0, hypot(x - l.bottom.x, z - l.bottom.z));
    StationPad pad{
        shape.r,
        shape.deck,
        shape.lean,
        l.id,
        l.kind,
        x,
        z,
        y,
        (x - l.bottom.x) / len,
        (z - l.bottom.z) / len,
        len,
        l.kind == LiftKind::chair ? std::optional(unload_point(l.bottom, l.top)) : std::nullopt};
    level_pad(ground, pad, nullptr);
    if (shape.lean > 0) cut_approach(ground, pad, nullptr);
    pads.push_back(std::move(pad));
  }
  return pads;
}

void relevel_pads(ge::core::Heightfield& ground, std::span<const StationPad> pads,
                  const OnRun& on_run) {
  for (const StationPad& p : pads) {
    level_pad(ground, p, &on_run);
    if (p.lean > 0) cut_approach(ground, p, &on_run);
  }
}

void groom_pads(std::span<const StationPad> pads, ge::core::Heightfield& packed,
                const OnRun& on_run) {
  const double cell = packed.cell;
  for (const StationPad& p : pads) {
    const double r = p.r;
    const double reach = r + RR::lift::pad_blend / 2;
    const Box b = box_of(packed, p.x, p.x, p.z, p.z, reach);
    for (int row = b.r0; row <= b.r1; row++) {
      for (int col = b.c0; col <= b.c1; col++) {
        const double x = packed.origin_x + col * cell;
        const double z = packed.origin_z + row * cell;
        const double groom = 1 - smoothstep(r, reach, hypot(x - p.x, z - p.z));
        const size_t i = static_cast<size_t>(row) * static_cast<size_t>(packed.cols) + size_t(col);
        if (groom > packed.data[i] && !on_run(x, z)) packed.data[i] = static_cast<float>(groom);
      }
    }
  }
}

}  // namespace ge::mountain::mapgen
