// LAB `parallel-cost`: what handing a bake's rows to core::parallel_bands costs, by grid size and
// worker count. Each grid is a square of `side` rows baked by a five-point stencil (a few flops a
// cell, the cheap end of what a bake does), so the dispatch's own cost shows where a grid is small
// and the cores' share where it is large. The times are MEASURED metrics (median of `batches`,
// each the mean of enough calls to last about a millisecond): printed and compared with
// `--compare`, never in the digest. What IS in the digest is each grid's output hash, which must
// be the same on every worker count. `dispatch` times an empty band a worker: the floor a call
// pays before any work. Run it on the release preset.
//
// ge:summary Lab: wall-clock cost of parallel_bands by grid side and worker count (measured)
// ge:keywords lab bench perf parallel threads workers bands dispatch overhead pool speedup
// ge:skill perf labs determinism
// ge:test tests/lab_runner_test.cpp
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "core/parallel.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"

namespace {

using ge::lab::Det;
namespace gl = ge::lab;

// The stencil over a grid: every output cell a pure function of its row's neighbourhood.
void bake(const std::vector<double>& in, std::vector<double>& out, size_t side, size_t workers) {
  ge::core::parallel_bands(side, workers, [&](size_t r0, size_t r1) {
    for (size_t r = r0; r < r1; r++) {
      const size_t up = r == 0 ? r : r - 1;
      const size_t down = r + 1 == side ? r : r + 1;
      for (size_t c = 0; c < side; c++) {
        const size_t left = c == 0 ? c : c - 1;
        const size_t right = c + 1 == side ? c : c + 1;
        const double h = in[r * side + c];
        const double lap = in[r * side + left] + in[r * side + right] + in[up * side + c] +
                           in[down * side + c] - 4 * h;
        out[r * side + c] = h + 0.125 * lap;
      }
    }
  });
}

uint64_t hash_bits(const std::vector<double>& v) {
  uint64_t h = 1469598103934665603ull;
  for (const double d : v) {
    uint64_t b;
    std::memcpy(&b, &d, sizeof b);
    h = (h ^ b) * 1099511628211ull;
  }
  return h;
}

std::string hex(uint64_t h) {
  static const char* kDigits = "0123456789abcdef";
  std::string s(16, '0');
  for (int i = 15; i >= 0; i--, h >>= 4) s[size_t(i)] = kDigits[h & 15];
  return s;
}

// The median over `batches` of a call's mean time in microseconds, `calls` calls a batch.
template <class Call>
double median_us(int batches, size_t calls, Call&& call) {
  std::vector<double> us;
  call();  // warm: the first call may start what later calls reuse
  for (int b = 0; b < batches; b++) {
    const auto t0 = std::chrono::steady_clock::now();
    for (size_t k = 0; k < calls; k++) call();
    const double total =
        std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    us.push_back(total / double(calls));
  }
  std::sort(us.begin(), us.end());
  return us[us.size() / 2];
}

void shoot(gl::Lab& lab, const std::string& id, const gl::Chart& chart, const std::string& caption,
           const std::string& look_for) {
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()), Det::measured);
  lab.shot({id,
            chart.title,
            caption,
            look_for,
            {{"sides", lab.arg("sides")},
             {"workers", lab.arg("workers")},
             {"batches", lab.arg("batches")}}},
           gl::to_picture(chart.plot), Det::measured);
  lab.note(gl::chart_text(chart, 0));
}

