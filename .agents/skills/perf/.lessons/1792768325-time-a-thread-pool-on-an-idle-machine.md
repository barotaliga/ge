---
title: Time a threaded call on an idle machine, and alternate the two builds
date: 2026-10-23
scope: engine/core/parallel.hpp
concepts: [parallel, parallel-bands, thread-pool, dispatch, overhead, bench, a-b, load, tsan]
---

The `parallel-cost` lab's first runs beside a running `ctest -j4` put a thread start at 1 to 4
ms instead of 60 µs, and made the old per-call threads look ten times worse than they are; even
one-worker points (no threads at all) moved by half. Time on an idle machine, alternate the
before and after binaries (keep a copy of the before build's `labs`), and report the median of
three runs. A pool is checked for races cheaply without a sanitizer preset: compile
`tests/core_parallel_test.cpp`, `tests/main.cpp`, the pool and the core's few .cpp files by
hand with `-fsanitize=thread -DGE_SOURCE_DIR=...` and run it ten times.
