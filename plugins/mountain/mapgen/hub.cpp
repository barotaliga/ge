// The hub laid across the valley floor, groomed, and its two wind tunnels on their beds.
//
// ge:summary The hub's band and edges, its grooming and fringe, the tunnels' beds pressed
// ge:keywords hub plan-hub hub-clear groom-hub lay-tunnels bed-of press-bed outside-hub
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include "mountain/mapgen/hub.hpp"

#include <cmath>
#include <limits>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/noise.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/resort_rules.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace H = resort_rules::hub;
namespace T = resort_rules::tunnel;
using ge::core::hypot;
using ge::core::sample_field;
using ge::core::smoothstep;
using ge::core::value_noise;

namespace {

// The wavelength the woods' edge wanders over along the hub, m.
constexpr double kEdgeScale = 45;
// How far either side of a tunnel's width its bed is eased into the floor, m; how many stations
// either way a pass of the bed's smoothing averages over, and the most passes.
constexpr double kBedEase = 10;
constexpr int kBedReach = 8;
constexpr int kBedPasses = 12;
// The step the hub's upper edge is walked up the face by, m.
constexpr double kWalk = 5;

// A tunnel's bed: the floor under its stations, smoothed pass by pass until no window of it falls
// steeper than seven tenths of `tunnel.grade`.
std::vector<double> bed_of(std::vector<double> ys, double step) {
  const size_t k = static_cast<size_t>(num::max(1.0, num::round(T::window / step)));
  const auto steepest = [&](const std::vector<double>& v) {
    double m = 0;
    for (size_t i = 0; i + k < v.size(); i++) {
      m = num::max(m, std::abs(v[i + k] - v[i]) / (double(k) * step));
    }
    return m;
  };
  const size_t n = ys.size();
  for (int pass = 0; pass < kBedPasses && steepest(ys) > T::grade * 0.7; pass++) {
    std::vector<double> next(n);
    for (size_t i = 0; i < n; i++) {
      const size_t lo = i >= size_t(kBedReach) ? i - size_t(kBedReach) : 0;
      const size_t hi = i + size_t(kBedReach) < n ? i + size_t(kBedReach) : n - 1;
      double sum = 0;
      for (size_t j = lo; j <= hi; j++) sum += ys[j];
      next[i] = sum / double(hi - lo + 1);
    }
    ys = std::move(next);
  }
  return ys;
}

// Press a tunnel's bed into the floor: the ground across its width (and a little) set to the bed,
// eased back into the floor over `kBedEase` either side and past either end.
void press_bed(ge::core::Heightfield& ground, const std::vector<double>& xs,
               const std::vector<double>& bed, double z) {
  const double half = T::width / 2 + 2;
  const double reach = half + kBedEase;
  const size_t n = xs.size();
  const double x0 = num::min(xs.front(), xs.back());
  const double x1 = num::max(xs.front(), xs.back());
  const double step = (x1 - x0) / num::max(1.0, double(n - 1));
  const bool asc = xs.front() <= xs.back();
  const double cell = ground.cell;
  const int c0 =
      static_cast<int>(num::max(0.0, std::floor((x0 - kBedEase - ground.origin_x) / cell)));
  const int c1 = static_cast<int>(
      num::min(double(ground.cols - 1), std::ceil((x1 + kBedEase - ground.origin_x) / cell)));
  const int r0 = static_cast<int>(num::max(0.0, std::floor((z - reach - ground.origin_z) / cell)));
  const int r1 = static_cast<int>(
      num::min(double(ground.rows - 1), std::ceil((z + reach - ground.origin_z) / cell)));
  for (int r = r0; r <= r1; r++) {
    const double across = std::abs(ground.origin_z + r * cell - z);
    for (int c = c0; c <= c1; c++) {
      const double x = ground.origin_x + c * cell;
      const double past = num::max(0.0, x0 - x, x - x1);
      const double w = (1 - smoothstep(half, reach, across)) * (1 - smoothstep(0, kBedEase, past));
      if (w <= 0) continue;
      const double u =
          num::min(double(n - 1), num::max(0.0, (num::min(x1, num::max(x0, x)) - x0) / step));
      const size_t i = static_cast<size_t>(num::min(double(n - 2), std::floor(u)));
      const double f = u - double(i);
      const size_t a = asc ? i : n - 1 - i;
      const size_t b = asc ? i + 1 : n - 2 - i;
      const double y = bed[a] + (bed[b] - bed[a]) * f;
      const size_t k = static_cast<size_t>(r) * static_cast<size_t>(ground.cols) + size_t(c);
      const double was = double(ground.data[k]);
      ground.data[k] = static_cast<float>(was + (y - was) * w);
    }
  }
}

}  // namespace

