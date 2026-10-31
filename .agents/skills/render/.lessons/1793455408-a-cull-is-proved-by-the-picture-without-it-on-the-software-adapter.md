---
title: A cull is proved by the picture drawn without it, to the bit, on the software adapter only
date: 2026-10-22
scope: engine/render/instances.hpp, engine/render/instances_cull.wgsl, games/fall_line/labs/gpu_world_lab.cpp, plugins/brush/app/bsp_cull.wgsl
concepts: [cull, occlusion, hi-z, two-phase, cull-check, picture, digest, macos, paravirtual, compaction]
---

- A cull that drops something visible costs a few pixels no reviewer sees. The `world` lab
  redraws every view with `FrameOptions::occlusion` off and compares RGB bytes (`cull-faults`).
  It can be exact only because the prepass and the `equal` colour pass make the picture
  independent of draw order; a blending draw would force a tolerance.
- When a GPU result disagrees with a CPU replica over the same inputs, suspect a per-thread query
  (`textureDimensions` at a varying level) before the arithmetic; simpler suspects "fixed" it by
  moving the rectangle.
- Read the cull's counters too: a late phase that retested the early phase's faces doubled
  "drawn" with no change in the picture. A face lost through three aligned doorways in a classic
  map is its visibility data, not the GPU.
- Apple's paravirtual Metal adapter drops a pixel on some runs only. Gate the bit check on the
  software adapter; on hardware measure and warn (view, early/late/hidden counts, the box) and
  attach nothing, because an attachment's NAME enters the digest even when its bytes are measured.
