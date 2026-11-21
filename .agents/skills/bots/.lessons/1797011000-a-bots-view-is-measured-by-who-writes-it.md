---
title: A bot's view is measured by who writes it and where it aims, and its facing by the enemy appearing
date: 2026-11-08
scope: engine/ai/mouse.cpp, plugins/shooterbots/bot.cpp, plugins/shooterbots/view_book.hpp, games/shooters_edge/bots/fighter.cpp, games/shooters_edge/bots/watch.cpp, games/shooters_edge/labs/se_people_lab.cpp, games/shooters_edge/labs/se_team_lab.cpp
concepts: [mouse, hand, view, view-by, reversals, tilt, turn-back, calm-view, glance, lookway, facing, appear, watch, finger, se-people]
---

- se-people's `hand.*` read alike: sweeps dead level 3 % for people against 60 %, reversals 0.09
  against 0.74 a second. The mouse hand on the aim and calm view moved them only to 52 % and 0.40:
  most of the view came from other writers. Count reversals per writer (the trace's `view`).
- A minimum-jerk stroke runs at 1.3x its mean speed at 70 % of its time: a stroke planned back then
  reverses at speed (`MouseWant::turn_back`).
- A finger clicking only ON the target fires late (~100 frags a set): click roughly on (2.5x the
  tolerance), some clicks timed to the refire.
- A view trailing the run is a slow hand or a TARGET in the wrong place: the trace's `look` against
  `yaw` splits it (the calm view sat on a point 70 degrees behind the turn; `lookway` takes the
  furthest route point in sight).
- A still view lost head to head to a snapping one (an accidental scan); a glance on purpose (75-120
  degrees toward the longest sight line, at a hand's pace) won it back. Judge a watch by enemies
  seen (`in-view.seen`, `pre-aimed`), not time watching.
- FACING: the sharp measure is an enemy APPEARING and whether he is on screen then, by where he lies
  against the run (ahead: people 90 %, bots 69 %). Count the share of appearances the writer you
  change holds (the calm view a tenth).
- Two layers limiting one motion (a pace and a dead band): the pace's step must exceed the band, or
  the hand never strokes (a bot ran 35 s looking 79 degrees down).