std::optional<HubEdges> hub_at(const Hub& hub, double x) {
  const size_t n = hub.top.size();
  const double t = (x - hub.x0) / hub.step;
  if (n == 0 || t < 0 || t > double(n - 1)) return std::nullopt;
  if (n == 1) return HubEdges{hub.top[0], hub.bottom[0]};
  const size_t i = static_cast<size_t>(num::min(double(n) - 2, std::floor(t)));
  const double f = t - double(i);
  const size_t j = i + 1;
  return HubEdges{hub.top[i] + (hub.top[j] - hub.top[i]) * f,
                  hub.bottom[i] + (hub.bottom[j] - hub.bottom[i]) * f};
}

double outside_hub(const Hub& hub, double x, double z) {
  const double x1 = hub.x0 + hub.step * double(hub.top.size() - 1);
  const double cx = num::min(x1, num::max(hub.x0, x));
  const std::optional<HubEdges> edges = hub_at(hub, cx);
  if (!edges) return std::numeric_limits<double>::infinity();
  const double dz = num::max(0.0, edges->top - z, z - edges->bottom);
  return hypot(x - cx, dz);
}

HubPlan plan_hub(const TerrainPlan& plan, const ge::core::Heightfield& ground,
                 std::span<const PlanPoint> floor, uint32_t seed) {
  const double cx = plan.size / 2;
  const double inner = plan.flank_band.inner - 40;
  double min_x = std::numeric_limits<double>::infinity();
  double max_x = -min_x;
  double lowest = plan.base_z;
  for (const PlanPoint& p : floor) {
    min_x = num::min(min_x, p.x);
    max_x = num::max(max_x, p.x);
    lowest = num::max(lowest, p.z);
  }
  if (floor.empty()) {
    min_x = cx;
    max_x = cx;
  }
  // The tunnels never shorter than `tunnel.length`: a hub whose stations stand close together is
  // still crossed end to end.
  const double short_by = T::length - (max_x - min_x);
  if (short_by > 0) {
    min_x = num::max(cx - inner + H::step, min_x - short_by / 2);
    max_x = num::min(cx + inner - H::step, min_x + T::length);
  }
  // The tunnels' two lines, below the lowest finish; the band's lower edge below the outer one.
  const double lane1 = lowest + T::under;
  const double lane2 = lane1 + T::gap;
  const double least = lane2 + T::width / 2 + H::inside;
  const double x0 = std::floor(num::max(cx - inner, min_x - H::reach) / H::step) * H::step;
  const double x1 = std::ceil(num::min(cx + inner, max_x + H::reach) / H::step) * H::step;
  const size_t n = static_cast<size_t>(num::max(2.0, num::round((x1 - x0) / H::step) + 1));
  std::vector<double> xs(n);
  for (size_t i = 0; i < n; i++) xs[i] = x0 + double(i) * H::step;
  // The lower edge out over the floor, wandering a little along it, never inside the outer
  // tunnel.
  std::vector<double> bottoms(n);
  for (size_t i = 0; i < n; i++) {
    bottoms[i] = num::max(
        least, plan.base_z + H::below + H::wander * (2 * value_noise(xs[i], 0, H::ease, seed) - 1));
  }
  // The upper edge as the ground has it: walked up from the floor until it has risen `rise`,
  // within the depth's band, and closing at its two ends so the band ends in the woods' curve.
  std::vector<double> contour(n);
  for (size_t i = 0; i < n; i++) {
    const double x = xs[i];
    const double bottom = bottoms[i];
    const double floor_y = sample_field(ground, x, bottom);
    double z = bottom - H::depth.min;
    while (z > bottom - H::depth.max && sample_field(ground, x, z) - floor_y < H::rise) z -= kWalk;
    const double end = num::min(x - x0, x1 - x);
    const double open = H::depth.min + (H::depth.max - H::depth.min) * smoothstep(0, H::reach, end);
    contour[i] = num::max(z, bottom - open);
  }
  // ...but above every finish and station near this column.
  const double half = H::ease / 2;
  std::vector<double> need(n, std::numeric_limits<double>::infinity());
  for (size_t i = 0; i < n; i++) {
    for (const PlanPoint& p : floor) {
      if (std::abs(p.x - xs[i]) <= half + H::step) need[i] = num::min(need[i], p.z - H::margin);
    }
  }
  std::vector<double> raw(n);
  for (size_t i = 0; i < n; i++) raw[i] = num::min(contour[i], need[i]);
  const size_t k = static_cast<size_t>(num::max(1.0, num::round(half / H::step)));
  std::vector<double> top(n);
  for (size_t i = 0; i < n; i++) {
    const size_t lo = i >= k ? i - k : 0;
    const size_t hi = i + k < n ? i + k : n - 1;
    double sum = 0;
    for (size_t j = lo; j <= hi; j++) sum += raw[j];
    const double mean = sum / double(hi - lo + 1);
    top[i] = num::max(bottoms[i] - H::depth.max,
                      num::min(num::min(mean, need[i]), bottoms[i] - H::depth.min));
  }
  return {{x0, H::step, std::move(top), std::move(bottoms)}, {lane1, lane2}, min_x, max_x};
}

