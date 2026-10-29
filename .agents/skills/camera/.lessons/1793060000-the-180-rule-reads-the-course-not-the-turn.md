---
title: The 180° rule reads the course's line, not his swinging turns, and along-the-line lenses are neutral
date: 2026-10-24
scope: engine/camera/plant.cpp
concepts: [broadcast, 180-degree-rule, trackside, neutral-shot, line-of-action, cut]
---

The first broadcast crossed his line on 23 of 27 cuts. Three causes, each found by printing the
cuts and the lenses' offsets from the piste: lenses alternated sides; the line was his velocity,
which swings ±40° through each turn, so a lens 100 m ahead flips side mid-turn (easing it over
1.5 s did not help); and a lens nearly behind or ahead of him counted as one side, when on screen
he runs at it or away (a neutral shot). Fix: one side of the piste, every lens checked off the
piste with `nearest_track_point` after planting (a candidate turned round a bend lands on it), the
course's direction passed as the line, and a lens within 20° of the line on neither side. 2 of 23.
