---
type: Fixed
title: The nailguns' barrels turn smoothly in first person
---

The nailgun and super nailgun held firing no longer jump from frame to frame ten times a second in your own game: each shot's frame is blended in from the last over the frame's tenth of a second, so the barrels turn every picture. The weapon's own muzzle flash comes out of the gun whole on each shot instead of being smeared across the screen. On a joined server, in a demo and in Quake II, the held weapon's frames are now always blended over that same tenth of a second, so a recording written 30 times a second no longer turns the barrels at an uneven speed. `r_lerpframes` is accepted as another name for `r_lerpmodels`.
