---
title: A hop's speed is split command by command into the keys' and the walls' share, and its trips timed against a recorded player
date: 2026-11-01
scope: plugins/shooter/pilot.cpp, plugins/shooter/bot.cpp, plugins/shooter/hops.cpp, games/shooters_edge/labs/se_moves_lab.cpp, games/shooters_edge/labs/se_hop_lab.cpp, games/shooters_edge/labs/se_team_lab.cpp
concepts: [bunny-hop, hop-speed, air-accelerate, walls, lintel, stairs, airwalk, view-lag, trace, se-moves, se-hop]
---

- Tune a hop against a recorded player (`read_hops`; `se-hop` replays his trips). Time a trip by its
  first pass within reach of the end, not `arrived` (a fast hop overshoots and returns); read
  se-hop's speed p50 and p90, not its `gain`.
- The air move recomputes off an `se-team --trace --trace-every=1` row (the wish's share along the
  velocity capped at 30, accelerate x wishspeed x 0.013 s; 0.01 u/s p50), so a flight's change
  splits exactly into the keys' and the rest. Use `vx`, `vy`. `se-moves --track` splits losses into
  one-frame drops (walls: 91 % of the bots') and gradual ones.
- dm2's hops lost ~200 u/s a wall stop, at a few places (count `speed - last < -100` by 64-unit
  cell). Compare the feet with a link's ends, not node with node (a hop started under a stair node
  jumped into the riser); check the CEILING over the route within the flight's reach (lintels); stop
  jumping when the stretch left is under a flight's reach.
- A double lag (the pilot's hands lead, the mouse hand follows) held the view past the 7-degree
  window where a side key gains: choose air keys for the view really pressed (`aim_air_set`). Two
  thirds of a chain's key loss came after the hop ended in the air (`airwalk`). A plain arc with no
  braking key is faster and read by every aim (-5.6 a match head to head).
- A jump held on the way down goes off on the landing frame: no ground row between flights, so split
  chains at the jump's rise. Under ~240 a side key gains less than a ground run.
