---
title: A world's GPU half is many small costs, built a step a frame and warmed offscreen before it is shown
date: 2026-11-04
scope: games/shooters_edge/client/world_draw.cpp, games/shooters_edge/client/world_extras.cpp, engine/render/moon_maps.cpp, engine/gpu/pipeline.cpp
concepts: [hitch, begin-world, shader-module, moon-maps, build-steps, warm-up, shadow-cubes, first-all, acquire-wait]
---

`begin_world` cost 0.2 s on the window's thread: ten moon maps decoded with their mips in doubles
every time (115 ms) and each pass's shader module parsed (water 22 ms, the BSP 17). Now the moon
maps are prepared once a process, a device keeps modules by text, and the world is built in
`kBuildSteps` steps, one per `world_compiled` call. The BSP draws every light's shadow cubes on a
map's first frame (32 cubes on dm6, 60 ms of GPU), so the world is warmed offscreen first
(`warm_world`: `Options::first_all` off, two cubes a frame) from the player's view. To find such
costs, time each `create_*` with a steady clock and `sample <pid> 5` over the load.
