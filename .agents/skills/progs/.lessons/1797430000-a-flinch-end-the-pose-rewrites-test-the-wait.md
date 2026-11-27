---
title: Test a Quake II monster's pain by its wait, not its flinch's end
date: 2026-11-27
scope: games/shooters_edge/server/q2_monsters.cpp
concepts: [pain, pain-at, pain-hold, flinch, test, remix]
---

`Q2Monster::pain_hold` is set when a pain begins and then rewritten by the pose from the pain run
it picks, so it moves between frames without a new pain. A test that a second blow did or did not
start a pain reads `pain_at` (the wait's mark, which only a pain sets), and reads `pain_hold` only
to see that a flinch is on at all.
