---
type: Added
title: Renames limited, and announced if the server wants
---

`sv_namelimit` lets a player in the game rename N times in any S seconds ("5 60" by default; "5/min" reads the same) and keeps the old name past that, telling the player why. `sv_shownames` 1 tells everyone when a player renames, 2 when a spectator does too. Both games' servers read them; Quake II's `sv_namechange_limit` and `sv_show_name_changes` are its spellings.
