---
title: A map load on the window's thread froze it for seconds; read on a thread, compile while drawing
date: 2026-11-04
scope: games/shooters_edge/client/main.cpp, games/shooters_edge/client/map_load.hpp, games/shooters_edge/client/world_draw.hpp, plugins/classicenhance/cached.cpp
concepts: [freeze, hitch, map-load, background-load, pipelines, find-cache, sha256, enhance, title, spinner]
---

The window drew no frame for about 12 s at start on the M3 Max: the first map's world made before the window opened, then made again when the background enhance run ended having changed nothing. A world cost about 3.3 s on the window's thread: 1.65 s WAITING on Dawn's async pipeline compiles (`PipelineSet::finish`), 1.3 s of the cache's PNGs decoded, and 1.25 s hashing the 179 MB pack three times (`find_cache`, a SHA-256 each call). `sample <pid> 5` on macOS shows it at once.

Now: `find_cache` keeps each pack's digest; `read_world` (the CPU half) runs on a thread; `begin_world` uploads and starts the pipelines; `PipelineSet::ready` polls them while frames go on; `finish_world` takes the world. The window opens first, on the title, with a spinner while anything loads (`client/map_load.hpp`). What is left on the window's thread is `begin_world`'s uploads (about 0.22 s once a load: the exit line's "the longest" frame says so). A measured run (`--frames`, a timedemo) waits each step out, so its first frame is its map's.

An enhance run that ends with every stage up to date (`RunReport::stages[].ran` all false) changes nothing: do not make the world again for it.
