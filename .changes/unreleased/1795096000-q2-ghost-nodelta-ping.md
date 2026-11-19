---
type: Added
title: Quake II servers show real pings and free slots nobody uses (sv_pingmethod, sv_ghosttime, sv_nodelta)
---

A Quake II server's scores and `status` used to show every ping as 0. They now show each player's ping, worked out from their last 16 round trips. `sv_pingmethod` picks how: 2 (the default) takes the least of them, 1 the average, 0 shows none. `sv_ghosttime` (6 seconds by default) frees a slot that a connect took but whose player never sent anything. `sv_nodelta 0` drops a player whose game keeps asking for whole frames, more than 64 of them. Quake II's spellings (`sv_calcpings_method`, `sv_ghostime`, `sv_allow_nodelta`) work too.
