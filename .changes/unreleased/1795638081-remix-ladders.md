---
type: Fixed
title: Quake's move climbs Quake II's ladders under classic remix
---

A Quake II map played on Quake's rules (classic remix) can be climbed again: Quake's move takes hold of a ladder brush by the rule Quake II's move runs (`plugins/shooter/ladder.hpp`, one rule for both moves), climbing and descending at 200 by the view or the up move, holding on with no keys. The brush trace now hands back the hit brush's contents (`HullTrace::brush_bits`). Maps of Quake's own family have no ladders and are never probed, so Quake's move there is unchanged. Lab `se-remix-ladder` climbs every ladder of a real Quake II map under both moves. On a Quake II map Quake's slide also pushes off a plane it meets twice instead of stopping dead, so a player no longer sticks on a sloped wall (q2dm3's ladders).
