---
type: Fixed
title: Quake II's own screen tints show, and a hit can flash round the screen's edge
---

Quake II's screen tints (the damage, pickup, underwater and power flashes as the game works them
out) now show in a local game, on a server and in recordings. Before, the window drew Quake's
flash in their place. New setting `gl_damageblend_frac`: a hit's flash is drawn as a frame round
the screen's edge, so the middle stays clear. It is 0.2 in Quake II and 0 in Quake, where the
flash covers the whole view as it always did.
