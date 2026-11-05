---
title: MSVC rejects an int ternary stored in a float (C4244 under /WX) that GCC and Clang pass
date: 2026-11-05
scope: plugins/brush/app/bsp_draw.cpp, .github/workflows/ci.yml
concepts: [msvc, c4244, int-to-float, ternary, windows-row, merge-queue, wconversion]
---

`info[2] = flag ? 1 : 0;` with `info` a `float[4]` passes GCC's and Clang's `-Wconversion`
(the ternary's two constants fit a float exactly), so `make precheck` is green, but MSVC warns
C4244 ("conversion from 'int' to 'float'") and `/WX` makes it the Windows row's build error.
Every PR in the queue then fails on Windows until the line is fixed. Write the arms as the
destination's type: `flag ? 1.0f : 0.0f`. The container has no MSVC, so a float stored from an
int expression (a ternary, a `bool`, a count) gets an explicit `float(...)` or float literals.
