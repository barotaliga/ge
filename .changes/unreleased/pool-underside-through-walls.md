---
type: Fixed
title: a floorless pool's underside no longer shows through walls
---

Under classic-enhance, the start map's pool over the drop to the fourth episode drew grey jagged teeth through the walls of the first hall (by the lava, looking up), and from the hall under it the pool's underside showed in mismatched panels. The underside's coarser mesh rings, lifted out of the way where a finer ring covers them, are now dropped in its shader (it draws with no depth test), so neither shows.
