---
title: A demo series' next recording swaps the demo, not the run
date: 2026-11-28
scope: games/shooters_edge/client/timedemo.cpp, games/shooters_edge/client/demo.cpp, games/shooters_edge/client/demo_playback.cpp
concepts: [demo-series, geds, timedemo, snapshots, seek, new-level, entry-begun]
---

Shooter's Edge's demo player keeps snapshots that lend `t.demo.bytes` back rather than copy them,
so a snapshot of one recording restored over the next one's bytes reads garbage. When
`timedemo_step` opens a series' next recording it must clear `t.snaps`, make the new recording
`t.start`, keep the timeline where it began (`t.entry_timeline`, which a seek back to the start
lands on) and set `t.levels = 0` so the new recording's first level reads as `new_level` and the
window loads its map. Keep the series in a `shared_ptr<const DemoSeries>` on the `Demo`: every
snapshot copies the demo, and copying the entries' bytes there would copy megabytes a frame. A
replay file inside a series is held byte for byte, so its tests hold the series' sum only over
stored tracks: a zstd track's bytes belong to the codec's version, not to the format.
