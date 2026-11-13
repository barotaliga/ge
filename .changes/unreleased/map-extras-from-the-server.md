---
type: Added
title: A map's coloured light and place names fetched from the server
---

Shooter's Edge's first game, joining a server, now fetches the map's coloured light (`lits/<map>.lit`) and place names (`locs/<map>.loc`) when you lack them, and only while they are on (`r_loadlit`, new, and `tp_loadlocs`); both are kept in your game folder. `r_loadlit 0` draws every map in its own grey light.
