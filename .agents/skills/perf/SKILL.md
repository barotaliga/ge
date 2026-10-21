---
name: perf
description: "Use for any optimisation, benchmark, profile or performance claim in gameengine2. The budgets, how to measure CPU and GPU cost, the proofs a speed-up owes (same bits, before/after), and the ledger."
---

# Performance

The engine exists to be fast (G2). Speed is proven by measurement.

## Budgets

- **Step**: well under 1 ms of one core for a race step at 120 Hz (the player plus
  three rivals).
- **Level build**: faster than game5's `generateLevel` on the same machine, measured
  per seed.
- **Frame**: the display's refresh on the reference machines with the picture at its
  top. Timestamp queries per pass give the GPU numbers.

## Measuring

- CPU: `release` preset, the bench executable's median of N runs (N ≥ 20), on an idle
  machine, back to back before and after.
- Profiles: Tracy behind `GE_TRACY=ON` (added in M5). Zones per system in `step` and
  per pass in `prepare`.
- Compare with game5: run game5's `make cpu-cost` or `make sim` in `../game5` on the
  same machine for the reference number.

## The proofs a speed-up owes

1. **Same bits.** Every parity test is still green. An optimisation that changes a
   digest is a bug, not a trade-off.
2. **Before and after**, same machine, same build type, with the numbers in the PR.
3. **No allocation added** to a step or a frame. In debug, an allocation counter
   test asserts zero during `step`.

## Levers, in the order to try them

1. Algorithmic: a spatial hash instead of a scan, bake once instead of per step
   (game5's compile step), early-out on the nearest ring first (the framework's
   `blockOffsets`).
2. Memory: SoA for hot arrays, contiguous storage, no pointer chasing, reserve up
   front.
3. Parallelism: only where the result does not depend on scheduling (per-pass
   `prepare`, independent rows of a bake). Never inside `step`.
4. SIMD: only with operations that are exact (add, mul, sqrt, compares, with no FMA).

When a perf ledger exists (`docs/perf.md`), add a row: what changed, before and after,
and how.
