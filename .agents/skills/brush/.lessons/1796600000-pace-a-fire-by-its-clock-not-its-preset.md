---
title: A map's fire and the wall it lights waver as one (fire_flicker), and its pace is the puffs' rate
date: 2026-11-08
scope: plugins/brush/app/bsp_flames.cpp, plugins/brush/app/draw_data.cpp, plugins/brush/app/bsp_draw.cpp
concepts: [fire, flame, torch, bonfire, flicker, pace, r-fire-speed, puff, lean, chop, pool]
---

A fire's motion has two consumers: the gas (`bsp_fires` sets each flame's `puff_hz` and `level`)
and the wall's light (`bsp_draw` multiplies the map light by `fire_flicker`). Both read the same
`render::flame_puff_level` of the main body's flame, so change the pace in `fire_flame` only; a
second clock makes the wall waver out of step with the flame. The lean is wind over rise, so a
bonfire's air and `rise_mps` are scaled by the pace together. A pool's "speed" is the chop's
shortest waves (capillary-gravity: 5 cm runs ~6 Hz), so a calmer pool is a longer
`chop_shortest`, never a slowed clock. Film motion with `se-client --film=N`; the pictures are
measured, so a look change moves no digest.
