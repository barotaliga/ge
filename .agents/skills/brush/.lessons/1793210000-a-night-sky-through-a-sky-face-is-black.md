---
title: Under a night sky an eye adapted to the rooms sees black through the sky faces; lift the veil
date: 2026-10-28
scope: plugins/brush/app/bsp_sky.cpp, plugins/brush/app/bsp_draw.cpp, plugins/brush/app/bsp_fix.wgsl
concepts: [night, exposure, adapt-cd, veil, sky-light, sky-seen, blend-constant, moon, lava]
---

A classic map's rooms are lit in a white face's units, so the exposure follows whatever the eye is adapted to. Keyed on a moonlit night (`adapt_cd` 0), the halls go black and the lava blows out pink. Keyed on a torch-lit hall (`adapt_cd` 5 cd/m^2), the rooms keep their look, but the moonlit sky is a hundred times darker than them and reads as black: no cloud, only the moon's disc. The night's answer is both: keep the eye on the rooms and lift the sky as seen (`Options::sky_seen`, up to 40x).

The old veil multiplied the sky's pixels by the BLEND CONSTANT, and a constant over 1 changed nothing on lavapipe: it is clamped. The veil now writes its factor as the fragment's colour, with `dst * src` blending, so it can lift as well as dim. `--skylight` over 1 also lights the rooms by the sky, so use `--seen` to lift the sky alone.

To find a view of the moon, pass a few eyes in an open courtyard with `--at=0 --to-moon=1` at a small `--width`. Walls block most of them, and one look at the sheet picks the eye.
