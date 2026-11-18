---
title: A setting that replaces a constant keeps the constant's arithmetic, or the goldens move
date: 2026-11-14
scope: plugins/shooter/pmove.cpp, plugins/shooter/pmove.hpp, plugins/shooter/settings.cpp, plugins/classic/sequel_move.cpp
concepts: [classic-remix, cr-physics, pm-swimscale, determinism, settings, swim, pmove]
---

The classic swimmer's `wishspeed * 0.7` multiplied a float by a DOUBLE 0.7 and rounded back to
float; turning it into `pm_swimscale` as a float (0.7f) would have changed the bits of every swim
and moved goldens with no behaviour asked for. The setting is a `double` in `MoveSettings`
(`GE_SETTING_FIELD(..., double)` takes it) and the expression keeps its cast, so the default
plays bit for bit as before. The sequel's `speed *= 0.5f` was float already, so its knob is a
float. Check the literal's type before lifting any constant into a knob. And a remix line that is
the rules' own (`set sv_falldamage 1`, read by the arena with `cvar`) is not a move setting:
whatever applies the lines to `ServerSettings` (the test, the lab) skips `set ` lines.
