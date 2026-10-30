---
title: A pilot tuned on plain hands fell apart under late ones until it stopped aiming at points it stands on
date: 2026-10-30
scope: plugins/shooter/pilot.cpp, games/shooters_edge/labs/se_pilot_lab.cpp
concepts: [pilot, hands, delay, run-up, orbit, facing, brake, ride, plat, humanness]
---

The shooter's pilot passed every course route with plain hands while human hands (0.15 s late)
arrived on 7 of 12. Each fix had the same cause: a point aimed at from close by turns the view a
long way in one step, and late hands answer that a moment later from somewhere else, so the
player orbits the point. The run-up point was circled until the pilot was told to stand there
turned down the jump (forward 0, go when slow and facing); the goal of a short route was circled
until the walk slowed into it; turning round to run back was a wide loop until forward was scaled
by how much the view already faces the way (all of it within 60 degrees, so a corner's own lag
does not make the forward axis dither). A ride's top was "arrived" only within 12 units of its
node, so a player who boarded at the plat's edge rode up and down for ever, and walked off the far
side when let off; the graph now rides only columns where the whole player box stands on the top,
and the pilot steps to the middle while carried. Test new pilot logic with the human hands over a
dozen seeds from the start: plain hands hide all of this.
