---
title: Anything that depends on a GPU-culled draw's order is measured, not hashed
date: 2026-10-21
scope: games/fall_line/labs
concepts: [digest, measured, indirect-draw, atomics, overdraw, cull]
---

The terrain's cull appends visible tiles with atomics, so the order of the instances in the
indirect draw changes from run to run. Overdraw with no prepass (`greater`, depth written) counts
how many fragments win in turn, which depends on that order: the `world` lab read 1.007 and
1.087 for the same view on two runs. Only order-free results (the count after the prepass, the
pyramid of the depth, a converged still) can be checked exactly; the rest goes in as
`Det::measured`.
