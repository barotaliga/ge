---
title: A scripted run drifts on after its keys let go; check where it stopped before reading its heights
date: 2026-10-30
scope: tests/shooters_edge_listen_test.cpp, games/shooters_edge/labs/gpu_se_client_lab.cpp
concepts: [rocket-jump, fixture, doorway, lintel, scripted-run, friction, golden]
---

The first rocket jump on the fixture halls rose 72 units against a jump's 44, and it looked like
a weak blast. The rows said otherwise: thirty frames of running took the player 110 units, he
slid on through the jump to x 508, under the east doorway, and its lintel at 128 stopped him
(his top is 32 over his origin). Walk fewer frames and stand still before the jump, and the same
blast lifts him 200 units to the hall's ceiling. Print every tenth frame's position before
suspecting the mod: a ceiling shows as the vertical speed going from +600 to −90 in a tenth of a
second, which gravity alone cannot do.
