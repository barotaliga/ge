---
type: Added
title: The sequel's dedicated server changes levels
---

`shooters_edge_sequel_server` moves on when the frag or time limit ends a level (or on `map`, `changelevel`, `restart`): the next map of `sv_map` then `sv_maprotation` is loaded and every client stays connected and signs on again. `fraglimit`, `timelimit`, `dmflags` and `sv_gravity` can be set in `server.cfg` or with `+set`, and an intermission nobody presses a button in ends by itself after 20 seconds.
