---
title: A world's GPU half is many small costs; build it a step a frame, and keep what is constant
date: 2026-11-04
scope: games/shooters_edge/client/world_draw.cpp, games/shooters_edge/client/world_extras.cpp, engine/render/moon_maps.cpp, engine/gpu/pipeline.cpp
concepts: [hitch, begin-world, shader-module, tint-parse, moon-maps, mipmaps, build-steps, background-load]
---

After the CPU half of a map's world moved to a thread, `begin_world` still cost about 0.2 s on the window's thread. It was no one thing: `create_sky` decoded the other worlds' ten moon maps and worked their mips in doubles every time (115 ms), and every pass's `CreateShaderModule` parses its WGSL synchronously in Dawn (water 22 ms, liquids 13, flames 8, the BSP 17: half the samples under `begin_world` in `sample`). Pipelines compile asynchronously; modules do not.

Now constant inputs are prepared once a process (`moon_maps.cpp`'s `Prepared`, a function-local static), a device keeps each module by its text (`Device::shaders`), and the world is built in `kBuildSteps` steps, one per `world_compiled` call, so no frame takes more than one (25 ms at most). What is left is the new world's first frame (about 40 ms on the M3 Max): the BSP draws every light's shadow cubes on a map's first frame by design.

To find such costs: time each `create_*` with a steady clock around it, then `sample <pid> 5` over the load and sum the samples by function under `begin_world`.
