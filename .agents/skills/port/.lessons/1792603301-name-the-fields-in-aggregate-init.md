---
title: Positional aggregate init of a struct of doubles swaps fields silently
date: 2026-10-21
scope: plugins/mountain/mapgen/forest.cpp
concepts: [aggregate-init, struct, field-order, parity]
---

A TypeScript object literal names its fields (`{ radius, crown, age }`), so its order means
nothing; a C++ `TreeDef{x, z, y, height, radius, crown, age, ...}` fills the struct's members in
DECLARATION order, and with every field a `double` the compiler cannot tell that `crown` went
into `age`. M4a's first build refused every map with "R14 a crown 146.0 m wide" for exactly
that. When a port builds a struct from an oracle object literal, write the members in the
struct's own order and check it against the header, or use designated initializers
(`TreeDef{.x = x, ...}`, C++20) where the struct allows them.
