---
title: Set the bots' numbers beside people's before guessing at a fix, and split speed by the pilot's mode before changing the walk
date: 2026-11-07
scope: games/shooters_edge/bots/fighter.cpp, games/shooters_edge/labs/se_team_lab.cpp, plugins/shooter/bot.cpp
concepts: [realism, ktx-stats, speed, hop, chase, plans, companion, dodge]
---

Measure against people before guessing at a fix: KTX's stats of real matches (frags a minute,
kills by weapon, items taken, average speed) beside the bots' showed the gaps (the launcher and
the yellow armour never taken, speed 185 against 284), which no lab of ours had flagged. Two tries
at speed did nothing and were taken out: keeping the hop on while aiming (the move reads the wish,
not the view, so it is allowed, but frags fell), and keeping a chase's goal while the enemy moved
under 128 units (frags fell on all three seeds). The pilot's `plans` count is not a count of goal
changes: a pilot standing arrived on its goal plans again every command. Split speed by the pilot's
mode (`se-team`'s mode table, aiming apart) before changing the walk. A fighter change reaches the
co-op companion too (it is a fighter): the dodge put a companion out of position and a monster
fell to the leader instead; gate a deathmatch behaviour off `escort.leader >= 0`.
A third try, also taken out: pressing the walk's keys at the target from the view as it stands
(forward and side together, instead of cutting the forward key while the view turns) sent human
hands off the course's jumps and ledges (4 of 12 runs arrived): their side key comes as late as
their yaw, so the wish wanders; the cut on turning is what keeps late hands on the line.
