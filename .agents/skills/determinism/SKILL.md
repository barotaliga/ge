---
name: determinism
description: "Use for anything in engine/core, any floating-point, libm or RNG question, and any parity failure. The bit-exact contract with the TypeScript oracle: the FP flags, the libm rule, the JS semantics helpers, and how to find the first diverging bit."
---

# Determinism

The contract is **bits**: for the same seed and the same inputs, the C++ simulation
produces the oracle's exact IEEE 754 doubles (`docs/goals.md` G1). Being close counts as
a failure.

## Why this is hard (read `docs/research.md` §2)

- `+ − × ÷ √` are correctly rounded everywhere, so they port exactly **if** the
  compiler does not fuse `a*b+c` into an FMA or reassociate.
- Transcendentals are not. glibc differs from V8 in 3–18% of calls (measured over
  200k inputs), Apple's libm and MSVC differ again, and V8 itself changed from fdlibm
  to LLVM libc between versions.

## The rules

1. **Flags**: `-ffp-contract=off -fno-fast-math` (GCC `-fexcess-precision=standard`,
   MSVC `/fp:strict`), set on `ge_flags` in `CMakeLists.txt`. Every target links
   `ge_flags`. Never add `-ffast-math`, `-Ofast` or `-march=native` to a sim target.
2. **libm**: simulation code calls `ge::core::libm::*` (M1) for sin, cos, tan, atan,
   atan2, asin, acos, exp, expm1, log, log2, pow, cbrt and tanh. It may use `<cmath>`
   only for exact functions: `sqrt`, `fabs`, `floor`, `ceil`, `trunc` and `fmod`.
3. **JS semantics**: `Math.round`, `|0`, `>>>0`, `Math.max`/`min` with NaN or −0, and
   `Math.sign` all go through `ge::core::js::*` (M1). See `docs/porting.md`'s list of
   traps.
4. **Randomness**: only `ge::core::Rng` (`engine/core/prng.hpp`), owned by the state.
   Draw in exactly the oracle's order. Hoist draws out of function arguments, because
   C++ does not define their evaluation order.
5. **Order**: no `unordered_map` or `unordered_set` iteration feeds a result. Sort with
   `std::stable_sort`.
6. **No clock and no thread-timing dependence** in the simulation.

## Choosing the libm (M1)

1. Write golden tables of every transcendental the sim uses under **game5's pinned
   Node** (`../game5/.nvmrc`), recording the Node and V8 versions in the file.
2. Compare them with candidate implementations: an fdlibm port (V8 `ieee754.cc` before
   the LLVM libc switch) and LLVM libc's correctly rounded routines (CORE-MATH
   lineage).
3. Adopt the candidate that matches on 100% of rows, and vendor it into
   `engine/core/libm/` with its licence header. Record which one it is and why in this
   skill.
4. `tanh` is `std::tanh` in current V8 (the platform's), so it needs its own answer
   from the measurement.

## A parity failure

1. Read the failing row: the inputs and the got/want bits. A difference of one ULP
   means a contraction or a libm call. A large difference means a logic or ordering
   bug.
2. Check that the target links `ge_flags`.
3. Bisect: dump intermediate values as hex from both sides (add a temporary print to
   a copy of the oracle under `tools/parity/`, never to the oracle repo).
4. Fix the cause. Never loosen `CHECK_BITS` to a tolerance.
