---
title: A `+` line runs before the window registers its own commands; `+wait` puts it on a frame
date: 2026-11-02
scope: games/shooters_edge/client/main.cpp
concepts: [plus-commands, wait, console, window, smoke-run, basedir]
---

`ge::console::start` runs the command line's `+` lines before `main.cpp` has registered the
window's late commands (`basedir`, and anything added after the window opens), so `+basedir X`
alone prints an unknown command. `+wait` sends the rest of the line to the next frame, when they
exist: `shooters_edge "+wait" "+basedir" "~/Games/classic" "+wait" "+wait" "+map" "dm3"
--frames=1500` drives LOAD GAME and a map change headless of any click, the quickest smoke run of
the window's runtime paths.
