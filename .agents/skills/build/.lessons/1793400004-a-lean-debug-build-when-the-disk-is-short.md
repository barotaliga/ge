---
title: A lean debug build when the container's disk is short
date: 2026-10-29
scope: CMakePresets.json, build/debug
concepts: [disk, debug-info, gpu-off, targets, worktree]
---

With about 2 GB free, a full `make build` failed with "No space left on device", because the
debug test binaries carry DWARF. This was enough for a CPU-only plugin:
- `cmake --preset debug -DGE_GPU=OFF -DCMAKE_CXX_FLAGS_DEBUG=-g0 -DCMAKE_C_FLAGS_DEBUG=-g0`.
- Then build only the targets the change reaches: `cmake --build build/debug --target <t>`.
  `make test T=<regex>` builds just the tests it names.

A new worktree can copy the main checkout's `.cache/deps` instead of fetching again. Say in the
report that the full build was not run.
