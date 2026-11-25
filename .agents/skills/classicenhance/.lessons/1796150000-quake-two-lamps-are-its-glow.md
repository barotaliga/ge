---
title: Quake II's glow and its rebake's light come from its surface lights, not from a palette range
date: 2026-11-23
scope: plugins/brush/read_v38.cpp, plugins/brush/rebake.cpp, plugins/brush/app/draw_data.cpp, plugins/classicenhance/profile.hpp
concepts: [surface-light, lamp, fullbright, ce-glow, ce-rebake, emit, quake2, fullbright-first, flowing]
---

Quake II has no fullbright palette range. Its lamps are faces whose texinfo carries the light flag
(0x1) and a value. The values are large: base1 has 7000 to 20000. Read them onto the texture
(`Texture::lamp`), and never apply Quake's 224-and-up rule to its textures or skins
(`Profile::fullbright_first`). A key that changes only for the new value keeps Quake's caches
valid.

A Quake II lamp face is already lit past 1 by its own lightmap, so the classic and modern looks'
`max(lit, 1)` glow barely shows. The glow only shows under the rebake, which adds `albedo * glow`.
So judge a lamp change with `ce_rebake` on, and with the rebake's emission (value over 250,
colour squared) beside it.

The full game's cache takes more than two hours in the debug build (textures plus every map's
rebake). The machine's disk also fills while precheck links its test binaries. Rebake only up to
the maps you picture, then delete the cache before running precheck.
