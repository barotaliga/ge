---
title: Split a view's fault into where it aims and how it gets there, and read a refusal by the route's age
date: 2026-11-13
scope: plugins/shooter/bot.cpp, plugins/shooter/view_book.hpp, plugins/shooter/pilot_gaps.cpp, games/shooters_edge/labs/se_team_lab.cpp
concepts: [facing, calm-view, view-lead, turnview, view-book, gap-refused, water, wet, pool]
---

- A view that trails the run is either a hand too slow or a TARGET in the wrong place. The trace's
  `look` (the target) and `yaw` (the view) split it in one script: the calm view sat ON its target
  (p50 3 degrees off) while the target, the route's point 320 on, lay 70 degrees behind the turn.
  Tuning the hand's pace would have moved nothing.
- A view closer to the run (`lookturn`) and people's own view by spot (`viewbook`) each brought a
  turning measure nearer people's and left enemies appearing on screen unchanged: count what share
  of the appearances the view writer you change actually holds before building it (the calm view
  held a tenth; swimming and the hop's air view held as much each, at 20 %).
- People's view at a spot is consistent (a 96-unit cell and a heading octant: the other
  recordings' mean predicts a person within 30 degrees 64 % of frames, along the run 56 %). The
  bots' is not (44 / 44): a book of people's view is a fair yardstick for a view's choice of spot.
- A pilot's refusal on the FIRST command of a route (`k` 0 in the trace) is the planner's fault,
  not the flight's: e1m2's pool gaps were planned from runs within 160 units of the lip heading
  away, refused at once, and the plan round went through the water. Turning to the gap first
  (`gapturn`) only delayed the refusal: from that near no jump builds the speed.
- se-people's `wet` against people's is a cheap tell to check first on any map with water: the
  bots were wet 28 % of e1m2 against people's 9 %, a gap bigger than any facing change moved.
