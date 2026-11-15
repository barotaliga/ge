---
title: A test's host tick is 13 ms, not the sequel's 0.1 s rules frame
date: 2026-11-15
scope: tests/shooters_edge_sequel_gibs_test.cpp, tests/shooters_edge_sequel_fight_test.cpp
concepts: [sequel-rules, rules-frame, host-tick, test-timing, gibs]
---

The sequel tests' `run(a, n)` sends one packet a client per call with 13 ms moves, so 77 calls
make a second and 8 make one rules frame. A wait written as "8 × seconds" waits a tenth of what
it says: the gibs test looked like pieces that never landed. Count time in host ticks (77 a
second) or step the rules yourself (`gibs_frame` with `level_time += 0.1`).
