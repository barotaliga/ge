---
type: Added
title: Lag compensation on our server of the sequel
---

Our own server of the sequel now has the first game's lag compensation: with `sv_antilag 1`, a player's bullets and railgun hit the others where that player saw them on screen, so a hit on a high-ping connection counts as it looked. It is off by default, as on the first game's server, and `sv_antilagexact` works the same way.
