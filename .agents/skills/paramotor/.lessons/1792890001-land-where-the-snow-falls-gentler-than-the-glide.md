---
title: A wing gliding near 4 lands only where the snow falls gentler than its glide; spiral down over it, then fly the final along it
date: 2026-10-23
scope: games/fall_line/sim/para_outing.cpp, plugins/paramotor/pilot.cpp
concepts: [landing, glide, spiral, final, stretch, terrain-following, crash]
---

On seed 38, the first outing aimed the bot at a point halfway down the summit run's fall. It ended
some 500 m over the snow there. A run steeper than about 1:4 falls away faster than the wing
sinks, so following it only gains height. The plugin's pilot then circled the aim, steering at it
from ever closer. The bank tightened into a spiral that met the snow at 57 g.

What works:
- **Pick the landing where a wing can reach the snow:** a stretch of the run that falls under
  0.14 for 300 m (`para_landing`).
- **Lose the height over its head:** spiral down on a fixed toggle with the throttle shut.
- **Fly the final along the run:** under 25 m, aim at a point 120 m ahead on the run, never at a
  fixed spot, and let the plugin's flare set him down. The landing came in at 3.4 g.

Never let a pilot steer at a spot it is about to pass over: the turn back tightens as the distance
shrinks.
