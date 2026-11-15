---
title: The hop's air turns only as fast as the hand strokes, and a want on the velocity never asks it to
date: 2026-11-15
scope: plugins/shooter/bot.cpp, plugins/shooter/pilot_recipes.cpp, plugins/shooter/pilot_hop.cpp, games/shooters_edge/bots/fighter_setup.cpp
concepts: [hop, airview, airturn, mouse, hand, tolerance, turn-rate, wall, starthead, startclear, trace, git]
---

- With the side key alone the velocity turns only while the view leads it (2-4 degrees into the
  turn), so a view WANTED on the velocity moves only as the view itself does: the hand's 2-degree
  tolerance held the air's turn at 35 deg/s before walls where the move allows ~400 (`airturn`
  wants the view further into the turn when the aim is well off: walls 59 -> 53 %). Measure the
  velocity's turn rate before a wall against the most the air gives (`hop_turn_rate`) before
  blaming the line, the keys or the place.
- People's and the bots' view sit alike off the velocity at each turn rate when read off
  recordings by one rule (the rule's frame lag puts both 4-5 degrees "behind"): compare how OFTEN
  each turns hard (fast air at 150+ deg/s: people 37 %, bots 20 -> 26 %), not the angle.
- A flight's loss split into one-frame drops (walls) and the rest: people meet walls as often as
  the bots (45-51 % of flights) but GAIN in the air (+26 to +62 a flight); the bots bleed. Speed is
  won in the air and on the landing frame, not by dodging every wall.
- A copied start's proof ends at its takeoff: score what the flight after it meets (`startclear`)
  and where it heads (`starthead`), choosing among proven starts instead of refusing them.
- The trace's `recipe` column names why a run at speed is not hopped (a climb, a stair, a mover,
  the goal by distance): read it before building a new hop entry. A switch whose set is bit for bit
  its base never ran: check its wiring line in `fighter_setup.cpp` first.
- `git checkout <file>` to drop a tuning hack also drops the uncommitted work in that file: commit
  the feature before adding a hack to the same file (startclear's wiring was lost this way, after
  its set had run on a binary that had it).
