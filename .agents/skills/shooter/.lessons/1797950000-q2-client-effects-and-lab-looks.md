---
title: Quake II's client effects live in effects.cpp; a lab's Picture starts with Quake's defaults
date: 2026-11-21
scope: games/shooters_edge/app/effects.cpp, games/shooters_edge/client/world_fx.cpp, games/shooters_edge/labs/gpu_se_q2_models_lab.cpp
concepts: [flies, power-screen, thinning-trail, colour-shell, lab-picture, power-shells, effect-bits]
---

Quake II's particle effects are pure functions in `app/effects.cpp` (`q2_trail`, `q2_flies`,
`q2_rocket_fire`, `q2_bolt_trail`) and `client/world_fx.cpp` only calls them with per-entity
state in `FxState` (`q2_trails`, `flies_from`, `Bleeding::count`), since `world_fx.cpp` sits near
the 2000-line cap. A model drawn as a shell alone (the power screen's plate) is a second `Placed`
with `shell_only`, found in the server's model list by name, so the server must precache it
before the player model (kept last). A GPU lab that builds its own `look::Picture` gets the
struct's defaults, which are Quake's (`power_shells` and `item_pulse` off), not Quake II's cvar
layer: set them in the lab or every shell tile renders bare. Server missile rows carried no
effect bits until this round, so a protocol client saw no trails: check the wire for each bit
the world draws.
