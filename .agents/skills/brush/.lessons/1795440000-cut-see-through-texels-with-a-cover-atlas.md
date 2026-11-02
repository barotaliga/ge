---
title: Cut see-through texels with a cover atlas, never the texture atlas's alpha
date: 2026-11-01
scope: plugins/brush/app/draw_data.cpp, plugins/brush/app/bsp.wgsl, plugins/brush/app/bsp_draw.cpp, engine/render/wgsl/frame_types.wgsl
concepts: [cover, cut-out, alpha, fence-texture, atlas, wgsl-size, replacement-texture]
---

The map's texture atlas spends its alpha on GLOW (255 a fullbright texel, smoothstepped in
`bsp.wgsl`), and `TextureSource::replace` hands colour with that same glow alpha, so a picture's
see-through alpha cannot ride in it. Keep it in `Scene::cover`, one byte a texel in the same places
and mips, empty when every texel is solid (the GPU then binds a 1 x 1 stand-in and the shader
skips the lookup, so an existing map's pictures and digests do not move). `bsp.wgsl` sits near the
60 KB embed cap: whole-line comments are dropped at embed, comments after code are not, so the
cheap headroom is moving the shared includes' trailing comments onto their own lines; share
helpers (`wrapped_at`) rather than copying the wrap. To find a fixture view, the halls' map units
map to engine metres as (x, z, y) x 0.025 with y up: the hall's middle is about (6.4, 1.6, 4.8).
