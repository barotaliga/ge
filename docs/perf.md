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
| The piste's nearest-point lookup on a dense grid over the cells it touches (was a hash map), and both it and the network index walking each ring only where it crosses the occupied cells | 2156 ms → 1543 ms (−28%) | 280 ms → 257 ms (−8%) | `query.cpp`, `net_index.hpp` |

The lookups' ring walks visited every cell of every ring out to the search radius, most of
them empty: a query far from any run read up to 90 rings of hash-map misses. Clamping each ring
to the box the segments lie in skips those cells and reads the occupied ones in the same order,
so the segment that wins a tie is the same and no bit moves.
