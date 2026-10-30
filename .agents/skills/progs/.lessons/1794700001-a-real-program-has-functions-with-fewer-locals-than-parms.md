---
title: A real program has functions whose locals are fewer than their parameters
date: 2026-10-30
scope: plugins/progs/file.cpp
concepts: [loader, locals, parameters, checker, single-player, layout]
---

Our compiler always counts a function's parameters inside its locals, so the checker refused any
function whose `locals` was smaller than its parameters' words. The shareware pack's
single-player program has a function whose `locals` is 0 and whose parameter takes 1 word, so the
checker turned down a valid program. Only a negative `locals` is malformed. The range check now
uses `max(locals, parm words)` inside the globals. Before you tighten a loader check, run it
against a real pack's program (lab `se-campaign --game=<zip>`), never only against our own
compiler's output.
