---
title: People's keys are read off a recording by direction, a keyboard steers by tapping between sets, and keys are judged off the recording
date: 2026-11-11
scope: games/shooters_edge/labs/keys_read.hpp, games/shooters_edge/bots/keyboard.cpp, engine/ai/keys.hpp, plugins/shooterbots/pilot.cpp, games/shooters_edge/labs/se_people_lab.cpp
concepts: [keys, keyboard, inferred-keys, key-set, keysteer, feathering, jumpkey, se-people, bumps, calm, fight]
---

- A recording updates a player ~29 times a second, 2-3 commands an update: a frame's speed is off by
  a third, its direction exact. Read keys by direction over long windows (`keys_read.hpp`): a steady
  run's heading off a steady view, at 280-340 only.
- An analog pilot under keys broke where a part-way axis held a speed: a full key overran a jump's
  speed window (run-ups 6 -> 24 % of the time). Feather the key round the speed wished, and let go
  on the WHOLE speed, not the speed along the wish (a body sliding across keeps it low: `kSlide`).
- Under the nearest key set a wish 22 degrees off is plain W: the bot cannot bear round a corner.
  Tapping the further set in for its share (sigma-delta, `keysteer`) cut walk bumps 11.0 -> 9.0 and
  is how people press (side-key runs ~0.08 s).
- A keyboard hand drops a press wanted for less than a finger takes: a pilot asking jump every other
  command needs a latch (`jumpkey`). Bypassing a novice's thumb hand before the keyboard cut bumps
  and lost speed: its smoothing keeps speed. Judge a hands change on speed first.
- Judge keys by se-people `keys.*` off the recording, never the trace's keys (the calm view raised W
  in the trace 25 -> 32 % and lowered it off the recording 26.7 -> 23.7, people 40), split calm and
  fight (`keys.calm.*`, `keys.fight.*`): most of the W gap was in calm running.
