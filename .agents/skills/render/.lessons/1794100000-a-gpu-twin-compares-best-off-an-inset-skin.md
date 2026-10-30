---
title: A GPU twin held to a CPU raster disagrees first where a UV sits on a region's edge
date: 2026-10-30
scope: engine/lab/labs/gpu_viewmodel_lab.cpp, engine/render/viewmodel.wgsl
concepts: [cpu-reference, gpu-twin, compare, nearest, texel, uv, inset, atlas, edge, rasterization]
---

The `gpu-viewmodel` lab's first run put the GPU pass 3 levels from the CPU reference on average at
the smoke size, with a 99th centile of 112. The light was not the cause. A lab skin's regions ran
right to their edges (`v` 0 to 0.5), so a face's own edge read the first row of the region below
it. Where two faces met, the CPU and the GPU gave the shared pixel to different triangles, and one
of them read the wrong region: a brown line down a steel barrel. With every region inset by half a
texel, the mean fell to 0.18 and the 99th centile to 1. Before blaming a shader for a difference
from its twin, look at where the difference lies. A line along a model's edges is texel addressing
or the fill rule, not the light.
