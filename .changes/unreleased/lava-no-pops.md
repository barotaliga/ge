---
type: Fixed
title: Lava no longer pops and costs half as much
---

Lava's crust plates no longer vanish in one frame: a plate forms out of the melt and at the end of its life melts back and sinks. The whole lava surface no longer swaps every few seconds: the flow map's two phases now change pattern only while unseen, and a still pool draws one pattern. The lava's own cost per frame is about halved (the surface looked up once a pixel instead of three or six times, the mirror marched only on the smooth melt). `gpu-lava` gains a `steady` view that films the surface frame to frame and fails on a pop; the `se-client` lab's `--film-step` films a map's lava over seconds.
