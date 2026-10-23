---
title: A matrix of runs off one seed repeats one unlucky draw down every cell
date: 2026-10-23
scope: engine/lab/labs/net_reach_lab.cpp, engine/net/reach.cpp
concepts: [lab, seed, loss, rendezvous, unknown-session]
---

The first net-reach run failed all 25 pairs on wifi and bad with `unknown-session`: each pair
built its network off the same seed, so the host's first two registers were lost in every cell
alike (a burst on a fresh link), and the client gave up on the server's first `unknown`. Two
fixes: a client keeps asking past `unknown` until give-up (the host's register may be on its
way), and the lab seeds each cell apart (`seed * 31 + cell`). A lab that runs a grid of
scenarios should give each cell its own draws, or one bad draw reads as a systematic failure.
