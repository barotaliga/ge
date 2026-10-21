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
2. **libm**: simulation code calls `ge::core::libm::*` for sin, cos, tan, atan,
   atan2, asin, acos, exp, expm1, log, log2, pow, cbrt and tanh. It may use `<cmath>`
   only for exact functions: `sqrt`, `fabs`, `floor`, `ceil`, `trunc` and `fmod`.
3. **JS semantics**: `Math.round`, `|0`, `>>>0`, `Math.max`/`min` with NaN or −0, and
   `Math.sign` all go through `ge::core::js::*` (`engine/core/js.hpp`). See `docs/porting.md`'s list of
   traps.
4. **Randomness**: only `ge::core::Rng` (`engine/core/prng.hpp`), owned by the state.
   Draw in exactly the oracle's order. Hoist draws out of function arguments, because
   C++ does not define their evaluation order.
5. **Order**: no `unordered_map` or `unordered_set` iteration feeds a result. Sort with
   `std::stable_sort`.
6. **No clock and no thread-timing dependence** in the simulation.

## The libm (M1, settled)

`engine/core/libm/libm.hpp` is the API; the measurement is `docs/research.md` §2.

- **sin, cos, tan, asin, acos, atan, atan2, exp, expm1, log, log2, cbrt, tanh** are V8
  13.6's fdlibm port (`src/base/ieee754.cc`), vendored line for line into
  `fdlibm_trig.cpp`, `fdlibm_arc.cpp` and `fdlibm_exp_log.cpp`. Node 24 does not build
  V8 with its glibc trig, so this is what `Math.sin` runs. Do not reformat or "improve"
  these files: diffing them against upstream is how a drift is found.
- **pow** is not fdlibm. Node 24 runs `Math.pow` on the host `std::pow` (V8's
  `--use-std-math-pow`, after the `y == 2` and `y == 0.5` shortcuts), so the reference is
  glibc's pow on game5's Linux x86-64 CI: Arm optimized-routines pow, which glibc
  compiles with FMA contraction. `pow.cpp` writes each contraction as `std::fma`. Adding
  or removing one changes about 1 result in 2,400.
- A correctly rounded libm (LLVM libc, CORE-MATH) misses Node 24 in 0.1–7% of calls, so it
  is not an option while game5 pins Node 24.
- **When game5's `.nvmrc` moves**, regenerate the tables with `make golden-libm
  NODE=<that node>` on Linux x86-64. If they change, find what the new V8 runs
  (`src/base/ieee754.cc`, the `use_std_math_pow` and `use_libm_trig_functions` flags,
  and Node's `tools/v8_gypfiles/features.gypi`), vendor that, and say so in the PR.
- A function game5 starts calling that is not in `libm.hpp` is ported from the same
  upstream into the same files, with its golden table, before the code that calls it.
- `tools/check-libm.sh` fails any `<cmath>` transcendental in simulation code.

## A parity failure

1. Read the failing row: the inputs and the got/want bits. A difference of one ULP
   means a contraction or a libm call. A large difference means a logic or ordering
   bug.
2. Check that the target links `ge_flags`.
3. Bisect: dump intermediate values as hex from both sides (add a temporary print to
   a copy of the oracle under `tools/parity/`, never to the oracle repo).
4. Fix the cause. Never loosen `CHECK_BITS` to a tolerance.
