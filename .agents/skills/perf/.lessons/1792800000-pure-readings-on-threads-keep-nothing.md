---
title: Put a pure reading on threads only once it keeps nothing between calls
date: 2026-10-22
scope: plugins/mountain/mapgen
concepts: [parallel, parallel-bands, threads, race, nethit, closure, digest, tsan, determinism]
---

The cheapest way to take wall time off a bake that is already bit-exact is to read its pure
candidates side by side and then take their answers in the old order (`parallel_bands` over the
candidates, a sequential pass after). Before doing it, read every closure the reading calls: the
stations' and the woods' readings each answered into one `NetHit` captured by reference, which is
harmless on one thread and a race on several. It showed only as a level digest that changed from
one run to the next. Give each call its own answer, build anything lazy (the piste's lookup)
before the threads start, and run the mountain tests once under `-fsanitize=thread`
(`-Wno-maybe-uninitialized`: GCC's warning trips on the sanitizer build). A heap search with lazy
deletion should also skip an entry popped at a dearer cost than its square holds: with weights
that are never negative, processing it again changes nothing, and on a ski area's routes that was
half of all the pops.
