---
title: A pusher waits on its own clock, so a test of a mover reads its whole path
date: 2026-11-05
scope: games/shooters_edge/rules/campaign_set_pieces.cpp, tests/shooters_edge_campaign_set_pieces_test.cpp
concepts: [qc, pusher, ltime, calc-move, func-train, test]
---

A brush mover (MOVETYPE_PUSH) thinks on `.ltime`, which only runs while it moves or waits, and
a train's next leg starts at once after a corner with no wait. A check of "where it is after N
seconds" lands mid-leg and fails by a few units: record the highest point and the lowest point
after it instead. The shared campaign test harness is `tests/shooters_edge_campaign_fixture.hpp`
(the arena's door brush is "*1", x 128..144, y -64..64, z 0..128).
