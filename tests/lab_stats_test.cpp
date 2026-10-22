// engine/lab/stats: the quantiles, the median and MAD, outliers that one wild value cannot hide,
// spikes that a smooth climb does not make, and the strongest first.
//
// ge:summary Test: quantiles, MAD, robust outliers and spikes, strongest first
// ge:keywords test stats percentile median mad outlier spike
// ge:skill testing debug
#include <vector>

#include "check.hpp"
#include "lab/stats.hpp"

namespace gl = ge::lab;

TEST(quantiles_and_the_summary) {
  const std::vector<double> v{5, 1, 4, 2, 3};
  CHECK(gl::median(v) == 3);
  CHECK(gl::mad(v) == 1);
  const gl::Summary s = gl::summarize(v);
  CHECK(s.n == 5 && s.min == 1 && s.max == 5 && s.mean == 3 && s.p50 == 3);
  CHECK(gl::quantile_sorted({0, 10}, 0.25) == 2.5);
}

TEST(outliers_strongest_first) {
  std::vector<double> v;
  v.reserve(100);
  for (int i = 0; i < 100; i++) v.push_back(10 + (i % 5) * 0.1);
  v[20] = 30;
  v[70] = 50;
  const auto o = gl::outliers(v);
  CHECK(o.size() == 2);
  CHECK(o.size() == 2 && o[0] == 70 && o[1] == 20);
  CHECK(gl::outliers(std::vector<double>(10, 4.0)).empty());
}

TEST(a_smooth_climb_has_no_spikes_and_a_jump_is_one) {
  std::vector<double> v;
  v.reserve(600);
  for (int i = 0; i < 600; i++) v.push_back(i * 0.1 + 0.0001 * (i % 3));
  CHECK(gl::spikes(v).empty());
  v[300] += 20;
  const auto s = gl::spikes(v);
  CHECK(s.size() == 1 && s[0] == 300);
}
