---
title: A cvar lives at the layer of everything that reads it; a game extends the engine's struct
date: 2026-11-15
scope: engine/platform/window_cvars.hpp, games/shooters_edge/client/window_cvars.hpp
concepts: [cvars, layering, window-cvars, seams]
---

Settings any game with a window reads (`vid_fullscreen`, `vid_display`, `volume`) are bound by the
engine (`ge::platform::add_window_cvars`); a game's own setting of the same kind (`scr_levelstats`)
stays in the game, whose struct derives from the engine's and whose `add_window_cvars` calls the
engine's first, so the game's callers change nothing. Do not lift a setting only one game's rules
or weapons read (a weapon's tuning stays per game even when two games have that weapon).
