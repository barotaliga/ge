---
title: Count the hops never started before tuning a flight; people get their speed by a circle jump on the ground; a look before a jump must fly the hop's own turn
date: 2026-11-10
scope: plugins/shooter/pilot.cpp, plugins/shooter/nav_gaps.cpp, games/shooters_edge/labs/se_moves_lab.cpp, games/shooters_edge/labs/se_hop_lab.cpp
concepts: [circle-jump, ground-strafe, prestrafe, bunny-hop, hop-speed, hop-stretch, brink, liphop, fight-hop, chain, crash, se-moves, se-hop, hands-wander]
---

- Before shaping a hop's flight, split the bot's alive time by pilot mode and speed: on dm2
  experts and averages walked at 280+ u/s 31-34 % of their lives and hopped 6-7 %; 62 % of that
  fast walking was in a fight (the hop is off while the view is on an enemy). The flight's gain
  is the smaller lever; the stretches never started are the larger.
- Count WHY a stretch is refused, per command at speed on a walk (a temporary print in
  `hop_stretch`): a lava lip 34 %, the route's end 26 %, a climb 17 %, a mover 13 %. A rule that
  was a hop's own stop (the lip) may be redundant once a guard covers the same danger everywhere
  (the brink guard in the air): letting hops run along lips raised hop share 9 -> 14 % with lava
  deaths unchanged.
- People's chains are as short as the bots' (most of their jumps stand alone), yet a chain's first
  jump already takes off at 433 u/s (12 dm2 matches, se-moves `--track`): they get it ON THE
  GROUND, by a CIRCLE JUMP. In the 0.3 s before a standing jump that takes off at 400-480 their
  speed rises 347 -> 424 while the heading turns 51-75 degrees and the view stays on the velocity
  (forward + side: the wish ~45-50 degrees off it). The move allows it: on the ground with the
  wish 50 degrees off the velocity friction and acceleration settle near 465, and from 320 the
  best angle each command reaches 440 in 0.13 s. A walk with the wish along the velocity is
  capped at 320. Chained jumps come on the landing frame (ground p50 0.015 s) at no cost.
- Past 380 people fly with the view within 10 degrees of the velocity (81 % of flights): the side
  key alone, as the expert hop does. Under 380 the view sits 30-50 off (forward + side).
- The hop reader's ground-frames table (se-moves) said people stand 4-5 frames before a jump;
  reading the track frame by frame said one. Check a summary against the raw frames before
  building on it.
- A look before a jump (fly the jump, run on if it meets a wall) flown with the forward key toward
  the route turns as fast as the air allows: quicker than either hop really turns, so it passed
  the flights that crashed and only shortened chains. A predictor must fly the pilot's own rule.
- se-hop's `gain u/s/s` swings from -90 to +60 on a 0.3-degree change of the hands' wander with
  the speeds nearly equal: read its speed p50/p90 and its trips, not the gain. Its dm2 trips miss
  the runtime's floor doors and teleports ("no way on the bots' graph").
- Hopping more costs sightings: the hop's view rides the velocity, not a scan (head to head the
  side hopping more saw ~9 % fewer enemies). A hop that lasts longer needs a view that still
  sweeps the room, as people's weave does.
