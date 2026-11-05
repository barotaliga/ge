---
title: A worktree missed every cached compile: debug builds hash the folder, deps were per checkout, the cache was full
date: 2026-11-05
scope: cmake/CompilerCache.cmake
concepts: [ccache, worktree, hash-dir, debug-prefix-map, max-size, deps, deps-dir]
---

Three things made a new git worktree build cold. `-g` compiles hash the working directory unless
`hash_dir=false`; with it, `-fdebug-prefix-map=<build>=.` keeps the debug information right (its
compile dir is `.`), and ccache still hits across checkouts (tried on two scratch trees: 0 of 2
without, 1 of 2 with). `.cache/deps/` was the checkout's own, so Dawn and the codecs downloaded
again: CompilerCache.cmake now points a worktree at the main checkout's (`git rev-parse
--git-common-dir`), and drops cached find results under the old folder, or the build asks for a
library that is no longer there. And ccache's default 5 GiB was full with 2016 cleanups: a few
checkouts' debug and release objects evict each other, so a worktree raises it while it is the
default. Changing the launcher's flags misses once for everything; the next checkout hits.
