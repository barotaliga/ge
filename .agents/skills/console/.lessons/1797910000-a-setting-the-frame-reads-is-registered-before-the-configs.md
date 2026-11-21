---
title: A setting a later-built object reads lives in main and is registered before the configs run
date: 2026-11-21
scope: games/shooters_edge/client/main.cpp, games/shooters_edge/client/room_ear.cpp
concepts: [cvar-registration-order, start-configs, saved-settings, world-draw, settings-struct]
---

Shooter's Edge runs the saved configs (`start_configs`) early in `main`; the world draw, the
synth's hearings and the device come hundreds of lines later. A cvar registered after the configs
never sees its saved value (the config line names a setting that did not exist yet and is passed
over). So a setting read by something built later (`s_underwater` read by the world draw's ear)
is a small struct declared next to `q2_screen` before `start_configs`, its cvars bound there, and
the later object is handed a pointer (`hear_worlds_on(&synth, &under_ear)`), never a field of the
later object.
