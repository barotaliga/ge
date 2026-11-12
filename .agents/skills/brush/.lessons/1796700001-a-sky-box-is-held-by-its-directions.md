---
title: A sky box's orientation is tested by directions, not by pictures looked at
date: 2026-11-12
scope: plugins/brush/app/sky_box.cpp, tests/brush_sky_box_test.cpp, plugins/brush/app/bsp.wgsl
concepts: [sky-box, skybox, orientation, rt, up, cube-map, atlas, resample]
---

Each picture of a sky box has a look, a top and a right (`kSkyBoxPictures`, engine axes), and the
atlas's six faces have their own (`kSkyBoxLook`, `kSkyBoxUp`, right = cross(up, look), the same
rule `sky_box_at` uses in the shader). Code every test picture's texel with its own (x, y, picture)
and check that each atlas texel's direction equals the direction of the texel it read: that holds
every face and every turn at once, which no picture looked at by eye can. The up and down
pictures are the ones that turn (their tops point along the map's x, the face's up along +z).

`bsp.wgsl` with its includes now sits a few bytes under the embedding's 60 KB cap (its resolved
text, comments dropped). The sky box fitted by writing tight WGSL (abstract ints: `vec3f(0, 1, 0)`,
`vec3f()` for zero); the next feature in the world pass splits a part into its own shader (the
`gpu` lesson on a shader near 64 KB).
