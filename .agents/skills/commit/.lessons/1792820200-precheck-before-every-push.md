---
title: make precheck before every push; its gates are the ones that cost CI rounds
date: 2026-10-22
scope: tools/precheck.sh, Makefile
concepts: [precheck, ci, fmt-check, agents-check, lint, push]
---

The CI rounds threads lost were almost all to the cheap gates: a file not formatted, an
uppercase `ge:keywords` word, a clang-tidy finding in a changed file, a message off the
diagnostics convention. `make precheck` runs exactly those, in CI's order, in about 40 s on a
warm build, so run it after the last edit and before every push, even for a docs-only change
(agents-check reads the docs too). Each gate's output goes to `build/<preset>/precheck.log`
and only a red gate prints it.
