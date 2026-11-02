---
type: Added
title: a mod's own qwprogs.dat runs from its folder
---

With no `-progs`, Shooter's Edge now looks through the game's files, a mod's folder over the base game, for the multiplayer program files `qwprogs.dat` and then `progs.dat`, and runs the first one that is a multiplayer program. So `-game mymod` runs that mod's own program. A single-player program is passed over, and the status line says which program runs. The start map, the full-game rule and these file names now come from the game's profile rows instead of the window's code.
