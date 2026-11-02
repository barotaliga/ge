---
title: A command added after console::start is unknown to the command line's + lines
date: 2026-11-02
scope: games/shooters_edge/client/main.cpp, games/fall_line/app/main.cpp, engine/console/cfg.hpp
concepts: [plus-commands, start, add-command, command-line, unknown-command, order, screenshot]
---

`ge::console::start` runs the command line's `+` lines at once, so a command a window registers
after it (once the device or the game folder exists) answers `+<it>` with "unknown command". The
first `shooters_edge +screenshot` printed exactly that. Register the command, and whatever state
it writes into, before `start`; keep the GPU half (a readback ring) a separate object made after
the device, so its buffers still go before the device does. Check a new command with `+<it>` on
the window's command line: it is the cheapest end-to-end run a window has (`--frames=40` quits).
