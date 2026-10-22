---
title: The first ge mem on Fall Line found the tape recorder allocating in the step
date: 2026-10-22
scope: games/fall_line/sim/simulate.cpp
concepts: [allocations, hot-path, tape, ge-mem, bench]
---

`play bench --seeds=38 --rivals=3` on the debug build: median step 33 us, `bot` 40% of it,
`skier` 23%, `hash` 15%, `course` 8%; 17 steps after the first second allocate, the first at
step 255, all in zone `tape` (the Driver's control-tape recorder growing its runs). Reserving
the recorder's storage for the run's longest length is the open fix; `ge mem` holds it.
