// INDEPENDENT ROWS SIDE BY SIDE. A bake whose every output (a grid row, a cell) is a pure
// function of its index splits [0, count) into contiguous bands and runs each on its own
// thread. The result is the same bits for any number of workers, because no output is written
// by two bands and nothing is summed across them; the caller keeps that promise. Never inside
// a simulation step: a step runs on one thread (the `perf` skill, § Levers).
//
// ge:summary Runs a bake's independent rows in contiguous bands on worker threads
// ge:keywords parallel threads workers bands rows bake parallel_bands default_workers
// ge:keywords multithreading
// ge:skill perf determinism
// ge:test tests/core_parallel_test.cpp
#pragma once

#include <algorithm>
#include <cstddef>
#include <thread>
#include <vector>

namespace ge::core {

// At most this many threads a bake: beyond it the bands' memory traffic, not the cores, sets
// the pace of a grid bake.
inline constexpr size_t kMaxWorkers = 8;

// The machine's hardware threads, 1 to kMaxWorkers (1 when the platform cannot tell).
inline size_t default_workers() {
  const size_t n = std::thread::hardware_concurrency();
  return std::clamp<size_t>(n, 1, kMaxWorkers);
}

// body(begin, end) once per band, the bands covering [0, count) in order with no overlap:
// band b is [count * b / bands, count * (b + 1) / bands). Band 0 runs on the calling thread.
// Returns once every band is done.
template <class Body>
void parallel_bands(size_t count, size_t workers, Body&& body) {
  const size_t bands = std::min(std::max<size_t>(workers, 1), count);
  if (bands <= 1) {
    if (count > 0) body(size_t{0}, count);
    return;
  }
  std::vector<std::thread> pool;
  pool.reserve(bands - 1);
  for (size_t b = 1; b < bands; b++) {
    pool.emplace_back(
        [&body, count, bands, b] { body(count * b / bands, count * (b + 1) / bands); });
  }
  body(size_t{0}, count / bands);
  for (std::thread& t : pool) t.join();
}

template <class Body>
void parallel_bands(size_t count, Body&& body) {
  parallel_bands(count, default_workers(), static_cast<Body&&>(body));
}

}  // namespace ge::core
