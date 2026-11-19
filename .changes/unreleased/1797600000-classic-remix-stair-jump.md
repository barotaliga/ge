---
type: Added
title: Classic remix's cr_physics carries the stair jump
---

Under `cr_physics 2` the classic game jumps like Quake II onto a ledge: a jump that reaches a ledge while still rising lands on it and keeps the rise, so a jump off it right away goes much higher (the stair or crate double jump). Under `cr_physics 1` Quake II loses it and climbs a ledge only by rising past the edge, as the classic game does.

New server setting on the classic rules: `pm_airclimb`.
