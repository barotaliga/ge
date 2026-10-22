// The allocation counts and the resident set (memory.hpp). The hook that feeds the counts is
// alloc_hook.cpp.
//
// ge:summary Allocation counters the hook feeds, and the peak resident set per platform
// ge:keywords prof memory allocations counters rss peak-rss getrusage
// ge:skill perf
// ge:test tests/prof_zones_test.cpp
#include "prof/memory.hpp"

#include <atomic>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
// windows.h first: psapi.h needs its types.
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

namespace ge::prof {

namespace {

std::atomic<int64_t> g_count{0};
std::atomic<int64_t> g_bytes{0};
std::atomic<int64_t> g_frees{0};

}  // namespace

namespace detail {

std::atomic<bool> g_counting{false};

void note_alloc(uint64_t bytes) {
  g_count.fetch_add(1, std::memory_order_relaxed);
  g_bytes.fetch_add(static_cast<int64_t>(bytes), std::memory_order_relaxed);
}

void note_free() { g_frees.fetch_add(1, std::memory_order_relaxed); }

}  // namespace detail

Allocations allocations() {
  return {detail::g_counting.load(std::memory_order_relaxed),
          g_count.load(std::memory_order_relaxed), g_bytes.load(std::memory_order_relaxed),
          g_frees.load(std::memory_order_relaxed)};
}

int64_t peak_rss_bytes() {
#if defined(_WIN32)
  PROCESS_MEMORY_COUNTERS pmc{};
  if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc) == 0) return 0;
  return static_cast<int64_t>(pmc.PeakWorkingSetSize);
#else
  rusage u{};
  if (getrusage(RUSAGE_SELF, &u) != 0) return 0;
#if defined(__APPLE__)
  return static_cast<int64_t>(u.ru_maxrss);  // bytes on macOS
#else
  return static_cast<int64_t>(u.ru_maxrss) * 1024;  // kilobytes elsewhere
#endif
#endif
}

}  // namespace ge::prof
