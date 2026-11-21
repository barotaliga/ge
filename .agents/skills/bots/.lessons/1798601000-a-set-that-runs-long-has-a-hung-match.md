---
title: A set that runs far past its time has a hung match, and a look-ahead loop must always advance
date: 2026-11-21
scope: plugins/shooterbots/look_ahead.cpp, games/shooters_edge/labs/team_watch.cpp, plugins/brush/survey.cpp
concepts: [hang, set, look-ahead, arc-clear, archeight, flights, crash, lintel, clip-hull, slice]
---

- A 12-seed 10-minute set takes minutes; one still running after half an hour is a hung match, not
  a slow one. Count the recordings it has saved by variant (`--save=.../<variant>-{seed}.mvd`):
  the variant short of 12 is the hung one, and its switch's code holds the loop.
- A traced look-ahead that stops at a floor or a ceiling and goes on from the hit must still let
  the chunk's time pass: `archeight`'s first cut added `fraction * dt`, and a box pinned between
  a floor and a ceiling met both at no distance forever (the whole set ran into the two-hour cap).
- A crash spot the flat look-ahead cannot see is found by `se-team --flights` (`high`: the hit
  missed by a trace at the takeoff's height and a step) and shown by `brush slice --hull=1`: the
  clip hull is where an origin cannot be, so a lintel's reach (dm2's pit edge bars an origin above
  64) shows where the drawn leaves are empty.
