---
title: Slow a map's fires on one clock (fire_time), the gas's rise and the light's flicker together, and keep the lean
date: 2026-11-08
scope: plugins/brush/app/bsp_flames.cpp, plugins/brush/app/draw_data.cpp, plugins/brush/app/bsp_draw.cpp
concepts: [fire, flame, torch, bonfire, flicker, pace, r-fire-pace, rise, lean, chop, pool]
---

A flame's motion has two sources: the shader carries its turbulence up at `rise_mps` (nothing else
in it moves with time), and its level is the map light's `flicker_at`, which also lights the walls.
Slowing only one makes the gas and the wall's light waver out of step. `kFirePace` scales both:
`fire_time` for `flicker_at` in `bsp_fires` AND `bsp_draw`, and `rise_mps` times the pace with the
air scaled alike (the lean is wind over rise, so slowing the rise alone bends the flame further).
A pool's "speed" is the chop's shortest waves (capillary-gravity: 5 cm runs ~6 Hz), so a calmer
pool is a longer `chop_shortest`, never a slowed clock. Film motion with `se-client --film=N`;
the pictures are measured, so a look change moves no digest.
