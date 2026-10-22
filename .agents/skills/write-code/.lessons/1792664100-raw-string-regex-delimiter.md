---
title: A raw-string regex that contains )" ends early, so give it a delimiter
date: 2026-10-21
scope: engine/cli
concepts: [regex, raw-string, compile-error]
---

`R"(set\(X\s+"?([0-9.]+)"?\))"` stops at the first `)"` inside it (after the capture group),
and the compiler reports a cascade of "stray '\' in program" errors far from the cause. Any
pattern that matches a quote after a group needs a delimiter: `R"re(...)re"`. Patterns split
across adjacent raw pieces (`R"(a|)" R"(b)"`) are fine, since each piece is closed on purpose.
