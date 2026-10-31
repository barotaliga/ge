---
title: A GPU twin matches its CPU raster when it shares the raster's rules and numbers, and a line along edges is addressing
date: 2026-10-23
scope: engine/render/ui_pass.hpp, engine/render/ui_pass.wgsl, engine/ui/draw.cpp, engine/ui/raster.cpp, engine/render/viewmodel.wgsl, engine/lab/labs/gpu_viewmodel_lab.cpp
concepts: [gpu-twin, cpu-reference, parity, 2d-pass, ui, sdf, premultiplied, unorm, inset, atlas]
---

- The 2D pass matched `ui::render` within 3 levels first time by copying its RULES: coverage
  from the same signed distance at the pixel centre, the same centre-in-clip test, text from the
  same `place_glyphs` masks (atlas keyed by mask address), images through clamp-to-edge linear,
  a fill and border folded into one premultiplied (one, 1 - src alpha) blend, on a UNORM target
  because the raster blends sRGB bytes.
- Every number a shape needs (arc directions, edge normals, a turn's sine and cosine, bounds) is
  worked out once in the draw list with the engine's libm and handed to both; the shader calls no
  `sin`. An acute vertex still differs on the last bit, so judge parity by the count of pixels
  over a few levels, not the worst one.
- A model's twin that differs along its edges is reading the neighbouring atlas region: inset
  every region by half a texel (mean 3 to 0.18 levels). Look at where a difference lies before
  blaming the light. Factor any shared rule into one function both call, and hold them with a
  per-pixel diff test.
