---
title: A Mac's precheck misses GCC-only errors; run GCC 13 in Docker over the reached files before queueing
date: 2026-11-20
scope: games/shooters_edge/labs/se_people_lab.cpp, games/shooters_edge/labs/se_team_lab.cpp
concepts: [gcc, shadow, lambda, dangling-reference, merge-queue, linux-gcc, apple-clang, precheck-on-mac, docker]
---

A precheck on the Mac (Apple Clang) passed, and the merge queue's `linux-gcc` row ejected the PR:
`const auto share = [](int64_t k, int64_t n)` inside a function with a local `n` is
`-Werror=shadow` under GCC, silent under Clang's `-Wshadow`. Before a push from a Mac, compile
the changed files with Clang's `-Wshadow-uncaptured-local` (the same check) off
`build/release/compile_commands.json`, `-fsyntax-only`, and rename what it names
(`(int64_t part, int64_t whole)`).

The next queue round ejected it again on GCC 13's `-Wdangling-reference` (a test holding
`const NavField& f = nav_field(fs, 4, {})`: a reference returned from a call given a temporary).
Each ejection costs a queue round (~25 minutes). What found every one at once: `docker run gcc:13`
with the tree mounted at its own path, each reached file's line from
`build/release/compile_commands.json` with the Apple flags (`-arch`, `-isysroot`, `-o`, `-MF`, `-MT`,
`-MD`) dropped, compiled at -O3 to `/dev/null`, twelve at a time: 540 files in a few minutes.
