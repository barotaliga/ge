---
title: An orthographic map of a closed level reads its empty texels as open sky; shut every texel no sky face covers
date: 2026-11-09
scope: plugins/brush/app/bsp_draw.cpp, plugins/brush/app/bsp_shadow.wgsl, engine/render/wgsl/fall_shelter.wgsl
concepts: [shelter, weather, rain, zenith, sun-map, sky-faces, stencil, closed-level, indoor, crack, seam]
---

A BSP level's faces point into its rooms: nothing is drawn inside a wall, so a depth map looking
down (or along the sun or the fall) has empty texels over every wall's thickness and every seam,
and an empty texel compares as "open". On dm4 that put rain, wet streaks and sky light in the
rooms' corners and down their walls. Testing the sky in the shader needs a second layer and
bytes `bsp.wgsl` does not have (it sits at the 60000-byte embed cap); instead the maps are
`kOrthoFormat` (depth and stencil) and drawn in three steps: solid faces, sky faces nearer than
them marking the stencil (depth test on, no write), a full-map triangle at the near plane where
the stencil is 0. Readers are unchanged. To see where rain reaches, diff a `se-client --views=`
run with `--before="...; r_precip 0; r_splashes 0"` against the same cvars with them on: the diff
is the particles alone. Rebuild after `git stash pop` before an "after" run.
