---
title: A door's errand whose button lies past the door is the way round already; weigh it against going round
date: 2026-11-20
scope: games/shooters_edge/bots/fighter_doors.cpp, plugins/shooterbots/pilot_plan.cpp, games/shooters_edge/labs/se_team_lab.cpp
concepts: [door-errand, errandworth, button, locked, door, round, time-up, dm2, red-armour, pilot-way, se-team, buttons-table]
---

- dm2's south red armour's west door opens off a touch button INSIDE the armour's room (it lets a
  player out). A bot west of the shut door "went round to its button": that is going round to the
  armour anyway, over a bridge's button and a lava gap's jump, and the 12 s errand ran out (47 of
  67 a match). The handoff blamed fights; the decision log's `mode` per errand said ~9 of 12 s
  roaming. Read where the errand's bot WENT (the trace's distance to the button) before blaming
  what pulled it away.
- An errand is a choice of ways: weigh button + press + back through the door against the way
  round with the door barred, both by the pilot's own planner (`sh::pilot_way`: barred doors,
  shut bridges, given-up gaps), never a bare `nav_route`.
- A door barred for "going round" must be barred for THAT goal only: barred for every goal for
  20 s, the quad (whose only way was that door) went routeless and bots stood idle (still time
  doubled). Tie a bar to the goal it was weighed for.
- The errands by button are in se-team now (the buttons table, `team.errand.b<n>.*`): read them
  first instead of pairing `door_errand` begin/end rows by hand.
