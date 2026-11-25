---
title: A game library's single player lives in q2_native_save.cpp; check it black-box with a built stock library
date: 2026-11-21
scope: games/shooters_edge/server/q2_native.cpp, games/shooters_edge/server/q2_native_save.cpp, tests/q2_native_fixture_game.cpp
concepts: [game-library, native-save, kept-level, unit, autosave, latched-setting, spawn-point]
---

The native runner's `nv_start` calls `native_leave` (keep the level being left, or empty the
working folder), `native_new_game` or `native_load_begin` before the old level goes, and
`native_enter` after the spawn, its two frames and the baselines. The game library writes its
own files by the full paths we hand it into a per-host temp folder; a save is that folder packed
into an engine/save chunk. Two traps: the host's `deathmatch 1` default makes the window's
`deathmatch 0` only a latched value, so nothing was single player until a new game puts latched
settings in and restarts the game; and `pending_map` must stay the bare map (the host loads it),
so the whole level string (`*`, `$spot`, `+`) is kept apart in `NativeHost::level`. A real check
is cheap: build the stock game library from its source (`make game`, run only, never read) and
drive it on base1 from a scratch test; the fixture library must clear its players' `inuse` at
spawn as a real game does, or every carried player looks like a saved body.
