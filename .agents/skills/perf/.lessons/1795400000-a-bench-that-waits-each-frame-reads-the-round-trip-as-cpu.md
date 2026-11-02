---
title: A bench that waits for every frame reads the GPU's round trip as CPU time; zone the frame before cutting
date: 2026-11-02
scope: games/shooters_edge/labs/gpu_se_timedemo_lab.cpp, games/shooters_edge/client/world_draw.cpp, plugins/brush/app/bsp_draw.cpp
concepts: [timedemo, frame-in-flight, cpu-zones, ge-zone, round-trip, dawn-submit, light-grid, sample]
---

- `se-timedemo` waited for each frame before the next while the window keeps one in flight, so at
  720p its frame (2.75 ms) ran 1.2 ms past the GPU's span (1.6) and read as "CPU-bound". Zoned,
  the CPU's whole part was 0.5 ms; the rest was the wait's round trip (span + about 0.6 ms on the
  M3 Max). Paced as the window, the frame is 1.85 ms. Before calling a frame CPU-bound, read
  `cpu by zone` (the lab's note) and compare `gpu.wait` with the span.
- The lab's picture frames wait for a read back; timed, they were the run's three worst frames
  and set the 0.1% low. They are untimed now (`bench.skip` before `record_span`, so the spans
  and the frames stay aligned).
- `sample <pid>` from an agent's shell on macOS captured no stacks (no task port): GE_ZONE
  needs no permission and stays in the tree. With frames overlapped, the CPU's zones read
  higher (0.6 to 0.9 ms) than serialized (0.5): it shares unified memory with a busy GPU.
- Of our own CPU work, the light grid's binning led (0.12 ms at 720p); a row of cells a sphere
  reaches is one run from one square root, which halved it with the same lists.
