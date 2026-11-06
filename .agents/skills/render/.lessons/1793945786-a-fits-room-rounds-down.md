---
title: A fit in grains rounds the costs up and the room down
date: 2026-11-06
scope: engine/render/ladder.cpp
concepts: [fit-ladder, knapsack, grains, rounding, budget, ladder]
---

`fit_ladder` summed its costs in hundredths of a millisecond rounded up, and rounded the room up
too, so a room of 0.314 ms (32 hundredths) took a 0.32 ms picture: the `ladder` lab's sweep failed
the first time the table's prices landed on that edge. Round a capacity down and a demand up
(`room_grains`); `tests/render_ladder_test.cpp` holds a room between two hundredths.
