---
title: Rules two programs share compile once per declaration; a side effect in a condition is hoisted
date: 2026-11-13
scope: games/shooters_edge/rules/, tests/shooters_edge_campaign_rules_test.cpp, tests/shooters_edge_campaign_fixture.hpp
concepts: [native-campaign, variant, shared-rules, parity, side-by-side, short-circuit, th-check]
---

The arena and the campaign share `common_*.cpp`, but each program numbers its globals, fields,
strings and functions its own way. So the shared files include `variant.hpp`, which picks the
declaration by `SE_RULES_CAMPAIGN`, and CMake compiles them a second time into an OBJECT library
with it defined, in another namespace; a hook only one variant has (`items_come_back`) is a call
each variant's glue answers. The parity traps were the same as the arena's, and more of them:
QC's `&&` and `||` run both sides, so a condition whose right side traces (`can_see`), draws a
random number, calls makevectors (`in_front`) or rewrites `.chain` (`findradius` in a spawn check)
is hoisted into a temporary before the test, left to right. A float times a vector is `fv`
(MUL_FV) and a dot product sums x and y before z. A th_check that answers returns through
`set_return_float`, read back by the caller. Tests that reach a builtin by name now reach it by
number (`g.builtin(3, ...)`): a native program has no builtin definitions to look up.
