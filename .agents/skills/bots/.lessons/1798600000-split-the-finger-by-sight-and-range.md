---
title: Split a fire-rate gap by sight and by range before building for it
date: 2026-11-20
scope: games/shooters_edge/bots/fighter.cpp, games/shooters_edge/bots/launcher_fire.cpp, games/shooters_edge/labs/se_team_lab.cpp, games/shooters_edge/labs/se_people_lab.cpp
concepts: [launcher, finger, trigger, rockets-per-min, blind-fire, weapon-switch, rlback, afterfire, earfire]
---

"Why does the finger hold with the launcher up" had no answer in the trigger's own counts (they
run only with a target in sight). A ledger of EVERY command carrying the launcher by its reason
(`FingerWhy`, se-team `team.finger.*`, `.view.*`, `.band.*`) and the same split of people's
rockets off recordings (se-people `rl.*`: in view, just lost, blind; by the nearest one's range)
found three gaps no single number showed: with one in plain view people fire 55 a minute, bots
30; with none in view 11.6 against 1.8; at mid range a quarter of the bots' time had the
scattergun up. The trace's `weapon` and `finger` columns then showed the cause in one look: the
scattergun asked for under the blast's reach LANDS after the launcher's refire, when he is at mid
range again (`rlback`). People's blind rockets read by where the crosshair meets the world: 26 %
burst in sight of an unseen enemy (`earfire`: +4.9 a match).
