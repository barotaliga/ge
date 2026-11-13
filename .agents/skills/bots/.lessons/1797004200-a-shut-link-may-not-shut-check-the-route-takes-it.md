---
title: A link the bot "shuts" may still be routed; check the route really avoids it before tuning what it believes
date: 2026-11-13
scope: plugins/shooter/nav.cpp, plugins/shooter/pilot.cpp, games/shooters_edge/bots/fighter.cpp, games/shooters_edge/bots/map_nav.cpp, games/shooters_edge/bots/floor_trap.cpp, games/shooters_edge/net/classic_mirror.cpp
concepts: [bridge, bridgeseen, shut-links, cut-links, nav-route, nav-reach, floor-door, floor-trap, trapflee, dm2, lava, client-fed, no-route, failed-jumps]
---

- dm2's RA pit killed bots walking onto its bridge while it was retracted, from the match's
  first seconds. Fixing the BELIEF (an unsent bridge seen gone) moved nothing, because the belief
  was already right: `nav_route` read cut links only on links through a mover or gap tricks, and a
  bridge or a floor door the nav holds as a solid carries no mover. Every shut link onto a bridge
  still in, a floor trap's floor away and its teleporters was ignored. A probe (the shut list's
  size and the links into the bridge, printed once) showed the list full; the search was the bug.
  When a fix to what a bot believes changes no bit, print what the planner is handed and read
  the planner's filter.
- The same filter silently dropped the pilot's failed jumps ("missed twice: not taken again").
  Honouring every cut link halved dm2's jumps and cost 17 units/s of speed: a long-dead rule
  turned on is a new behaviour, judged as one. Keep the bridges' shut links apart
  (`nav_route`'s `shut`) and change one thing.
- A route shut must be shut in the REACH too (`nav_reach`): goals are chosen by what the reach
  says can be got to, and a goal only a shut bridge reaches left bots standing on "no route"
  (1.3 -> 6.1 % of the time, speed -10).
- A mover's node list grown by a player's half-width took the walkway's row beside the bridge's
  root: a bot standing there planned on across the bridge (shut links close only links INTO it).
  A bridge's own nodes are those with no floor of the map's under the box without it.
- Under `--client-fed=1` a mover the client is no longer sent stays where it was last seen: a
  floor door stood away in a bot's mind for 48 s, a bridge seen out stood out. Give a belief out
  of sight an end (the door's cycle) or a check by sight.
- A bot on a floor that slides away cannot follow the nav's floor nodes: they stay where the
  floor was. Run straight at solid ground (and dm2's trap floor lies 20 under the strips beside
  it: the way off is a jump up).
