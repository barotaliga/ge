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
| The ski routes: a heap entry left at a dearer cost than its square was reached at is passed over, the felled metres of every downhill step worked once on threads, the two searches run side by side, the candidate lines judged on threads (only each top's best laid again), a line refused for its length before it is laid on the snow, headings worked only for a line kept | about −17% | | `ski_routes.cpp` |
| The network index's cells hold their segments as spans of a run's consecutive ones with a box, a span too far off passed over whole; a ring walks a row only across the columns it holds segments in (a column only across its rows) | about −4% | | `net_index.hpp` |
| Readings that are pure run on threads, the answers taken in their old order: a drag lift's candidate places, the woods' scan (its draws taken first, in order), a run's nearest-segment pass over the grid by bands of rows; the walk's candidate headings and their sines worked once | about −20% | about −10% | `station_clear.cpp`, `forest.cpp`, `network_stamp.cpp`, `network.cpp` |
| All of the above, together | 1548 ms → 932 ms (−40%) | 257 ms → 224 ms (−13%) | |
| Two library calls the profile hid (they run few instructions but many cycles): a remainder whose dividend is already inside the divisor returns it (`num::mod`, every angle difference), and a wrap to 32 bits within int64's range is an integer cast (`num::to_int32`, every noise hash); the walk's heading choice weighs the heading that turns least first and passes over any whose turn alone already costs more | about −8% | | `engine/core/num.hpp`, `network.cpp` |
| `hypot` divides only the smaller side (the larger over itself is exactly 1); a power-of-two grid pitch multiplies by its exact inverse | about −2% | | `engine/core/math.hpp`, `heightfield.hpp` |
| More pure per-cell passes on threads: a run's levelling of the grid, a lane's raster; the ski routes' trunks filed once a map and the kept lines threaded through the woods side by side | about −7% | | `network_stamp.cpp`, `lanes.cpp`, `ski_routes.cpp` |
| These three, together, against the last row, side by side on one machine | 948 ms → 788 ms (−17%) | 242 ms → 223 ms (−8%) | |

The lookups' ring walks visited every cell of every ring out to the search radius, most of
them empty: a query far from any run read up to 90 rings of hash-map misses. Clamping each ring
to the box the segments lie in skips those cells and reads the occupied ones in the same order,
so the segment that wins a tie is the same and no bit moves.

A reading run on threads must keep nothing between calls. The station and woods readings each
wrote their answer into one `NetHit` the closure shared, harmless on one thread; on several the
writes raced and the maps moved (the digest changed from one run to the next). Each call now
answers into its own. A thread-sanitizer build of `mountain_resort_level_test`,
`mountain_ski_routes_test` and `core_parallel_test` reports no race.

The instruction counts (callgrind) missed the two library calls: `fmod` runs a handful of
instructions that take tens of cycles, so its cost shows only in wall time. A sampling profile
(SIGPROF over the build, the samples symbolised with `addr2line`) found it, and the A/B runs of
the two builds back to back, three times over, are how the rows above were measured: the
container's timings wander by ±5% from one run to the next.

