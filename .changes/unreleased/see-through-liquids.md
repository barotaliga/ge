---
type: Added
title: Shooter's Edge draws see-through water, slime, lava and teleporters
---

`r_wateralpha` below 1 lets you see through the warped water drawn by the classic look (and wherever the map's own liquid faces are drawn), with `r_slimealpha`, `r_lavaalpha` and `r_telealpha` for the others (empty: as water). A map's own `wateralpha` and kin hold for that map, and a server that sets `watervis` 0 keeps every liquid solid. While a liquid is see-through, the rooms under it are drawn even on a map whose visibility was not built for it.
