---
title: Stage an acro figure from a steady glide, never straight out of a full-throttle climb
date: 2026-10-23
scope: tests/fall_line_para_test.cpp, plugins/paramotor/scenarios.cpp
concepts: [acro, dive, loop, tangle, staging, script, test]
---

On seed 38 the free ride's test first climbed 40 s on full throttle and then dived at once. The
throttle cut swung the wing forward over him, so the 2 s dive began from a climb, never built its
speed, and the swing-up after it neither looped nor tangled. The same 2 s on the flat scenario
(started in a trimmed glide) tangled every time.

A figure depends on the dive's length AND on where the dive starts: the rears bite by the speed
he carries when he pulls. Shut the throttle and glide a few seconds first (4 s settles it), then
script the figure. After that the result held over glides of 4 to 8 s and dives of 2 to 2.5 s.
