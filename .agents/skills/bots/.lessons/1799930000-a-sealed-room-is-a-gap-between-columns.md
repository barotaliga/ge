---
title: A room sealed on the nav with an open exit is a way narrower than the grid; dump the graph
date: 2026-11-28
scope: plugins/shooterbots/nav.cpp, plugins/shooterbots/nav_squeeze.cpp, plugins/shooterbots/way_through.cpp
concepts: [nav, squeeze, sealed, narrow-corridor, grid, reach, explore-idle, chasm, door-lip]
---

- When a bot idles early with every step "all 0" (unreachable even ignoring doors), dump the
  graph once (each node's origin and reached flag, each link) from the plan and look at the
  reached set's edge in Python: unreached nodes beside it mean a missing link, none means a
  missing node. hip2m3's exits had no node at all.
- `secli map slice` draws hull 0 (what is drawn), not the player's hull: a 56-wide corridor there
  can leave the box 20 units, between two rows 32 apart. Trace the player's box down at 2-unit
  steps across the way (`sh::trace_move` with the player's mins and maxs) to see the real room.
- Squeeze only where the neighbouring column is rock at the floor's height: an open column is a
  chasm's edge, and a node on its far lip invites a jump the bot misses (hip2m3's lava cleft).
- Centre a squeeze node only in a way that is narrow on both sides: centring every one moved most
  of them within reach of a grid node, the known-test dropped them, and r1m5 fell from 20 to 5. A
  node at a narrow way's edge leaves no room for an opened door's lip (hip3m1's secret nook).
