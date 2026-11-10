---
title: A view the pilot sets for one command is not the view that fires once a hand flies the view
date: 2026-11-09
scope: plugins/shooter/bot.cpp, plugins/shooter/pilot.cpp, plugins/shooter/pilot.hpp, engine/ai/mouse.hpp
concepts: [mouse, hand, rocket-jump, flip, shot, pitch, pilot, view, override, dm2]
---

The pilot fires a rocket jump with a pitched view in the same command as the jump (80 degrees
down, the shot pressed). Once the mouse hand flew the view, that one-command view never reached
the server: the branch for "a view the pilot sets itself" reset the hand's pitch to level, and the
fighter's after-the-fact override then stroked from there, so the rocket left at 0.3 degrees and
every rocket jump was a plain jump (rise 44-59 units against ~250). Nothing failed loudly: the
count of rocket jumps even rose, as the pilot retried them. Read a move's OUTCOME (the rise in a
second after the shot, off se-team's `--trace`), not its count, after any change to who writes
the view.

The fix is a contract between the pilot and the hand: the pilot says the shot coming (its view,
the time to it, `Pilot::shot_*`), the hand starts its stroke one Fitts time before it (the aim and
a look give way inside that window), the shot command takes the view exactly, and the pilot waits
to fire until the hand says the view is near (`shot_hand`, `shot_ready`). Any other one-command
view (a grenade jump's throw) needs the same.
