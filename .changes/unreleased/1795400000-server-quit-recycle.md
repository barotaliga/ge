---
type: Added
title: Dedicated servers quit and recycle with a reason; showip, macrolist and the clock macros
---

Every dedicated server takes `quit [reason]` and `recycle [reason]` (the game says goodbye with the reason; a recycle tells players to reconnect and exits for its supervisor to start it again), `showip`, `dns`, `net_stats`, `macrolist [filter]`, `$com_maplist`, `$net_uprate`, `$net_dnrate` and the clock macros (`$com_date`, `$com_time`, `$com_uptime`, `$com_uptime_long`, `$random`). Quake II's server tells its players and relay viewers "Server quit" or "Server restarted" with the reason, answers only rcon after `killserver` (its masters told `shutdown`), and lists its map with `bsplist`; a relay's `mvd_stats_score` picks the frags viewers see. Password settings no longer carry the server-info flag.
