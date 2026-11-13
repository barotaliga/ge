---
type: Added
title: Each classic game's names for the server's settings
---

A Shooter's Edge server now takes each classic game's own names for its settings: `hostname`, `maxclients`, `maxspectators`, `password`, `spectator_password`, `timeout`, `floodprot` and `floodprotmsg`, and the sequel's `sv_maplist` and `flood_msgs`, `flood_persecond`, `flood_waitdelay` (each one word of `sv_floodprot`). They are synonyms of the server's own settings (`Cvars::add_synonym`), so a `server.cfg`, rcon or vote line by either name sets the one setting. A mod reading `maxclients`, `hostname` or `maxspectators` gets the server's values, and the sequel's `dmflags` is taken as a setting, which a classic map leaves alone.
