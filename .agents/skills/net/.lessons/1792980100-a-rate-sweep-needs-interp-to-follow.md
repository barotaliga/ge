---
title: A lower update rate needs the interpolation delay to follow it
date: 2026-10-23
scope: engine/net/replicate_client.cpp, engine/cli/net.cpp
concepts: [replicate, update-rate, interp, interpolation, sweep, rewind]
---

`ge net sweep --knob=update-rate` showed the rewind gap's p95 rising from zero to 6 cm at 10
snapshots a second while the view delay stayed at 120 ms. `interp_ticks` (6 ticks, 100 ms at
60 Hz) is shorter than the gap between two snapshots at 10 a second, so the render tick
outruns the newest snapshot and the client holds the last one. A client that asks for fewer
snapshots must draw further behind (interp_ticks at least the snapshot interval, two to ride out
a lost one); sweep `interp` beside `update-rate` before calling a rate cheap.
