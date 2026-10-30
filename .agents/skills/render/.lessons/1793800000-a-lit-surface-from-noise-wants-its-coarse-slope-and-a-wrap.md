---
title: A medium's top lit off a noise wants the coarse slope and a wrapped cosine
date: 2026-10-29
scope: engine/render/wgsl/valley_fog.wgsl
concepts: [valley-fog, noise, normal, billows, wrap-lighting, low-sun, flicker]
---

The sea of fog's top first came out a flat, even slab: a level face under the sun reads as paint,
not fog. Shading it with the noise's slope at half a texel and a plain cosine fixed noon and broke
dawn: at a grazing sun every small turn of the slope flips lit to unlit, and the top became a
camouflage of orange and grey cells. Take the slope over two texels of the coarse octave only, light
it with a wrapped cosine (a fog is lit through itself, so no billow goes black) normalised so a
level top keeps a level face's light, and lay the normal level with range before the waves fall
under a pixel. Always look at a lit noise at the lowest sun the scene can have, not only at noon.
