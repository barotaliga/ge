// THE STATISTICS a chart and a report read off a column of numbers: the percentiles, the robust
// spread and the OUTLIERS, found the same way everywhere so "an outlier" means one thing in a
// lab's note, a `ge graph` finding and a ring on a chart.
//
// - `summarize`: n, min, max, mean, p50, p95, p99 (linear between the nearest ranks); NaN cells
//   are skipped and counted.
// - `outliers`: the values whose robust z-score, |v - median| / (1.4826 MAD), passes `z` (3.5 by
//   default). The median and the MAD do not move for a few wild values the way a mean and a
//   standard deviation do, so one spike cannot hide another. A column with no spread (MAD 0)
//   falls back to the mean absolute deviation, and an all-equal one has no outliers.
// - `spikes`: the same test on what is left after a rolling median (`window` samples) is taken
//   off, for a SERIES OVER TIME: a value that is ordinary for the run but jumps out of its
//   neighbours (a hitch in a frame time, a snap in a joint) is a spike, a slow climb is not. The
//   robust scale of those deviations is floored at 1% of the series' p1..p99 range, so a smooth
//   signal (a 120 Hz speed, whose step-to-step deviations are all near nothing) flags nothing
//   but a real jump; and never under a millionth of the series' largest value, so a flat one's
//   rounding flags nothing either.
//
// No transcendental, sorts on copies: everything here is deterministic.
//
// ge:summary Percentiles, median, MAD and robust outliers and spikes for charts and reports
// ge:keywords stats statistics percentile median mad outlier spike robust-z summary p95 p99
// ge:keywords rolling-median anomaly summarize
// ge:skill debug labs
// ge:test tests/lab_stats_test.cpp
#pragma once

#include <cstddef>
#include <vector>

namespace ge::lab {

struct Summary {
  size_t n = 0;    // finite values
  size_t nan = 0;  // NaN values skipped
  double min = 0, max = 0, mean = 0, p50 = 0, p95 = 0, p99 = 0;
};

Summary summarize(const std::vector<double>& values);
// The q-th quantile (0..1) of `sorted` (ascending, finite), linear between ranks.
double quantile_sorted(const std::vector<double>& sorted, double q);
double median(std::vector<double> values);
// The median absolute deviation from the median.
double mad(const std::vector<double>& values);

inline constexpr double kOutlierZ = 3.5;

// Indices of the outliers of `values` (NaN never one), the strongest first.
std::vector<size_t> outliers(const std::vector<double>& values, double z = kOutlierZ);
// Indices of the spikes of a series over time, the strongest first.
std::vector<size_t> spikes(const std::vector<double>& values, size_t window = 9,
                           double z = kOutlierZ);

}  // namespace ge::lab
