---
title: A pool in a room mirrors the room by a planar mirror, and its volume is as deep as the map says
date: 2026-11-02
scope: engine/render/water.wgsl, plugins/brush/app/bsp_liquids.cpp, plugins/brush/app/bsp_views.cpp
concepts: [water, reflection, planar-mirror, ssr, underwater, snell-window, bsp, pool, wind, depth]
---

On a classic map the screen-space march misses most of what a pool mirrors (looking down, the
walls and the ceiling it reflects are off the screen), and its fallback, the sky's model times the
open share, drew bright blue bands of our daylight on water in a torch-lit room. Drawing the map
again with the frame's view_proj times a flip of the height (eye mirrored, the far side dropped by a
clip plane in the frame's sixth plane) is exact for level water and cheap on a classic map. Three
traps: read the mirror where the reflected ray's IMAGE lands (its leg off the mirror's own depth),
not at the pixel; a steep ripple's image falls off the picture or on nothing drawn, which must read
as the room, never as sky, and must not glitter; and the sun's glitter belongs only where the
mirror shows a sky face.

From below, draw the surface as geometry from the same waves and refract each pixel out (exact
Fresnel for light leaving water); the world above needs a view of its own looking up from the
SURFACE over the eye (from a deep eye, the rays read past the pool's walls into the void), clamped
at its rim along each bearing. And a volume's depth is the map's liquid leaves marched down, never
a constant: a fixed 2 m made an eye at the bottom of a 5 m pool "out of the water", and the whole
view under water (the surface included) switched off. Judge it in `se-client --game=<folder> --map=<map>
--views=...` (private pictures) against the before tag.
