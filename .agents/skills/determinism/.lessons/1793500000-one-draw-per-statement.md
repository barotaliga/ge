---
title: One random draw per statement; a call's arguments run in the compiler's order
date: 2026-10-28
scope: engine/net/stress_fuzz.cpp
concepts: [prng, rng, draw-order, unspecified-order, arguments, operands, cross-compiler, digest]
---

`(word(rng) << 32) | word(rng)` and `f(int_in(rng, ...), int_in(rng, ...))` draw twice from one
stream in an order C++ leaves unspecified: GCC on x86-64 and MSVC took the right draw first, Clang
and GCC on arm64 the left, so the fuzz lab's digest split by compiler and only the queue's
release rows saw it (the pull request runs GCC and MSVC, which agreed). Put each draw in its own
`const` in the order wanted, then use them. `||`, `&&`, `?:`, `,` and a braced list are sequenced
and safe. A digest that differs between Clang and GCC with contraction off is this first.
Labs that hold the whole fixed-step loop, including traffic and capacity sums like a soak's
held bytes, owe the same check: a buffer's capacity is the library's, so `Det::measured`.
