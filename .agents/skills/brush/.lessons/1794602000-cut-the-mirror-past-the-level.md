---
title: Cut a water mirror a little past the level, or the waterline shows a line
date: 2026-11-13
scope: plugins/brush/app/bsp_views.cpp, engine/render/wgsl/water_surface.wgsl
concepts: [planar-mirror, clip-plane, waterline, mirror-skirt, empty-texel, room-light]
---

A thin bright (or dotted dark) line where classic water meets a wall comes from the planar mirror. It does not come from the shore feather or z-fighting. When the mirror is cut exactly at the level, the wall's foot falls on the half-size picture's last row and leaves it empty. `mirror_along` reads an empty texel as "off the mirror", so the water shows the room's flat light there.

`kMirrorSkirt` keeps the cut 10 cm past the level. To check a waterline, run the se-client lab at `--cvars="ce_water_quality medium"`: a line that vanishes there is the mirror's.

Growing the visible set through the water to show the pool's bed was tried and rejected: the bed seen through the water looked worse to the player.
