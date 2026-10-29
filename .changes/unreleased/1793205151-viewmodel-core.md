---
type: Added
title: First-person view models
---

`engine/viewmodel` draws the weapon a first-person game holds: any model converted once into a held model (morph frames, a skin with glowing texels or a colour a vertex) and turned into the hand by its grip mount; placed in the view each frame with the eye's bob pushed forward, a sway that lags a turn, a shot's kick, a landing's dip and a put-away (a classic and a modern row; comfort scales it); and lit by the place the player stands in, the baked light at the player and up to eight local lights on the renderer's fall-off, with a floor that keeps a gun in the dark a silhouette. A CPU reference raster draws it, and the `viewmodel` lab pictures every weapon of a directory held, one under six lights (day, dim, dark, a torch, its own muzzle flash, coloured lamps) and a walk, turn, shot and put-away.
