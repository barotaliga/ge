---
title: Quake II's solid boxes reach the client's prediction, and a seat kept over a level change is not a new player
date: 2026-11-21
scope: games/shooters_edge/server, games/shooters_edge/net, games/shooters_edge/client
concepts: [prediction, solid, disc, teleporter, spawn-spot, coop, level-change, connect, carried, obituary]
---

- **A solid box the server sends must be clipped by the window's prediction too.** Making the
  deathmatch spots solid discs (the game's own) broke `shooters_edge_q2_net_test` by 9 to 18 units
  of correction: the player walked off a disc the prediction did not know. The entity's packed
  `solid` (5 bits half width, 5 bits depth, 6 bits height + 32, each × 8; 31 a brush) is now kept
  on `DemoEntity::solid` and `q2_predict` sweeps every box but the player's own.
- **Our box sweep has no epsilon, the brush trace has 1/32.** A player settles at z 24.125 on a
  brush floor but at exactly 32 on a disc's top. A test of a height on a box is exact, not +1/8.
- **The network server re-seats each client on `begin` after every level change.** The rules keep
  the slot through the change and place him with what he carried; a `connect` to an active slot
  must take that seat over (name refreshed, announced), never rebuild the player from nothing, or
  co-op's carried body and score are lost on a dedicated server only (the listen server never
  calls connect again, so its lab never showed it).
- **A model added by the map's points goes in before the player's.** Tests hold the player's model
  last in the list; precache such models (`map_precache`) before pushing it.
