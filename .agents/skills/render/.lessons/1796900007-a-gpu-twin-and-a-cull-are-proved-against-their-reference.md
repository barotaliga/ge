---
title: A GPU twin matches its CPU raster by sharing its rules, and a cull is proved by the picture without it on the software adapter
date: 2026-10-22
scope: engine/render/ui_pass.hpp, engine/render/ui_pass.wgsl, engine/ui/draw.cpp, engine/ui/raster.cpp, engine/render/viewmodel.wgsl, engine/render/instances.hpp, engine/render/instances_cull.wgsl, plugins/brush/app/bsp_cull.wgsl, plugins/brush/app/bsp.wgsl, games/fall_line/labs/gpu_world_lab.cpp
concepts: [gpu-twin, cpu-reference, parity, 2d-pass, sdf, premultiplied, atlas, cull, occlusion, hi-z, cull-check, light-grid, macos, paravirtual]
---

- The 2D pass matched `ui::render` within 3 levels by copying its RULES: coverage from the same
  signed distance at the pixel centre, text from the same `place_glyphs` masks, one premultiplied
  blend, a UNORM target. Every number a shape needs is worked out once with the engine's libm
  and handed to both (the shader calls no `sin`); judge parity by the count of pixels over a few
  levels. A twin differing along edges reads the neighbouring atlas region: inset every region
  by half a texel.
- When a GPU result disagrees with a CPU replica, suspect a per-thread query (`textureDimensions`
  at a varying level) before the arithmetic.
- The `world` lab redraws every view with `FrameOptions::occlusion` off and compares bytes
  (`cull-faults`); it is exact only because the prepass and `equal` pass make the picture
  order-independent. Read the cull's counters too (a late phase retesting early faces doubled
  "drawn"). Apple's paravirtual adapter drops a pixel on some runs: gate the bit check on the
  software adapter, and on hardware warn and attach nothing.
- A light cull that keeps the picture to the byte lists lights in ascending order and leaves out
  only those whose reach misses the cell by a margin. `se-timedemo`'s pictures vary on Metal;
  judge sameness with `se-client --game=<packs> --map=e1m1` against a build of `main`.
