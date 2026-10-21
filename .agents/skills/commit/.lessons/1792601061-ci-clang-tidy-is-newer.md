---
title: CI's clang-tidy is newer than Ubuntu 24.04's and fails a floating-point for counter that a local lint passes
date: 2026-10-21
scope: .clang-tidy
concepts: [lint, clang-tidy, ci]
---

CI's `lint` job runs a newer clang-tidy than Ubuntu 24.04's (18), and its
`clang-analyzer-*` set includes `security.FloatLoopCounter`, which 18 leaves off. A
local `make lint` pass is not proof: a floating-point `for` counter (common when porting
a scan that accumulates `h += kStep`) fails CI. Write such scans as `while` loops.
