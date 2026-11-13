---
title: A trap a bot springs is judged by who falls; trace the presser and the mates before tuning
date: 2026-11-13
scope: games/shooters_edge/bots/floor_trap.cpp, games/shooters_edge/bots/fighter.cpp, games/shooters_edge/labs/se_team_lab.cpp, games/shooters_edge/bots/mind.cpp
concepts: [floor-trap, floortrap, dm2, lava, team-call, client-fed, teleporter, side-metrics]
---

- Lava deaths with both teams counted hide whose they are: se-team's
  `team.side.<side>.deaths-in-lava` splits them. The first floor trap killed its own side as often
  as the enemy (+3.8 / +3.7) while head to head read "even".
- Then trace every firing and who entered the lava in the next seconds, and where each was when
  it fired (`trap` column: pressed/away; `liquid` -5). Every cause was found that way: the
  presser walking into the gap, a mate on the floor unseen, a press from the level above, mates
  teleporting or stepping into the alcove's teleporter.
- Under `--client-fed=1` a player out of a bot's sight is NOT SENT: its entity reads `free`. A
  "no mate there" check that skips free entities sees no mate at all; use the last place seen and
  the team lines (`Mind::mates`). A door's state is also only what the bot last saw (one believed
  the floor away 48 s).
- A shut route is not shut feet: a fight's footwork walks into a trigger the route avoids. Guard
  the keys near it while it is dangerous (as `brinkkeys` does near lava).
