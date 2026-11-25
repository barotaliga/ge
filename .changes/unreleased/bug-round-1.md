---
type: Fixed
title: Bug round 1: the map over the title demo, bots without a lobby, and the bots' freeze shortened
---

`map` run from a config or typed as the title's first demo is about to start now loads the map instead of waiting behind the demo. In a deathmatch with `sv_lobby 0`, `addbot` seats bots instead of saying there is no lobby,, and the window's own answer lines no longer run into the next line. In Quake II, `deathmatch 0` and a map with a match running now starts single player instead of a warmup. Adding a bot freezes the game for about half as long while the bots study the map (the same result as before). Bots joining a server with no lobby no longer say `ready`. On our QuakeWorld server `stats`, `teamstats` and `laststats` work typed plainly and are no longer followed by "Bad user command". `se --help` prints the usage, a recipe's last screenshot is taken before it quits, and a join dropped by an unknown message says the server may be newer than the game.
