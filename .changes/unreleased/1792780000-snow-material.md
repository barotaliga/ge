---
type: Added
title: Ground materials and the snow's material
---

The terrain's colour pass is now a material's: a shader that includes `render/wgsl/terrain_draw.wgsl` (the vertex stage, the bindings, `ground_at` and `ground_out`) and defines `fs_main`, with a bind group of its own (`TerrainSource::shader`, `material`, `bind_terrain_material`); `terrain.wgsl` stays the default. The mountain plugin gains its presentation half, `ge_mountain_app` (built with the GPU), and in it the snow's material (`mountain/app/snow.hpp`): a ground map of the packed share, the rock and the piste's direction, the snow's swells and grain, the groomer's corduroy and lanes, the wrap's blue terminator, the glare and the glitter, every term faded by the pixel's footprint. Fall Line draws its snow with it; the new `snow` lab shoots the close views plain and with the material, the `world` lab gains the `corduroy`, `powder` and `glitter` views, and `world` and `profile` take `--snow=0`.
