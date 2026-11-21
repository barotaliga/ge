---
title: Quake II's co-op sweep: items drop onto movers late, pushes test the brush, sham world boxes
date: 2026-11-21
scope: games/shooters_edge/server, games/shooters_edge/bots, games/shooters_edge/labs
concepts: [coop, sweep, items, drop, movers, push, rotating, nav, world-bounds, spawn-point, game-library]
---

The `se-q2-coop-sweep` lab (every campaign map in two-player co-op, our rules or a game library
with `--library`) found four bugs nothing else had:

- **Items drop 0.2 s in, through the movers too.** A key resting on a train (city3's security
  pass) fell to the floor under it when the drop ran at spawn against the world alone: drop items
  at the second frame, after the trains stand at their first corners, tracing the solid movers.
- **A push moves what is in the mover's brush, not its box.** A turned or L-shaped mover's box is
  far bigger than its brushes: testing the box alone swung city3's data CD round a mover it only
  sat beside, until it was crushed. Test the thing's box against the mover's own model.
- **A map's world box can be a sham.** fact3's world model is 99999 each way; the bots' walk and
  swim grids sized from it ran out of memory entering the map with a companion. Size from the
  map's faces, and refuse a swim grid past 32M cells.
- **The dead in co-op came back only in deathmatch's rule.** Our respawn checked deathmatch only.

A game library hosted unchanged (the stock one) HANGS joining a second co-op player on a map whose
co-op spots do not match the level's spawn point (an empty one included): its own fallback loops.
Start each map at the spot the campaign enters it by (`--first=base2$base1`), never bare.
