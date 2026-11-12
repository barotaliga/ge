---
title: A water underside flush with a ceiling needs its own occlusion, not the depth test
date: 2026-11-11
scope: engine/render/water_underside.wgsl, plugins/brush/app/bsp_liquids.cpp, games/shooters_edge/client/world_extras.cpp
concepts: [water, underside, open-below, depth-test, z-fighting, mirror, view-up, snell, bsp, pool]
---

A classic pool a player drops through has its water's bottom in the same plane as the ceiling round
the hole, and its bottom can be wider than its top, so the top's box and shore mask cut it short and
a depth-tested plane at the floor z-fights the ceiling. Lay the underside past the box, draw it with
no depth test, and discard where the opaque scene's distance is at or under the UNDISTURBED plane's
distance along the ray (plus a few centimetres): the waves then never lift the water over the
ceiling's edge. The views it needs are the mirror in the FLOOR's plane (`below`, what lies under it)
and the view up from the TOP (prepare_bsp_views' `up_level`). Through a flat slab entered from the
air there is no Snell's window rim to draw: every ray that came in through the bottom leaves the top;
only the top's tilted waves reflect totally.
