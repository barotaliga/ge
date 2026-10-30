---
title: A map change makes its frame again; open the device's load for it, or every pipeline is "mid-run"
date: 2026-10-30
scope: games/shooters_edge/client/world_draw.cpp
concepts: [end-load, pipelines-after-load, map-change, bsp, manifest, loading]
---

`create_bsp` adds the map's pipelines to the manifest, and `gpu::end_load` closes the load after
the first map: a second map's frame would print one "created mid-run" error per pipeline and
count them in `pipelines_after_load`. A game that changes maps changes them behind a load (the
screen is the console's or a loading picture for that moment), so `create_world_draw` sets
`device.loading = true` before it builds and calls `end_load` after `finish`. Do the same for any
frame a game rebuilds whole at a level change; a pass made during play still owes the manifest.
