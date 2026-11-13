---
type: Changed
title: The server check knows a mod's own settings; Fall Line's settings explain themselves
---

`server check` no longer calls the classic mod's own settings unknown: a server.cfg may set `sv_specprint`, `sv_phs`, `sv_longfraglog`, `sv_aim`, `coop`, `registered`, `sv_playermove` and `sv_idealpitchscale`, and the server hands them to the mod as it does its other rules. A game's rule may stay out of the server info (`GameRule::info`). Fall Line's settings and the camera's (`cl_fov`, `cl_bob`, `cl_camera` and the rest) now say what they do in one sentence and what each value does, like Shooter's Edge's.
