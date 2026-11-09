---
type: Fixed
title: Taken items vanish in single player and co-op
---

An item or weapon picked up in the classic campaign (the green armour at the start of e1m1, say) no longer stays drawn: an entity whose program empties its model text is neither drawn by the window nor sent to clients nor recorded, and our own campaign program removes taken items unless `deathmatch` is on. `se-campaign --pickups=<s>` checks every item of a map.
