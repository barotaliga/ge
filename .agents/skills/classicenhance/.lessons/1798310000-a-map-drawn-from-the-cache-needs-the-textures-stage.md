---
title: The enhance lab's map views read the cache the game reads; a pictures-only cache draws nothing new
date: 2026-11-24
scope: plugins/classicenhance/labs/enhance_map.cpp, plugins/brush/labs/textured_view.hpp
concepts: [enhance-lab, map-views, cached-texture, find-cache, start-eyes, true-colour, named-views]
---

The `enhance` lab's map views (`map-<name>`) put a map's own textures beside the cache's, from the
same starts the `bsp` lab looks from (brush's `start_eyes`). On a local pack, `find_cache` finds
a cache with only the pictures stage too, but `cached_texture` then finds nothing, and both halves
come out the same. Before drawing, run the textures stage into the same root:
`classicenhance run <id1> --cache=<root> --only=textures --upscaler=kernel --device=cpu`. Each
view's metric `map-<name>.from-cache` counts the textures that came from the cache. It should be
near `.textures`; only the sky's and the liquids' stay the pack's.

The software view takes a replaced texture as `TrueColour`, at any multiple of the pack's size
(alpha 255 marks a glowing texel). Infer the multiple from the byte count, and sample it
bilinear. Sampled nearest, a 4x copy just looks like the pack's own at this panel size.
