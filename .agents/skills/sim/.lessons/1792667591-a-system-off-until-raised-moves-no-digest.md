---
title: A step system that stays inactive and unhashed until something raises it moves no golden
date: 2026-10-22
scope: games/fall_line/sim/game.cpp, games/fall_line/sim/wanted.cpp
concepts: [step-order, digest, golden, wanted, new-system]
---

The wanted level runs inside every `step` but returns at once while `GameState::wanted.active`
is false, and `wanted_hash` folds nothing in then, so adding it rewrote no golden
(`fall_line_sim`, `fall_line_field`, `fall_line_play` and every lab digest held). Land a new
system the same way when the runs that exist never trigger it: an `active` flag, the hash gated
on it, its buffers reserved at create. A new `EventKind` fails the build (`-Wswitch` under
`-Werror`) in `game.cpp`'s `event_name` and `log_step` and in `sound/route.cpp`: handle it in all
three, even if only to say the sound comes later.
