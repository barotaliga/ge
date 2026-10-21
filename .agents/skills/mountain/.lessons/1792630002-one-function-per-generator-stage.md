---
title: Build a long generator as an attempt struct and one function per stage
date: 2026-10-21
scope: plugins/mountain/mapgen/resort_build.cpp
concepts: [staged-function, attempt-struct, refusal, generator]
---

The ski area's build runs for hundreds of lines whose state flows from stage to stage. It
keeps that state in `ResortAttempt` and gives each stage its own function (`resort_mountain`,
`resort_pads`, `resort_pistes`, `resort_links`, …) returning `StageResult` (a refusal message
or nothing). The test runs the stages in order, hashes each section the attempt holds, and
names the first one that moves, so a new stage lands without touching the ones before it.
Walked runs live in a `std::deque` because the network index keeps pointers to their points.
