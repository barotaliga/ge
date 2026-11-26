---
type: Fixed
title: In Quake and QuakeWorld, toggle flips every setting it names
---

`toggle a b c` flips each of `a`, `b` and `c` between 0 and 1 while Quake or QuakeWorld is loaded, as their configs expect; in Quake II and the engine's own console it still steps `a` through the values `b` and `c`. The new `flip` command does the first everywhere.
