// LAB `mapgen-cost`: what a map costs to build, by wall clock (G2's budget for a level). The times
// are MEASURED metrics: printed and compared with `--compare`, never in the digest. What IS in the
// digest is each map's `level_digest`, so a speed-up that changed what was built shows as a digest
// move. Run it on the release preset: a debug build with sanitizers times the sanitizers.
//
// ge:summary Lab: wall-clock cost of building maps (measured), with each map's level_digest
// ge:keywords lab bench perf cost mapgen generate_level timing wall-clock before-after
// ge:skill mountain labs perf
// ge:test tests/lab_runner_test.cpp
#include <algorithm>
#include <chrono>
#include <string>

#include "lab/lab.hpp"
#include "mountain/labs/level_map.hpp"
#include "mountain/mapgen/digest.hpp"

namespace {

using ge::lab::Det;

void run(ge::lab::Lab& lab) {
  const int reps = std::max(1, static_cast<int>(lab.arg_num("reps")));
  ge::lab::LabTable t{"maps", {"seed", "attempt", "best ms", "level_digest"}, {}};
  double total = 0;
  int n = 0;
  for (const double seed : lab.arg_nums("seeds")) {
    double best = 1e300;
    std::string digest;
    int attempt = 0;
    for (int r = 0; r < reps; r++) {
      const auto t0 = std::chrono::steady_clock::now();
      const auto level = ge::mountain::labs::build_map(lab, seed);
      const double ms =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
      if (!level) break;
      best = std::min(best, ms);
      digest = ge::mountain::mapgen::level_digest(*level);
      attempt = level->attempt;
    }
    if (digest.empty()) continue;
    const std::string s = std::to_string(static_cast<long long>(seed));
    lab.metric("seed-" + s + ".ms", best, "ms", Det::measured);
    total += best;
    n++;
    // The table's cells are hashed: keep the time out of them.
    t.rows.push_back({s, std::to_string(attempt), "-", digest});
    lab.note("seed " + s + ": best of " + std::to_string(reps) + " = " +
             std::to_string(static_cast<int>(best)) + " ms");
  }
  if (n) lab.metric("mean-ms", total / n, "ms", Det::measured);
  lab.table(std::move(t));
#ifndef NDEBUG
  lab.note("a DEBUG build: these times are not the engine's. Build PRESET=release to measure.");
#endif
}

const ge::lab::LabSpec kSpec{
    "mapgen-cost",
    ge::lab::LabKind::bench,
    "plugin:mountain",
    "plugins/mountain/mapgen",
    "perf",
    "How long does a map take to build, best of N, and did a speed-up change what was built?",
    {
        {"seeds", "38,7,1", "seeds, comma-separated"},
        {"reps", "3", "builds a seed; the best is reported"},
        ge::mountain::labs::kVersionFlag,
        ge::mountain::labs::kRegionFlag,
    },
    "--seeds=38 --reps=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
