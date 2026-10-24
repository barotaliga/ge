---
title: Floating-point from_chars does not build on Apple's standard library, so text numbers go through strtod
date: 2026-10-24
scope: engine/charset/design.cpp
concepts: [charset, parse, from-chars, strtod, macos, portability]
---

The design language's numbers were read with `std::from_chars(..., double&)`. That builds with
GCC and on Linux, and precheck and every PR row passed. Only the merge queue's macOS row
(Apple Clang) failed, with "call to deleted function 'from_chars'", because Apple's libc++ has no
floating-point overload. Read a decimal token with `std::strtod` on a copied string, as
`engine/console/cvar.cpp` does. It rounds correctly on every C library, and the engine never
changes the locale. Integer `from_chars` is fine everywhere.
