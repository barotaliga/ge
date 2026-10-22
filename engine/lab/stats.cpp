// The statistics' bodies (stats.hpp).
//
// ge:summary The statistics' bodies: summary, quantiles, MAD, outliers, spikes
// ge:keywords stats percentile median mad outlier spike
// ge:skill debug labs
// ge:test tests/lab_stats_test.cpp
#include "lab/stats.hpp"

#include <algorithm>
#include <cmath>

namespace ge::lab {

namespace {

constexpr double kSpikeFloor = 0.01;
constexpr double kSpikeRelative = 1e-6;

std::vector<double> finite(const std::vector<double>& values) {
  std::vector<double> out;
  out.reserve(values.size());
  for (const double v : values) {
    if (std::isfinite(v)) out.push_back(v);
  }
  return out;
}

// The indices whose |dev| / scale passes z, scale the robust sigma of `dev` but never under
// `floor` (so a smooth series, whose deviations are all near nothing, flags nothing).
std::vector<size_t> robust_flags(const std::vector<double>& dev, double z, double floor = 0) {
  std::vector<double> abs_dev;
  abs_dev.reserve(dev.size());
  for (const double d : dev) {
    if (std::isfinite(d)) abs_dev.push_back(std::fabs(d));
  }
  std::vector<size_t> out;
  if (abs_dev.empty()) return out;
  double scale = 1.4826 * median(abs_dev);
  if (scale <= 0) {
    double sum = 0;
    for (const double a : abs_dev) sum += a;
    scale = 1.2533 * sum / static_cast<double>(abs_dev.size());  // mean |dev| as a sigma
  }
  scale = std::max(scale, floor);
  if (scale <= 0) return out;
  for (size_t i = 0; i < dev.size(); i++) {
    if (std::isfinite(dev[i]) && std::fabs(dev[i]) / scale > z) out.push_back(i);
  }
  // Strongest first; ties (and equal strengths) in index order, so the order is deterministic.
  std::stable_sort(out.begin(), out.end(),
                   [&dev](size_t a, size_t b) { return std::fabs(dev[a]) > std::fabs(dev[b]); });
  return out;
}

}  // namespace

double quantile_sorted(const std::vector<double>& sorted, double q) {
  if (sorted.empty()) return 0;
  const double at = q * static_cast<double>(sorted.size() - 1);
  const auto lo = static_cast<size_t>(at);
  const size_t hi = std::min(lo + 1, sorted.size() - 1);
  const double f = at - static_cast<double>(lo);
  return sorted[lo] + (sorted[hi] - sorted[lo]) * f;
}

double median(std::vector<double> values) {
  values = finite(values);
  std::sort(values.begin(), values.end());
  return quantile_sorted(values, 0.5);
}

double mad(const std::vector<double>& values) {
  const double m = median(values);
  std::vector<double> dev;
  dev.reserve(values.size());
  for (const double v : values) {
    if (std::isfinite(v)) dev.push_back(std::fabs(v - m));
  }
  return median(dev);
}

Summary summarize(const std::vector<double>& values) {
  Summary s;
  std::vector<double> v = finite(values);
  s.n = v.size();
  s.nan = values.size() - v.size();
  if (v.empty()) return s;
  std::sort(v.begin(), v.end());
  double sum = 0;
  for (const double x : v) sum += x;
  s.min = v.front();
  s.max = v.back();
  s.mean = sum / static_cast<double>(v.size());
  s.p50 = quantile_sorted(v, 0.5);
  s.p95 = quantile_sorted(v, 0.95);
  s.p99 = quantile_sorted(v, 0.99);
  return s;
}

std::vector<size_t> outliers(const std::vector<double>& values, double z) {
  const double m = median(values);
  std::vector<double> dev(values.size());
  for (size_t i = 0; i < values.size(); i++) dev[i] = values[i] - m;
  return robust_flags(dev, z);
}

std::vector<size_t> spikes(const std::vector<double>& values, size_t window, double z) {
  const size_t half = std::max<size_t>(1, window / 2);
  std::vector<double> dev(values.size());
  std::vector<double> near;
  for (size_t i = 0; i < values.size(); i++) {
    near.clear();
    const size_t lo = i > half ? i - half : 0;
    const size_t hi = std::min(values.size(), i + half + 1);
    for (size_t k = lo; k < hi; k++) {
      if (k != i) near.push_back(values[k]);
    }
    dev[i] = values[i] - median(near);
  }
  // A spike stands out of the series' own spread too: a step of 1% of its p1..p99 range is the
  // least a deviation can be measured against, and a millionth of its size the least of all, so
  // a flat series' rounding (an angle that never moves, read to 1e-13) is not a spike.
  std::vector<double> sorted = finite(values);
  if (sorted.empty()) return {};
  std::sort(sorted.begin(), sorted.end());
  const double spread = quantile_sorted(sorted, 0.99) - quantile_sorted(sorted, 0.01);
  const double size = std::max(std::fabs(sorted.front()), std::fabs(sorted.back()));
  return robust_flags(dev, z, std::max(spread * kSpikeFloor, size * kSpikeRelative));
}

}  // namespace ge::lab
