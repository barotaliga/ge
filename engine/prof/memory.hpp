// WHAT THE PROCESS HOLDS: heap allocations counted as they happen, and the resident set's peak.
//
// Counting is opt-in at link time: an executable that calls `count_allocations()` links the
// engine's replacement of the global operator new and delete (alloc_hook.cpp), which counts every
// allocation and free and then calls malloc and free as before (so a sanitizer's leak checker still
// sees every block). The play executable and the labs do; a game's own app need not. Without it,
// `allocations().counting` is false and every count is 0.
//
// `ge mem` reads these through the play executable's `bench` (engine/play/bench.hpp): the
// allocations each step makes after the first second (a hot path that allocates is a finding),
// the zone they were made in, and the peak resident set.
//
// ge:summary Heap allocation counts (the operator new hook) and the process's peak resident set
// ge:keywords prof memory allocations alloc-count operator-new heap rss peak-rss resident leak
// ge:skill perf
// ge:test tests/prof_zones_test.cpp
#pragma once

#include <cstdint>

namespace ge::prof {

struct Allocations {
  bool counting = false;  // the hook is linked into this executable
  int64_t count = 0;      // allocations since the process began
  int64_t bytes = 0;      // bytes asked for by them
  int64_t frees = 0;
};

Allocations allocations();
// Links the counting hook into this executable (call it once from main); harmless twice.
void count_allocations();
// The resident set's peak since the process began, bytes; 0 where the platform does not say.
int64_t peak_rss_bytes();

namespace detail {
void note_alloc(uint64_t bytes);
void note_free();
}  // namespace detail

}  // namespace ge::prof
