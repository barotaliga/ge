# The performance ledger

Every speed-up lands a row here: what changed, the cost before and after, and how it was
measured. The budgets and the proofs a speed-up owes are the `perf` skill's. A row's numbers
are wall time on the machine named, so compare rows on one machine only.

## Map generation

Measured with `make lab LAB=mapgen-cost PRESET=release ARGS="--seeds=38,7,1 --reps=5
--version=<v>"`, the mean of each seed's best of five, in the 4-core Linux x86-64 container.
Every row kept every level digest (v10: `0b310d692fbf2dee`, v1: `4c49f555e1c4e253`).

| Change | v10 (ski area) | v1 (single piste) | Where |
| --- | --- | --- | --- |
| The x86-64 baseline raised to AVX2 (x86-64-v3), the compiler vectorising on its own | about −11% | | `ge_isa`, root `CMakeLists.txt` |
| The ski routes' search keeps each step's felled metres instead of walking the line through the trees on every visit | about −8% | | `plugins/mountain/mapgen/ski_routes.cpp` |
| The mountain's rows baked in bands on threads | about −12% | most of the v1 gain | `engine/core/parallel.hpp`, `massif.cpp`, `terrain.cpp` |
| All three, together | 2906 ms → 2154 ms (−26%) | 542 ms → 277 ms (−49%) | |

What is left, by callgrind's instruction counts on seed 38 (v10): the ski routes' two
searches (about a fifth now), the run walker's nearest-run lookups (18%), the analysis (9%) and
the piste's nearest-point lookup through its hash-map cells (8%), the next candidate for a dense
grid like the network index's.
