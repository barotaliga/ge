---
title: A live source's spectators are the delay plus one block behind
date: 2026-11-15
scope: engine/server/spectators.cpp, engine/tv/node.cpp, engine/lab/labs/server_record_lab.cpp
concepts: [tv-delay, anchor, server-tv, spectators, lag, late-joiner]
---

A TV node anchors the source's clock when the first block comes whole, and a block is whole only
after its second has passed, so a server feeding its node live puts every block out the delay
plus one second after the block opened (3.0 s minimum for a 2 s delay in the `server-record`
lab's spectators table), where a file read out at speed shows the delay alone. Measure a live
spectator's lag from the block's start and expect `delay + block`; the far outliers are a late
joiner catching up from its keyframe, not a slow link. A late joiner starts at the newest keyframe
already OUT (the delay behind), not the newest written: at 9 s with keyframes every 4 s and a 2 s
delay it starts at block 4, not 8. Give the net node every link (32) and let the TV node refuse a
viewer past its cap, or the extra one is dropped by the net layer with no reason to read.
