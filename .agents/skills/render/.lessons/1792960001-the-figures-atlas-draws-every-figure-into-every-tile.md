---
title: The figures' shadow atlas draws every moving figure into every tile
date: 2026-10-23
scope: engine/render/shadows.cpp, games/fall_line/app/frame.cpp
concepts: [shadows, atlas, casters, crowd, cost, cull, tiles]
---

The atlas culls its casters once, with every plane open, then draws that one list into each of
its sixteen tiles, with the viewport clipping. For a handful of racers that costs nothing. For a
crowd it costs the crowd times the tiles: 235 amateurs put the `shadows.figures` pass from 2 ms
to 385 ms on the software adapter.

Before a family of many joins `moving` in `prepare_sun_shadows`, give the atlas a cull a tile
(a caster slot a tile, each culled by its tile's own view), or keep the family out of it. Read
the `crowd-look` lab's `shadows-gpu` with the family and without it before deciding.
