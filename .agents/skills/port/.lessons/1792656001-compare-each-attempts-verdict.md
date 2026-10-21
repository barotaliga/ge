---
title: Check a carried-over generator stage by each attempt's verdict, not only by its digest
date: 2026-10-21
scope: plugins/mountain/mapgen
concepts: [resort-build, attempt, refusal, verdict, parity]
---

With bit parity no longer asked for, a ski-area stage carried over from game5 has no golden to
match. A cheap check that still catches a wrong port: run game5's `attemptResort` for the same
seeds and attempts (after `npm install` in the game5 clone, call it from a scratch `.mjs` under
`node --experimental-strip-types`) and compare each attempt's verdict with the engine's stage
run: refused or not, and the refusal's text. For the access stage (M4b3b) all twelve test
attempts gave the same verdict and the same reason ("R26: D1 stands on a run wherever its station
is moved", and so on). That showed that the half of the attempts that are refused are refused by
design, and not because of a bug in the port.
