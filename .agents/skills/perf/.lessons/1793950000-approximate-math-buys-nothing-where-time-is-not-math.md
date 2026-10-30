---
title: Approximate math buys nothing where the time is not in the math; skip the call instead
date: 2026-10-30
scope: plugins/mountain/mapgen
concepts: [approximation, polynomial, sine, cosine, hypot, rsqrt, lookup-table, early-out, a-b, digest]
---

Per call, a polynomial sine is 2–3x cheaper than the vendored one, a bare `sqrt(a*a + b*b)` 5x
cheaper than `hypot`, `atan2` to 1e-4 rad 6x, and the old 0x5f3759df inverse square root 6x
cheaper than `1/sqrtf` (an AVX2 `vrsqrtps` with one Newton step is cheaper still and a thousand
times more accurate: the trick is obsolete, and the GPU has `inverseSqrt` anyway). Built into
the map generator they bought nothing within the container's ±5% (A/B, three rounds), and every
one of them moved maps. The generator's time is in walks over grids and indexes; libm is 5% of
its instructions. What did pay was not calling the sine and cosine at all where a cheap bound
already knew the answer (a box's extents bound the distance): 5.1% fewer instructions, −7% wall,
the same bits. A 256-entry table for an sRGB byte's linear value is exact and 30x cheaper per
call, but no lab spent measurable time there, so it was left out. Rank the cost first
(callgrind for instructions, A/B for wall), then look for a call that can be skipped before
one that can be made cheaper.
