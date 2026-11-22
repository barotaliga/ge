---
type: Added
title: The classic file words, net_restart, Quake II's own ban words and fuller status answers
---

A classic game's server console answers `dir`, `fdir`, `whereis`, `link` and `softlink` (with their `unlink`s), and Quake II's adds `path` with its totals and `fs_restart`. Every dedicated server takes `net_restart` to open its socket again, falling back to the last port that worked. Quake II's `sv addip`, `sv removeip`, `sv listip` and `sv writeip` work on the server's ban list as the game's own words, and its `listip.cfg` loads. A Quake II status answer now shows the game's rules (deathmatch, coop, dmflags, fraglimit, timelimit, cheats), the version, the game folder, the port and an administrator's own keys. A bad game folder name is refused, and a deathmatch server of one slot is given eight.
