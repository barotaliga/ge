---
title: GCC's -Wshadow flags a lambda's local that shadows an enclosing one; Apple Clang's precheck does not
date: 2026-11-05
scope: games/shooters_edge/labs/gpu_se_models_lab.cpp
concepts: [shadow, wshadow, gcc, clang, lambda, precheck, merge-queue, mac]
---

- `const int w = ...` inside a lambda in `gpu_se_models_lab.cpp` shadowed the lab's own `w`.
  `make precheck` on the Mac (Apple Clang, `-Wshadow`) passed; the merge queue's GCC row failed
  it under `-Werror` and ejected the PR. Clang reports it only with `-Wshadow-all` (its
  `-Wshadow-uncaptured-local`). Before pushing from the Mac, recompile a file whose lambdas grew
  with `-Wshadow-all -fsyntax-only` (`ninja -t commands labs | grep <file>` gives the line), or
  name a lambda's locals apart from the enclosing ones.
