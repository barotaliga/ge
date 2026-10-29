---
title: A mist on a mountain stands on the ground, and a fog is judged in the sun
date: 2026-10-28
scope: engine/render/fog.wgsl
concepts: [fog, mist, froxel, slope, hug-ground, lab, sun, shadows, shader-names]
---

A box of mist placed over a wood on a slope missed the ground: its floor was level, so it lay
metres over the snow on the downhill side and under it uphill, and the picture showed nothing.
`FogVolume::hug_ground` measures a volume's height up from the terrain under each point
(`field_height` on the terrain's heights), so a ground mist lies the same depth everywhere. A
fog lit only by the sky reads as flat grey: judge a fog under a sun that reaches it (the `fog`
lab runs at 11 h, when the woods it looks at are out of the mountain's shade). And a new WGSL
file that includes `frame.wgsl` shares its namespace with the atmosphere: `Medium` and
`medium_at` were already taken, and the only sign was `pipelines.finish` failing with no
message; `make shaders-check` (Tint offline) names the redeclaration.
