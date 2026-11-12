---
title: A circle jump gains nothing past 320 under a view not its own, and a lead change is read in a weapon drill before a match
date: 2026-11-12
scope: plugins/shooter/bot.cpp, plugins/shooter/pilot.cpp, plugins/shooter/circle_jump.cpp, plugins/shooter/intercept.cpp, games/shooters_edge/labs/se_team_lab.cpp
concepts: [circle-jump, ground-strafe, key-set, keyboard-lag, mouse-hand, lead, intercept, rocket-drill, drill, head-to-head, takeoff-speed, landwall, circlekeys, circleview, leadwall, leadmean, leadarc]
---

- Above 320 the ground gains only with the wish near its best angle (~30 degrees off the
  velocity at 338: +18 a command); the 45-degree key sets give ~0. Under the aim's view, a
  spot watched or a lagging mouse hand, every one of a dm2 match's 258 circles stalled at
  ~340 whatever keys were chosen (`circlekeys`), and the keys land 1-3 commands after they
  are chosen while the best angle moves ~4 degrees a command. People hold the keys and turn
  the VIEW (on the velocity, the wish 45 degrees off it). Even fixed (`circleview`, led 12
  degrees: the hand's circles 346 -> 381), circles start too few hops to move a match:
  count how often a behaviour runs, and what it is worth when it does, before building it.
- Split takeoffs by the ground before them (py/takeoffs.py off se-moves tracks): the bots'
  "starts" were 53 % of takeoffs at 324 against people's 31 % at 391, and their landings 241
  against 347. One p50 hid two different gaps.
- A landing beside a wall, its velocity into it, cannot be saved at the press (`landwall`:
  running on keeps speed but ends the hop at a stretch's corner); the wall is made in the
  flight before.
- A projectile's lead is judged first in `se-team --drill=rockets` (direct hits by skill,
  `team.target.<state>.rockets`), where rockets are every shot: `leadwall` read dm2 expert
  11.8 -> 12.5 % there and only +0.4 frags head to head in a normal match. A variant run with
  the same seeds as an earlier set repeats its base bit for bit: compare against that base.
- A slower average of the target's course (`leadmean`) raised novices' hits and cut experts'
  (an expert target turns more often than a half-second drift follows).
- Keep a copy of the labs binary (/tmp/labs-<tag>) for a long set: a `for m in dm2 e1m2` loop
  runs the binary again for the second map, and a rebuild in between changes what it judges.
