---
title: A free ride's chain of runs is longer than any course, so size its time limit off the chain
date: 2026-10-22
scope: games/fall_line/sim/free.cpp, games/fall_line/labs/free_lab.cpp, tests/fall_line_free_test.cpp
concepts: [free-ride, run-chain, time-limit, odometer, golden, generator-version]
---

A piste that flows into another is skied to the foot of the run it joins, so the bot's line on
the free ride can be twice the piste's own length. On seed 38 (generator v10), the black `8`
flowing into `3` is a 4.3 km chain. At 300 s the lab failed it 280 m short of the foot. With
600 s, every one of 65 pistes over nine seeds reaches a foot, the slowest in 313 s. Before
reading a "did not arrive" as a bug, compare the chain's length (the lab's `length m` column)
with the time limit.

Two more traps:

- The odometer counts only metres on the snow. A test that compares it to the arc skied must
  allow for the flights over the kickers.
- A golden held at "the current generator" moves the day another change adds a generator
  version. `fall_line_free_test` and the `free` lab are pinned to v10 for that reason.
