---
type: Fixed
title: cr_monsters reaches the map a level's own exit leads to
---

Classic remix's monsters (`cr_monsters`) set during a single-player map now reach the next map whatever starts it: a level's exit, the start map's skill gates or the restart after a death take the changed `cr_` settings and read the map through the remix, as `map` at the console already did. A `cr_` setting changed while classic remix is off says so and how to turn it on; the console's remix line names the monsters' game; an uncaught exception is also kept in `crash.txt` in the game's settings folder.
