---
title: A nav link proven by "the move got there" let jumps walk and drops stall until the proof said what a link is
date: 2026-10-29
scope: plugins/shooter/nav.cpp, games/shooters_edge/labs/se_nav_lab.cpp
concepts: [nav, proof, prove-link, jump, drop, stall, candidates, cost, se-nav]
---

The first `se-nav` picture was orange from wall to wall: a "jump" proof pressed jump once and kept
pursuing, so it landed, walked to the stairs and up them, and counted as a jump to the platform
(11 304 links, 36 082 proofs, a minute of debug build for 449 nodes). What fixed it was stating
what each kind IS inside the proof: a jump is one flight (grounded anywhere short of the target is
a refusal), and the "no nearer for a quarter second" stall counts only on the ground, because a
player walking off a ledge overshoots in the air and comes back (that rule had left 25 drops where
there are 182). Then the candidates: a jump is tried only where the first pass's walks do not
already get there in 1.5 times a straight run, and a gap must be open at the higher end's height
(a wall is not a gap). 5 232 proofs, 13 s. Look at the lab's picture before tuning a count: every
one of these was a coloured line in the wrong place.
