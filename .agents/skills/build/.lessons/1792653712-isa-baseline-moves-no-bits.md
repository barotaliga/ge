---
title: Raising the x86 baseline to AVX2 moved no bits because contraction stays off
date: 2026-10-22
scope: CMakeLists.txt
concepts: [isa, avx2, march, fma, golden]
---

`-march=x86-64-v3` lets the compiler emit FMA instructions, but `ge_fp`'s `-ffp-contract=off`
(MSVC `/fp:strict`) means it never forms one from `a*b+c`, and the explicit `std::fma` calls in
`engine/core/libm/pow.cpp` are correctly rounded either way. Every golden, lab digest and level
digest held under GCC. A new ISA flag still owes the full suite on every CI row before it is
trusted, because the promise is per compiler.
