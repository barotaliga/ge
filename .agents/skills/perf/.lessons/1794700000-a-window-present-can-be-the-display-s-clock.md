---
title: A window's present can be the display's clock; time a benchmark offscreen, and time every pass
date: 2026-10-30
scope: games/shooters_edge/client/main.cpp, plugins/brush/app/bsp_draw.cpp, games/shooters_edge/client/world_draw.cpp
concepts: [benchmark, present-mode, immediate, macos, compositor, shadow-cache, pass-timer, timestamp-query]
---

On macOS an immediate present (display sync off) still held a window's frames to 8.33 ms and
16.7 ms: the compositor gives each image back once a refresh, so the next acquire waits. A
benchmark that must measure the machine draws its frames offscreen and shows only some (one in
33 ms here), with at most one frame in flight. Then the GPU per pass: the world frame's timer had
room for 16 passes and the frame had 28, so post, bloom and the UI read NaN and dropped out of
the report; size it `kMaxPasses`. The modern look's dearest work was not timed at all: the shadow
maps were drawn by bare `BeginRenderPass` calls outside the recorder (time a run of passes with
`render_part`). Once timed they were 438 of a 563 ms frame on llvmpipe: up to 32 lights x 6
cube faces of the whole map, every frame, for a world that never moves. Keeping each light's
cube while it holds its slot took them to 0.01 ms a frame, with every picture digest unchanged.
