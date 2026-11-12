---
title: The run-up dance only showed when the mode's entries were counted against their place
date: 2026-11-12
scope: plugins/shooter/pilot.cpp, games/shooters_edge/labs/se_team_lab.cpp, games/shooters_edge/labs/se_trick_lab.cpp
concepts: [run-up, retry, dance, jumpfly, brief-stops, trick-map, stall, gap-off, nearest-node]
---

The brief stops' causes looked like wall hits and slow turns until each entry into `run_up` was
counted with the last one's time and place: 1.8 of 3.4 run-ups a bot-minute were a RETRY within
4 s and 96 units, 2.3 s a bot-minute spent lining up again behind the same jump (up to 11 s), the
keyboard reaching the start 15-30 units off a 10-unit line. Flying the jump from where it stands
(`jumpfly`) halved it. Count a mode's re-entries by place before tuning its thresholds: a loop
reads as many unrelated short stops. se-team now reports `team.jump.run-ups` and `.flown`.
On a trick map (`se-trick`) two things bit first: a teleport destination is lifted ~67 units over
its floor, so `nav_nearest` (64 up or down) finds no node and `run_route` never starts (start
from the brute-force nearest node's origin); and a gap trick the pilot gives up leaves it idle
where a match always has a way round, so read the lab's stalls table, not just its fails.