void run(gl::Lab& lab) {
  const int batches = std::max(1, static_cast<int>(lab.arg_num("batches")));
  std::vector<size_t> workers;
  for (const double w : lab.arg_nums("workers")) workers.push_back(size_t(std::max(1.0, w)));
  std::vector<size_t> sides;
  for (const double s : lab.arg_nums("sides")) sides.push_back(size_t(std::max(2.0, s)));

  gl::LabTable bits{"bits", {"side", "cells", "output hash", "same on every worker count"}, {}};
  std::vector<gl::Trace> cost(workers.size()), speedup(workers.size());
  for (size_t w = 0; w < workers.size(); w++) {
    cost[w].label = speedup[w].label = std::to_string(workers[w]) + " workers";
  }
  for (const size_t side : sides) {
    std::vector<double> in(side * side), out(side * side);
    for (size_t i = 0; i < in.size(); i++) in[i] = double((i * 2654435761u) % 1000) / 1000.0;
    // About a millisecond of one core a batch at ~1 ns a cell, and at least one call.
    const size_t calls = std::clamp<size_t>(1000000 / (side * side), 1, 2000);
    const std::string s = std::to_string(side);
    uint64_t first = 0;
    bool same = true;
    double one = 0;
    for (size_t w = 0; w < workers.size(); w++) {
      bake(in, out, side, workers[w]);
      const uint64_t h = hash_bits(out);
      if (w == 0) first = h;
      same = same && h == first;
      const double us = median_us(batches, calls, [&] { bake(in, out, side, workers[w]); });
      if (w == 0) one = us;
      const std::string id = "side-" + s + ".w" + std::to_string(workers[w]);
      lab.metric(id + ".us", us, "us", Det::measured);
      lab.metric(id + ".speedup", one / us, "x", Det::measured);
      double at = 0;  // log2 of the side, a doubling a step: 16 rows is 4, 1024 is 10
      for (size_t k = side; k > 1; k >>= 1) at += 1;
      cost[w].x.push_back(at);
      cost[w].y.push_back(us);
      speedup[w].x.push_back(at);
      speedup[w].y.push_back(one / us);
    }
    bits.rows.push_back({s, std::to_string(side * side), hex(first), same ? "yes" : "NO"});
    if (!same) {
      lab.fail("side " + s + ": the bake's bits differ between worker counts",
               "A band's output must be a pure function of its rows: read engine/core/parallel.hpp "
               "and run tests/core_parallel_test.cpp (skill: determinism).");
    }
  }
  lab.table(std::move(bits));

  // The floor: one empty band a worker, nothing else.
  gl::LabTable floor{"dispatch", {"workers", "empty call (us)"}, {}};
  for (const size_t w : workers) {
    const double us =
        median_us(batches, 2000, [&] { ge::core::parallel_bands(w, w, [](size_t, size_t) {}); });
    lab.metric("dispatch.w" + std::to_string(w) + ".us", us, "us", Det::measured);
    floor.rows.push_back({std::to_string(w), std::to_string(us)});
  }
  lab.table(std::move(floor), Det::measured);

  gl::LineChart c;
  c.frame = {"A bake's call by grid side and worker count",
             "five-point stencil, median of " + lab.arg("batches") + " batches",
             "grid side (log2 of rows: 4 is 16, 10 is 1024)", "us per call"};
  c.traces = cost;
  c.log_y = true;
  c.spikes = false;
  shoot(lab, "cost", gl::line_chart(c),
        "Microseconds a parallel_bands call takes to bake a square grid, one line a worker "
        "count, log scale.",
        "on small grids the lines with more workers should not sit above the 1-worker line");
  gl::LineChart sp;
  sp.frame = {"Speed-up over one worker by grid side",
              "five-point stencil, median of " + lab.arg("batches") + " batches",
              "grid side (log2 of rows: 4 is 16, 10 is 1024)", "x faster than 1 worker"};
  sp.traces = speedup;
  sp.rules = {1.0};
  sp.spikes = false;
  shoot(lab, "speedup", gl::line_chart(sp),
        "How many times faster than one worker each worker count bakes the grid; the dashed line "
        "is break-even.",
        "every line at or above 1 (no loss from going wide), rising with the grid's size");
#ifndef NDEBUG
  lab.note("a DEBUG build: these times are not the engine's. Build PRESET=release to measure.");
#endif
}

const ge::lab::LabSpec kSpec{
    "parallel-cost",
    ge::lab::LabKind::bench,
    "engine",
    "engine/core/parallel.hpp",
    "perf",
    "What does a parallel_bands call cost by grid size and worker count, and does it pay?",
    {
        {"sides", "16,32,64,128,256,512,1024", "grid sides (rows = columns), comma-separated"},
        {"workers", "1,2,4,8", "worker counts, comma-separated; the first is the baseline"},
        {"batches", "15", "timed batches a point; the median is reported"},
    },
    "--sides=16,128 --workers=1,4 --batches=3",
    run,
};
GE_LAB(kSpec);

}  // namespace
