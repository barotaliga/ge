---
title: A guard on the feet runs in the air too; a speed trick waits on the speed; a cut only cuts what nav_route checks
date: 2026-11-10
scope: plugins/shooter/brink.cpp, games/shooters_edge/bots/fighter.cpp, plugins/shooter/gap_jumps.cpp, plugins/shooter/nav_gaps.cpp, plugins/shooter/nav.cpp, plugins/shooter/pilot.cpp
concepts: [brink, lava, air-control, gap-jump, speed-jump, hop-speed, cut, nav-route, brush-floor, start-open]
---

- A run down a stair is AIRBORNE from step to step: a guard that runs only on the ground meets
  the bot on the last step already sliding over the lip (dm2: 67 lava deaths a 10-min set). In
  the air a wish against the run still brakes at the full acceleration (`air_accelerate` caps the
  speed GAINED along the wish, not the speed lost), so the guard works there: 67 -> 5.
- Reading jumps off demos: the hop reader's ground is the world model only, so a walk on a
  brush entity (a door floor, a bridge) reads as one long "hop". Trace the gap through the solid
  func_ entities, leave out the ones that rest elsewhere (start-open doors, plats, trains), and
  drop any "flight" longer than a jump's fall.
- A jump proof at speed ends within a step of its target node or fails; a trick's landing is a
  floor node near people's landing, wherever the flight came down (as the rocket tricks do).
- A speed trick needs the speed: the gap links held, but the bots' hop runs at ~250 u/s and the
  jumps take off at 420-500, so none was flown. Count how often a behaviour can fire (here: the
  hop's speed) before judging it.
- `nav_route` honours `cut` ONLY on a mover's link (a door's leaf): a cut gap planned again at
  once, 36k refusals a set. Check what the route actually reads before trusting a cut. The same
  means jumps missed twice ARE planned again unless they cross a mover (open: fix with an A/B).
