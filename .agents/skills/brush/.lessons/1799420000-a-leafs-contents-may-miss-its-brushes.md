---
title: A leaf's stored contents may miss its brushes; the extended mode ORs them in
date: 2026-11-23
scope: plugins/brush/trace.cpp
concepts: [trace, leaf-contents, extended-contents, all-solid, extended-limits]
---

Our map writer stores each leaf's contents as the OR of the brushes in it, so a trace's leaf test
(skip a leaf whose contents miss the mask) never hides a brush on our own maps. A compiled map
may write a solid leaf as plain solid and leave out the clip or ladder brushes inside it, so a
player-clip trace walks past them. `BrushTracer::set_extended(true)` (a Quake II game with the
protocol extensions) ORs every leaf's brushes into its value (leaf 0 keeps its own) for the walk
and `contents`, and stops a sweep that starts and ends inside one brush at fraction 0 with that
brush's contents. To test it on a written map, clear the bit from the leaves holding the brush
after reading: a single leaf is not enough, a box spans several.
