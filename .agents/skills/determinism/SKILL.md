---
name: determinism
description: "Use for anything in engine/core, any floating-point, libm or RNG question, and any golden failure. The bit-exact contract: the FP flags, the libm rule, the number semantics helpers, and how to find the first diverging bit."
---

# Determinism

The contract is **bits**: for the same seed and the same inputs, the simulation produces
the same IEEE 754 doubles on every compiler and OS, and the golden tables in
`tests/golden/` hold them (`docs/goals.md` G1). Being close across platforms counts as a
failure. Matching another implementation's bits does not: the goldens are the engine's own,
and a deliberate behaviour change rewrites them in its PR (the `testing` skill).

## Why this is hard

- `+ − × ÷ √` are correctly rounded everywhere, so they give the same bits everywhere
  **if** the compiler does not fuse `a*b+c` into an FMA or reassociate.
- Transcendentals are not: glibc, Apple's libm and MSVC's each give different last bits
  for a few percent of inputs. So the engine carries its own.

## The rules

1. **Flags**: `-ffp-contract=off -fno-fast-math` (GCC `-fexcess-precision=standard`,
   MSVC `/fp:strict`), set on `ge_fp` in `CMakeLists.txt`. Every target links
   `ge_fp` (through `ge_core`), and a published package carries it as `ge::fp`. Never add `-ffast-math`, `-Ofast` or `-march=native` to a sim target.
2. **libm**: simulation code calls `ge::core::libm::*` for sin, cos, tan, atan,
   atan2, asin, acos, exp, expm1, log, log2, pow, cbrt and tanh. It may use `<cmath>`
   only for exact functions: `sqrt`, `fabs`, `floor`, `ceil`, `trunc` and `fmod`.
3. **Number semantics**: rounding (halves toward +inf), the int32/uint32 wrap, `imul`,
   `max`/`min` with NaN or −0, `sign` and `mod` all go through `ge::core::num::*`
   (`engine/core/num.hpp`), never a `<cmath>` or `<algorithm>` look-alike.
4. **Randomness**: only `ge::core::Rng` (`engine/core/prng.hpp`), owned by the state.
   The order of draws is part of the result: hoist draws out of function arguments,
   because C++ does not define their evaluation order.
5. **Order**: no `unordered_map` or `unordered_set` iteration feeds a result. Sort with
   `std::stable_sort`.
6. **No clock and no thread-timing dependence** in the simulation.

## The libm (M1, settled)

`engine/core/libm/libm.hpp` is the API, and `tests/golden/libm_*.txt` pin every
function's bits.

- **sin, cos, tan, asin, acos, atan, atan2, exp, expm1, log, log2, cbrt, tanh** are
  fdlibm, vendored line for line into `fdlibm_trig.cpp`, `fdlibm_arc.cpp` and
  `fdlibm_exp_log.cpp` (`LICENSE.md`). Do not reformat or "improve" these files: diffing
  them against upstream is how a drift is found.
- **pow** is glibc's (Arm optimized-routines) pow, in `pow.cpp`, with every FMA its
  reference build contracts written out as `std::fma`. Adding or removing one changes
  about 1 result in 2,400, and `tests/golden/libm_pow.txt` catches it.
- A function the simulation starts calling that is not in `libm.hpp` is vendored from the
  same upstream into the same files, with its golden table, before the code that calls it.
- Where the tables come from and when to regenerate them is the working area's
  (`porting/subsystems.md` § determinism).
- `tools/check-libm.sh` fails any `<cmath>` transcendental in simulation code.

## A golden failure

1. Read the failing row: the inputs and the got/want bits. A difference of one ULP
   means a contraction or a libm call. A large difference means a logic or ordering
   bug.
2. Check that the target links `ge_fp` (directly or through `ge_core`).
3. Bisect: dump intermediate values as hex and compare them with the golden's generator
   (while porting: `porting/README.md` § Bisecting).
4. Fix the cause. Never loosen `CHECK_BITS` to a tolerance.
