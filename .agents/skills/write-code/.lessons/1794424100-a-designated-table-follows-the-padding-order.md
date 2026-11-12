---
title: A table built with designated initializers takes its order from the struct, and lint picks that order
date: 2026-11-12
scope: games/shooters_edge/server/sequel_monster_kinds.hpp, .clang-tidy
concepts: [designated-initializers, padding, clang-analyzer-optin-performance-padding, struct-layout, data-table]
---

A data table written with C++20 designated initializers (`{.classname = ..., .health = ...}`)
must name its fields in the struct's declaration order, and lint's padding check
(`clang-analyzer-optin.performance.Padding`) fails a struct whose order wastes bytes, naming the
order it wants. Settle the struct's order first (pointers, then ints and floats, then vectors and
arrays, then the bools and small enums) and write the rows in it; reordering a long table after
the fact means moving every row's fields. A comment line inside a row breaks scripts that move
fields by line: keep row comments at the end of a value's line.
