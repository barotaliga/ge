---
title: A fixangle left set freezes a player's aim
date: 2026-10-30
scope: games/shooters_edge/server/qc_server.cpp, plugins/shooter/client.cpp
concepts: [fixangle, view-angle, mod-server, bots, baseline-mod]
---

The first baseline match had no frags: every shot left along the spawn point's angles. A mod
sets `fixangle` in PutClientInServer, the loop keeps the command's angles out of `v_angle` while
it is set, and nothing cleared it. The server now sends a forced view once at the frame's end
(`send_forced_views`: the view becomes `angles`, the flag clears). When a mod's players do not
turn, print `vtos(self.v_angle)` and `vtos(v_forward)` with `dprint` (the match report's
`console`) before suspecting the compiler.
