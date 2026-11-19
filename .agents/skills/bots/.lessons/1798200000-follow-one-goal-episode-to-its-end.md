---
title: Follow each goal or role episode to its end before tuning scores; flips hide there
date: 2026-11-19
scope: games/shooters_edge/bots/mind.cpp, games/shooters_edge/bots/fighter.cpp, games/shooters_edge/labs/se_team_lab.cpp
concepts: [item-control, rush, role-flip, goal-flip, episodes, trace, glabel, item-lying, quad]
---

The quad lay 56 % of a dm2 4on4 against people's 4 %, with "a bot going for it" most of that
time. Shares (time going, time near) said nothing; cutting the se-team trace into EPISODES per
bot (from the moment a goal or role starts to the row it changes) and asking how each ended did:
703 trips to the quad, 597 ended by the goal changing ~2 s in, no nearer; 1217 rush roles in 12
five-minute matches, each over in a median 1.5 s. The role was counted again every think (among
the two nearest of the team) and wobbled. Before weighing values or routes, run the episode read:
start, end, why it ended (goal changed, died, taken by me, taken by another), how far it got.
The trace's `glabel` (the goal by its spot) and `role` columns make it a short script.