bool hub_clear(const Hub& hub, uint32_t seed, double x, double z) {
  const double d = outside_hub(hub, x, z);
  if (d == 0) return true;
  if (d >= H::fringe) return false;
  return d < H::fringe * value_noise(x, z, kEdgeScale, seed);
}

void groom_hub(const Hub& hub, ge::core::Heightfield& packed) {
  const double cell = packed.cell;
  const double x1 = hub.x0 + hub.step * double(hub.top.size() - 1);
  double z0 = std::numeric_limits<double>::infinity();
  double z1 = -z0;
  for (const double t : hub.top) z0 = num::min(z0, t);
  for (const double b : hub.bottom) z1 = num::max(z1, b);
  z0 -= H::fade;
  z1 += H::fade;
  const int c0 = static_cast<int>(num::max(0.0, std::floor((hub.x0 - H::fade) / cell)));
  const int c1 =
      static_cast<int>(num::min(double(packed.cols - 1), std::ceil((x1 + H::fade) / cell)));
  const int r0 = static_cast<int>(num::max(0.0, std::floor(z0 / cell)));
  const int r1 = static_cast<int>(num::min(double(packed.rows - 1), std::ceil(z1 / cell)));
  for (int r = r0; r <= r1; r++) {
    for (int c = c0; c <= c1; c++) {
      const double d = outside_hub(hub, c * cell, r * cell);
      const double groom = 1 - smoothstep(0, H::fade, d);
      const size_t i = static_cast<size_t>(r) * static_cast<size_t>(packed.cols) + size_t(c);
      if (groom > double(packed.data[i])) packed.data[i] = static_cast<float>(groom);
    }
  }
}

std::vector<WindTunnel> lay_tunnels(const HubPlan& h, ge::core::Heightfield& ground) {
  const auto line = [&](const char* id, double z, double from, double to) {
    const double length = std::abs(to - from);
    const size_t n = static_cast<size_t>(num::max(1.0, num::round(length / T::step)));
    const double sgn = num::sign(to - from);
    const double dir = sgn == 0 ? 1 : sgn;
    const double heading = ge::core::libm::atan2(dir, 0);
    std::vector<double> xs(n + 1);
    std::vector<double> raw(n + 1);
    for (size_t i = 0; i <= n; i++) {
      xs[i] = from + (dir * (double(i) * length)) / double(n);
      raw[i] = sample_field(ground, xs[i], z);
    }
    const std::vector<double> bed = bed_of(std::move(raw), length / double(n));
    press_bed(ground, xs, bed, z);
    WindTunnel t{id, {}, length, T::width, T::speed};
    t.points.reserve(n + 1);
    for (size_t i = 0; i <= n; i++) {
      t.points.push_back(
          {xs[i], z, sample_field(ground, xs[i], z), (double(i) * length) / double(n), heading});
    }
    return t;
  };
  std::vector<WindTunnel> out;
  out.push_back(line("W1", h.lanes[0], h.from, h.to));
  out.push_back(line("W2", h.lanes[1], h.to, h.from));
  return out;
}

}  // namespace ge::mountain::mapgen
