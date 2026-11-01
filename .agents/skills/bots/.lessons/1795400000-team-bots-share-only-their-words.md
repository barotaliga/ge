---
title: Team bots share only their words; plan each one alone, and stagger the plans
date: 2026-11-01
scope: games/shooters_edge/bots/mind.cpp, games/shooters_edge/bots/talk.cpp, games/shooters_edge/net/classic_team.cpp
concepts: [team-message, separate-brains, read-by-skill, claims, rush, loc, say-team]
---

A team planner that reads every member's state plays like one mind and does not look human. Give
each bot its own beliefs, fed only by its own senses and by the team lines its CLIENT receives
(the server's prints, split by `split_team_line`), so a person's line counts the same as a bot's.

Three things go wrong when each plans alone:
- Every bot plans on the same tick, so at the start two claim the same item in the same frame
  ("coming rl" twice). Draw each bot's first plan off its own stream (`kPlan_s * draw`).
- A bot that does not know where its mates are believes itself the nearest to everything: all
  four rush the quad. Have the rusher SAY it (`coming quad`) and count heard rushers as ahead.
- Its reach is unknown on the first plan (`reach_at < 0`), and an item in a pocket looks
  reachable: hold the opening claim until the reach is worked out.

The callouts need a place for every point: with no `.loc` file, name places off the items, and
keep `item_call` lower case so a name read back matches the points it was made from.
