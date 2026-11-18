---
title: Speed a demo up through its speed, never by scaling the frame's wall time
date: 2026-11-18
scope: games/shooters_edge/client/demo_playback.hpp
concepts: [demo, playback, speed, catch-up, kmostplaystep, fly-down, live-map]
---

`playdemo_step` caps the wall time one frame counts (`kMostPlayStep`, 0.1 s) before it multiplies
by the speed, so a catch-up done by passing `wall * rate` silently loses time whenever the frame
is long (a slow machine, a lab at 10 fps): the live map's fly stood 1.5 s behind in `se-fly`'s
smoke run. Pass a rate through the speed instead (`DemoPlayback::rate`, multiplied into
`cl_demospeed`'s), and hold a catch-up to a measured "behind after" in its lab.
