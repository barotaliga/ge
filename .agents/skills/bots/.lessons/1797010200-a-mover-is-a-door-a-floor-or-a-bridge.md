---
title: A mover is a door, a floor or a bridge to the nav, and what a bot shuts must be shut in the route and the reach alike
date: 2026-11-09
scope: plugins/shooterbots/nav.cpp, plugins/shooterbots/pilot.cpp, plugins/shooterbots/pilot_movers.cpp, games/shooters_edge/bots/map_nav.cpp, games/shooters_edge/bots/buttons.cpp, games/shooters_edge/bots/fighter.cpp
concepts: [door, button, errand, floor-door, plat, bridge, teleporter, shut-links, cut-links, nav-route, nav-reach, no-route, mover-in-the-way, client-fed, dm2]
---

- A door only a button opens is an errand. Taking such doors off the routes traded dm2's waits
  (15-27 % of commands) for `no route` 56-83 %: on a deathmatch map they ARE the ways. When the
  route crosses one resting shut, go to its button, press or shoot it, plan again. The campaign
  leader and the companion keep their own door lists: leave them alone or their goldens move.
- The nav proves ways with every mover left out: wrong for a door that IS a floor (dm2's big room
  stands on plates; `floor_door`). `se-reach --route` is the first check when a bot idles with
  `goal_node -1`.
- Opening a way runs links never walked: a teleporter link cut to like a walk pinned bots on a wall
  (`NavLink::teleport`).
- A plat's nodes stand where its top rests: on it means the feet at its top now, a wider band while
  it moves (`kRideLag`: the client's box lags a rider ~15 u).
- Bridges' shut links are `nav_route`'s `shut`, honoured by `nav_reach` too: while the route read
  cuts only on a mover's links, a shut bridge stayed routed and fixing the belief moved no bit.
  Honouring every cut link at once halved dm2's jumps: a dead rule turned on is a new behaviour,
  judged alone.
- Under `--client-fed=1` a mover no longer sent stays where last seen (a floor door "away" 48 s):
  give a belief out of sight an end or a check by sight.
