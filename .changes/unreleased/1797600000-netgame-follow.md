---
type: Added
title: a single-player client picks who it watches at the TV
---

A single-player client watching a multiview recording through `setv` now chooses the player it follows. Fire goes to the next player, jump to the one before, and a weapon key 1 to 8 to that player. `cmd follow <name or number>` picks any player. The viewer is told who it follows, and a switch picks up where the match is, scoreboard included. A view is prepared the first time someone asks for it, without holding up the other viewers.
