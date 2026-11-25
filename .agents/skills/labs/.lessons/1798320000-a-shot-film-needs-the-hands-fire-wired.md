---
title: A lab that films the held weapon firing must wire the hand's fire and cases itself
date: 2026-11-24
scope: games/shooters_edge/labs/gpu_se_client_lab.cpp, games/shooters_edge/client/world_fx_shells.cpp
concepts: [se-client, fire-film, held-cases, fire-held, fx-switches, shells, filmstrip, view-pitch]
---

`step_fx` throws the cases of other players and monsters. The held gun's muzzle fire and cases
come from two separate calls, `fire_held` and `held_cases`, made after `step_held`. A lab that only
calls `step_held` draws the kick with no flash and no case. Those calls also need `fx_state.on`
set from the enhanced look's `fx_switches`, because a fresh `FxState` has every effect off.

The cases leave the port low and to the right, below a level view. Pitch the view down about 35
degrees (`--views=x,y,z,yaw,35`) so the floor where they land is in frame. A Quake pump hull lands
about 1.2 s after the shot, so film at least 100 host frames.
