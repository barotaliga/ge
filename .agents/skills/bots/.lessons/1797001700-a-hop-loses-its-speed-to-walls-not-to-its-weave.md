---
title: A hop loses its speed to walls and lintels, not to its weave; check a flight's ceiling over the route, not a predicted path
date: 2026-11-10
scope: plugins/shooter/pilot.cpp, games/shooters_edge/labs/se_moves_lab.cpp
concepts: [bunny-hop, hop-speed, weave, wall-stop, lintel, headroom, last-jump, stairs, se-moves, who-moves-fastest]
---

- Split a hop's speed command by command before shaping it: on dm2 the hoppers gained in the air
  as they should (~1.35 u/s a command at 340, the move's most), and lost ~200 u/s on each WALL
  STOP (344 in six 3-minute seeds), most at the same few places. Count `speed - last < -100` in
  hop mode by 64-unit cell.
- The `stairs` rule compared node with node: a link's first node is passed across before the
  player stands level with it, so a hop started 16 under it, on the stair, and jumped into the
  next riser. Compare the player's own feet with the link's ends.
- A jump off a stair's top under a lintel flies into it. Three tries failed before the one that
  held: a straight-on trace (the hop turns), a one-sided arc (an expert switches side mid-flight)
  and a trace along the route against a run along the same line (a wall both meet looks like the
  jump's fault, and a run held level floats over a pit the flight falls into). What held: the
  CEILING over the route's nodes within the flight's reach, at the height the flight has there,
  read only while the flight is above its takeoff. While low, run on along the route.
- A stretch's last jump flies ~220 units past `hop_end`: onto the climb that ended the stretch.
  Stop jumping when the stretch's remaining length is under a flight's reach (not for a gap
  trick's run-in, which must reach its lip).
- People's weave by speed (se-moves `--dir`, 51 dm2 matches): the heading sweeps ~90 degrees a
  flight under 340 and 60-77 past it; the bots' matched in the bins they reached. A weave planned
  per flight (turn to people's width past the route's line by the landing) made it slower (more
  wall stops, less gain for experts): dropped. The bots' real gap is that no jump takes off past
  420; people's best do most of theirs there, gaining 10-18 a jump.
