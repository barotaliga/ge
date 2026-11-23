---
type: Changed
title: Every classic game hides its HUD with scr_draw2d and draws timegraph
---

`scr_draw2d` and the frame-time graph (`timegraph`, `debuggraph`, `graphheight`, `graphscale`, `graphshift`) are now every classic game's, not Quake II's alone, and keep their values when you load another game. In Quake and QuakeWorld, `scr_draw2d 0` hides everything over the view (for screenshots), 1 keeps only the crosshair, and 3 hides the scoreboard; Quake's `r_timegraph` turns on the same graph along the bottom of the screen.
