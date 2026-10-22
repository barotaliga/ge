---
title: Keep a max-blended map's channels from adding up where they should exclude each other
date: 2026-10-22
scope: plugins/mountain/app/snow.wgsl
concepts: [trails, berm, groove, max-blend, beads, stamp, capsule, channels]
---

The first tracks came out beaded: a bump every frame's length along each groove. Each stamp is
a capsule with a berm ringing it, the ring round its end crosses the next stamp's groove, and
max blending keeps each channel's maximum on its own, so the berm channel held a ring inside the
groove the depth channel held. The surface is berm less depth, so every joint between two stamps
rose. A berm stands only beside a groove: the material takes the berm only where the depth is
nought (`trail_texel`). Whenever a map's channels are blended one at a time, ask what each
channel's maximum means where the other is set, and resolve it where the map is read.
