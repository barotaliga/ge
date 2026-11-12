---
title: Read a gap at the recordings' own rate and by kind before building for its guessed cause
date: 2026-11-12
scope: games/shooters_edge/labs/se_recipes_lab.cpp, games/shooters_edge/bots/fighter.cpp, plugins/shooter/pilot.cpp
concepts: [stops, brief-stop, reversal, uturn, turn-back, goal-chain, chains, retarget, se-recipes, se-people, samples, frame-rate, stop-kind]
---

- The plan said the bots stop more because they arrive at goals and replan; two features built
  on that (`chains`, `retarget`) worked as built and moved nothing (chains cost -2.5 a match).
  Arrivals were 3 of 29 stops a bot-minute, mostly at a YA not there to take. Count how often
  the guessed cause happens before building for it.
- The same stops read every 0.1 s (se-people `--samples`) were EQUAL for people and bots (9.6 /
  9.9); at the recordings' own rate (se-recipes) 12.3 / 18.8. The gap lived under 0.1 s: brief
  stops, a run reversed by pressing straight back through zero. A sampled reader hides what a
  frame-rate one shows: compare at the finest rate both sides have.
- Classify each stop by the motion across it (0.3 s before against 0.4 s after: still,
  reversed, turned, on) and by how long it was still; then trace the bots' kind by cause (goal
  changed, route turned back, keys reversed). The fix followed from the kind, not the cause:
  press a turn back across the run (`uturn`) and the bot curves through it at speed. +4.5 a
  match head to head, stops 18.8 -> 14.8, speed +9: a human-looking move also played better.
