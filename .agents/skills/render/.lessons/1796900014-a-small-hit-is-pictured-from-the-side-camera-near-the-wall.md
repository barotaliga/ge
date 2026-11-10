---
title: Debris off a classic wall shines in that face's own lightmap, in its full hue, judged from se-weapon-fx's side camera
date: 2026-11-10
scope: games/shooters_edge/app/weapon_fx.cpp, games/shooters_edge/client/world_fx.cpp, plugins/brush/light_probe.hpp
concepts: [impact, debris, chunks, dirt, side-along, particle-shine, lightmap_at, real-map, e1m1]
---

From the eye, a hit's pieces on a wall five metres off are a few pixels: run `se-weapon-fx` with
`--side-along=0.8` and `--size=960,540`, and crop round the hit. A particle is lit by the lamps and
the sky only, never by the map's lightmap, so indoors a lit speck reads as black soot. Make it
SHINE (negative `kelvin`) in the face's own light where it was hit (`brush::lightmap_at` off the
triangle `face_hit` found; `light_below`, the floor's light, is far darker than the wall's), times
about 3 for how the world pass draws a texel (`kWallSeen`). Keep the wall's whole hue: grey pieces
read blue beside a warm wall. Diff two frames' pixels to measure a piece against the wall. The
shareware pack's e1m1 (`--game=<folder>/id1 --map=e1m1`) is a real map with textures for this.
