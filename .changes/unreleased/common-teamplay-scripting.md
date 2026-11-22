---
type: Added
title: Teamplay and console scripting shared by every classic game
---

Quake and Quake II now share one set of teamplay and scripting tools. Aliases take arguments in every form (`$1`, `$@`, `${2-}`, `%1`), `if` compares with `eq`, `ne` and `isin`, `msg_trigger` takes a message level, and the string commands `alias_in`, `alias_out`, `cvar_in`, `cvar_out`, `set_alias_str`, `set_bind_str` and `hash` build messages piece by piece. Team messages read the player's state through `$health`, `$armortype`, `$bestweapon`, `$powerups`, `$location`, `$ledstatus` and 28 more in each game's own words; `f_mapend` and the countdown hooks run on a joined match; `cl_fakename`, flood protection, `cl_nofake`, the sound trigger and `f_version` answers work in both games, and Quake gets Quake II's map commands and `$loc_here`.
