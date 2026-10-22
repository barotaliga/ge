---
name: perf
description: "Use for any optimisation, benchmark, profile or performance claim in gameengine2. The budgets, how to measure CPU and GPU cost, the proofs a speed-up owes (same bits, before/after), and the ledger."
---

# Performance

The engine exists to be fast (G2). Speed is proven by measurement.

## Budgets

- **Step**: well under 1 ms of one core for a race step at 120 Hz (the player plus
  three rivals).
- **Level build**: well under a second a seed on one core (`generate_level`, measured
  per seed), and never slower than the last release.
- **Frame**: `docs/rendering.md` § Measurement: the top of the picture ladder at 1440p
  in 8.3 ms of GPU on the reference desktop (M3 Max), the middle of it in 12 ms of GPU
  on a recent phone or tablet (thermal headroom under 60 Hz), and the renderer's CPU
  half under 1 ms.
  Timestamp queries per pass give the GPU numbers.
- **GPU claims** carry per-pass GPU ms, draws and instances before and after, from the
  frame's stats on a real GPU. The development container renders in software, so its GPU
  numbers say nothing about speed: a GPU timing claim is measured on the Mac through a
  `needs-mac` issue (`docs/rendering.md` § Where it is developed).

## Measuring

- CPU: `release` preset, the bench executable's median of N runs (N ≥ 20), on an idle
  machine, back to back before and after.
- Profiles: Tracy behind `GE_TRACY=ON` (added in M5). Zones per system in `step` and
  per pass in `prepare`. Without it, in the GPU-less container (no `perf` there), callgrind
  ranks where the instructions go: `valgrind --tool=callgrind build/release/labs mapgen-cost
  --seeds=38 --reps=1 --version=10`, then `callgrind_annotate --inclusive=yes`. Its counts
  rank; wall time from the bench lab proves.
- While porting, `porting/subsystems.md` § perf says how to time the reference on the
  same machine.

## The proofs a speed-up owes

1. **Same bits.** Every golden test is still green. An optimisation that changes a
   digest is a bug, not a trade-off.
2. **Before and after**, same machine, same build type, with the numbers in the PR.
3. **No allocation added** to a step or a frame. In debug, an allocation counter
   test asserts zero during `step`.

## Levers, in the order to try them

1. Algorithmic: a spatial hash instead of a scan, bake once instead of per step
   (the level's compile step), early-out on the nearest ring first (the heightfield's
   block offsets).
2. Memory: SoA for hot arrays, contiguous storage, no pointer chasing, reserve up
   front.
3. Parallelism: only where the result does not depend on scheduling (per-pass
   `prepare`, independent rows of a bake: `core::parallel_bands`). Never inside `step`.
4. SIMD: only with operations that are exact (add, mul, sqrt, compares, with no FMA). The
   x86 build already targets AVX2 (`ge_isa`, the `build` skill), so the compiler vectorises
   plain loops first; intrinsics only where a profile still points after that.

Hand-written assembly is not used: intrinsics give the same instructions, inline, in one
source for the agent to read, and assembly would need an x86 and an ARM copy.

The ledger is `docs/perf.md`: add a row for every speed-up (what changed, before and after,
how measured).

## Labs

A cost claim is a `bench` lab run before and after: `make lab LAB=<bench> PRESET=release
ARGS=--tag=before`, then `--compare=before`. Wall-clock metrics are `Det::measured` (compared,
never hashed), and the digest still holds the work's OUTPUT, so a speed-up that changed what
was built shows as a digest move. `mapgen-cost` is the worked example.
