---
title: A world's GPU half is many small costs; build it a step a frame, and keep what is constant
date: 2026-11-04
scope: games/shooters_edge/client/world_draw.cpp, games/shooters_edge/client/world_extras.cpp, engine/render/moon_maps.cpp, engine/gpu/pipeline.cpp
concepts: [hitch, begin-world, shader-module, tint-parse, moon-maps, mipmaps, build-steps, background-load, warm-up, shadow-cubes, first-all, acquire-wait]
---

After the CPU half of a map's world moved to a thread, `begin_world` still cost about 0.2 s on the window's thread. It was no one thing: `create_sky` decoded the other worlds' ten moon maps and worked their mips in doubles every time (115 ms), and every pass's `CreateShaderModule` parses its WGSL synchronously in Dawn (water 22 ms, liquids 13, flames 8, the BSP 17: half the samples under `begin_world` in `sample`). Pipelines compile asynchronously; modules do not.

Now constant inputs are prepared once a process (`moon_maps.cpp`'s `Prepared`, a function-local static), a device keeps each module by its text (`Device::shaders`), and the world is built in `kBuildSteps` steps, one per `world_compiled` call, so no frame takes more than one (25 ms at most). The new world's first frame shown was the last one: the BSP draws every light's shadow cubes on a map's first frame (32 cubes on dm6, 60 ms of GPU, seen as the NEXT frame's acquire waiting). It is now warmed up offscreen before it is shown (`warm_world`: `Options::first_all` off, two cubes a frame, nothing drawn but `encode_bsp_shadows` and the sky's tables) from the view the player will have. A GPU cost shows on the CPU as `surface.acquire` waiting; read the passes' times off `FrameRecorder::stats` to see which.

To find such costs: time each `create_*` with a steady clock around it, then `sample <pid> 5` over the load and sum the samples by function under `begin_world`.
