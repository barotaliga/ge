---
title: Never shift a float clock far away to keep sources apart; tag each sample with its source
date: 2026-11-12
scope: games/shooters_edge/labs/se_aimlead_lab.cpp
concepts: [float, precision, clock, time, recording, demo, velocity, lab-bug]
---

The `se-aimlead` lab once kept the players of several recordings apart by adding
100000 s per recording to every time stamp. A float's step at 2e6 is 0.25 s.
Every later recording's samples collapsed onto a few times, every velocity read
from them was wrong, and the lab ranked the rules wrongly (a jump's arc looked
useless). The bug showed only when someone doubted a result and the path was
printed.

Keep each source on its own clock and tag its samples with the source (`Player::game`),
matching only within one source. When a lab's ranking surprises you, print a few
raw samples before you trust it.
