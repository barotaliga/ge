---
type: Changed
title: AVX2 required on x86-64, and map generation about a quarter faster
---

The engine now builds x86-64 targets for x86-64-v3 (AVX2), carried to consumers as `ge::isa` through `ge::core`; a ski area builds in about three quarters of the time and a single piste in about half, through threaded height bakes (`core/parallel.hpp`) and a cheaper route search, with every map's bits unchanged. `ge::core` now links `Threads::Threads`.
