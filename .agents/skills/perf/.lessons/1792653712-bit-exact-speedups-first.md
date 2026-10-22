---
title: In map generation the bit-exact wins are algorithmic and threaded, not hand-written SIMD
date: 2026-10-22
scope: plugins/mountain/mapgen
concepts: [mapgen, profile, callgrind, simd, threads, memo]
---

A callgrind pass over a v10 build (`valgrind --tool=callgrind build/release/labs mapgen-cost
--seeds=38 --reps=1 --version=10`) showed most instructions in grid searches and spatial lookups,
not in the noise. The three changes that paid were a build flag (AVX2), a memo of a pure per-step
cost the Dijkstra recomputed on every relaxation, and running independent bake rows on threads:
26% together with every digest unchanged. An AVX2 value-noise prototype was bit-exact and 1.5x
faster than the cached scalar path, but noise is about a tenth of the build. Profile first; look
for a pure function called again with the same arguments before reaching for intrinsics.
