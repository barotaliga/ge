---
type: Added
title: The development graph along the bottom of the screen
---

`engine/hud/bottom_graph.hpp` draws samples as columns across the bottom of a game's 2D screen,
newest at the right, scaled and held to a height. Quake II's `timegraph`, `debuggraph`,
`graphheight`, `graphscale` and `graphshift` draw each frame's time with it.
