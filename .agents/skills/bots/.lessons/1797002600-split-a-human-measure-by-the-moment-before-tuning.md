---
title: Split a human measure by the moment (calm or fight, dry or wet) before tuning a behaviour to it
date: 2026-11-11
scope: games/shooters_edge/labs/se_people_lab.cpp, games/shooters_edge/bots/footwork.cpp, plugins/shooter/bot.cpp
concepts: [keys, keys-fight, keys-calm, fight-along, fight-feet, press, lookway, calm-view, wet, trace, faced]
---

- A whole-match share hides where the gap is. People's W on a steady run (e1m2, 44 matches) is
  48 % calm and 28 % with an enemy in view; the keyboard bots' 31 % and 17 %: most of the W gap
  was in CALM running (the bots' back keys 19 % calm, people's 10 %), not in the fights the
  trace pointed at. `se-people` now splits `keys.fight.*` / `keys.calm.*` and the fight's feet on
  dry ground by range and launcher (`fight.dry|near|mid|far|rl|no-rl.*`).
- The calm view's point 320 units along the route lay behind the run 17 % of a calm walk (round
  a switchback, across a teleporter, a node just passed), analog bots as much as keyboard ones:
  the view 165-180 degrees off the run, keys pressed backwards. `lookway` (the furthest route
  point in sight, never past a teleporter, never over 90 degrees off the walk) cut the view past
  120 degrees 14 -> 6 % of calm walking; a tighter calm band (25 -> 12) changed nothing off the
  recording. The trace's `walk`, `look` and `view` (glance and sound swipes named) columns
  answer this in one script.
- COUNT WHERE A BEHAVIOUR RUNS before tuning its odds. The press (`press`) pressed on at the
  enemy as people do, yet `fight.dry.along` moved +7 -> +17 against people's +68: of the bots'
  aiming commands 38 % were in WATER, 30 % on a target held out of sight, 11 % on an errand, and
  the press reached 15 %. Raising its draw 0.55 -> 0.8 did nothing; widening its gate did a
  little. The trace's `press` column names what held it off each command; `team.press.*` sums it.
- A trace cannot stand in for se-people's "faced": without the map's line of sight an emulation
  counts enemies through walls (its along read +48 against the recording's +20). Measure the
  recording; use the trace to break a number down by cause.
- The water is the fights' biggest human gap on e1m2: the bots are waist deep 28-35 % of the
  time (people 9 %) and 31-43 % of their facing an enemy is there (people 7 %): plan item 0c.
