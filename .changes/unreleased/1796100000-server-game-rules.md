---
type: Added
title: A game's own rules are server settings
---

A game names its own rules for the dedicated server (`ServerGame::rules`), and each becomes a server setting beside the core's: set in any `server.cfg` layer, with `+set`, or at the console, shown by `--explain`, and handed to the game in `ServerConfig::rules`. Shooter's Edge's server takes `deathmatch`, `teamplay`, `fraglimit`, `timelimit`, `samelevel`, `sv_gravity` and `sv_maxspeed` this way (`+set fraglimit 20`), leaves an unset one to the mod's default, refuses one that is not a number, and prints the match's limits in its status line. Its map can come from a pack in the game folder.
