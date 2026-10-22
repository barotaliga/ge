// engine/core/parallel.hpp: the bands cover every index once, in order, and a bake gives the
// same bits on any number of workers.
//
// ge:summary Test: core/parallel's bands and a bake's bits across worker counts
// ge:keywords test parallel threads workers bands
// ge:skill testing
#include "core/parallel.hpp"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <utility>
#include <vector>

#include "check.hpp"
#include "core/noise.hpp"

namespace {

using ge::core::parallel_bands;

// Each band records itself; the bands sorted by start must tile [0, count).
std::vector<std::pair<size_t, size_t>> bands_of(size_t count, size_t workers) {
  std::vector<std::pair<size_t, size_t>> bands;
  std::mutex m;
  parallel_bands(count, workers, [&](size_t b, size_t e) {
    const std::lock_guard<std::mutex> lock(m);
    bands.emplace_back(b, e);
  });
  std::sort(bands.begin(), bands.end());
  return bands;
}

std::vector<double> bake(size_t workers) {
  constexpr size_t kN = 257;
  std::vector<double> out(kN * kN);
  parallel_bands(kN, workers, [&](size_t r0, size_t r1) {
    for (size_t r = r0; r < r1; r++) {
      ge::core::NoiseField f = ge::core::noise_field(37.5, 38);
      for (size_t c = 0; c < kN; c++) {
        out[r * kN + c] = ge::core::sample_noise(f, double(c) * 2.0, double(r) * 2.0);
      }
    }
  });
  return out;
}

}  // namespace

TEST(parallel_bands_tile_the_range) {
  for (const size_t count : {size_t{0}, size_t{1}, size_t{5}, size_t{1000}}) {
    for (const size_t workers : {size_t{0}, size_t{1}, size_t{3}, size_t{8}, size_t{16}}) {
      const auto bands = bands_of(count, workers);
      size_t at = 0;
      for (const auto& [b, e] : bands) {
        CHECK(b == at);
        CHECK(e > b);
        at = e;
      }
      CHECK(at == count);
      CHECK(bands.size() <= std::max<size_t>(workers, 1));
    }
  }
}

TEST(parallel_bake_is_the_same_bits_on_any_worker_count) {
  const std::vector<double> one = bake(1);
  for (const size_t workers : {size_t{2}, size_t{3}, size_t{7}, ge::core::default_workers()}) {
    const std::vector<double> many = bake(workers);
    CHECK(many.size() == one.size());
    for (size_t i = 0; i < one.size(); i++) CHECK_BITS(many[i], one[i]);
  }
}

TEST(parallel_default_workers_is_in_range) {
  const size_t w = ge::core::default_workers();
  CHECK(w >= 1);
  CHECK(w <= ge::core::kMaxWorkers);
}
