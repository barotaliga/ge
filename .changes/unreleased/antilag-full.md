---
type: Added
title: Lag compensation at its full extent in both classic games
---

`sv_antilag 2` rewinds every shot's line as well as the shots the game marks (1), `sv_antilag_projectiles` lets a player's rockets, grenades and nails meet the others where he saw them, and `sv_antilag_max` caps how far back a shot may see (250 ms by default in Quake II, which also gains `sv_antilag_interp`). A rewound player keeps the box he had, so a crouch is rewound with him, and a teleport or a respawn is never blended across. A player who sets `cl_antilag 0` turns it off for his own shots and is told so.
