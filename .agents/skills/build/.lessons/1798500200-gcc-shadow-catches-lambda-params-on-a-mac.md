---
title: GCC's -Wshadow flags a lambda's parameter that shadows an outer local; Apple Clang does not unless asked
date: 2026-11-20
scope: games/shooters_edge/labs/se_people_lab.cpp, games/shooters_edge/labs/se_team_lab.cpp
concepts: [gcc, shadow, lambda, merge-queue, linux-gcc, apple-clang, precheck-on-mac]
---

A precheck on the Mac (Apple Clang) passed, and the merge queue's `linux-gcc` row ejected the PR:
`const auto share = [](int64_t k, int64_t n)` inside a function with a local `n` is
`-Werror=shadow` under GCC, silent under Clang's `-Wshadow`. Before a push from a Mac, compile
the changed files with Clang's `-Wshadow-uncaptured-local` (the same check) off
`build/release/compile_commands.json`, `-fsyntax-only`, and rename what it names
(`(int64_t part, int64_t whole)`).
