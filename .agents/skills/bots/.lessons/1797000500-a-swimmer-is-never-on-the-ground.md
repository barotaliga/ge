---
title: A swimmer is never on the ground: every check the pilot gates on it is off in the water
date: 2026-11-08
scope: plugins/shooter/pilot.cpp, plugins/shooter/bot.cpp, plugins/shooter/aim.cpp, games/shooters_edge/bots/fighter.cpp, games/shooters_edge/labs/se_people_lab.cpp, games/shooters_edge/labs/se_team_lab.cpp
concepts: [water, swim, pool, floor, stuck, lost, replan, onground, lintel, friction, turn-axes, pools, wet, se-people, samples]
---

Bots spent 36 % of their lives waist deep (people 9 %), and three earlier tries at the keys and
the route's price moved it little. Breaking the wet time down by what the bot was doing (se-team
`team.wet-why.*`, `team.wet-{aiming,swim,walk}.*`) and dumping the still moments found the
pilot's faults: its STUCK and LOST checks ran only `onground`, and a swimmer is never on the
ground, so a bot knocked into a pool walked a link 100-220 units above it for seconds; a pool's
floor node was reached only standing on it, so a swimmer 40-70 over it circled; and swimmers
pressed into a channel's lintel at zero speed. Before tuning a water behaviour, grep the pilot
for `onground` and ask what each gated check does to a swimmer. Diving to each floor node (as the
swim mode pitched at its node) was slower than leaving the floor alone: on a pool's floor ground
friction adds to the water's, so swim level just off it and dive only where a box trace at that
height is blocked. And when another view takes the hands (the aim, the calm view), turn the
pilot's wish under it in three dimensions in water (`turn_axes_swim`), or the dive is lost.
`se-people --samples=<tsv>` writes every sample (place, wet, eyes under, speed, faced) for
questions the metrics do not answer: where people are wet, and how long they stay.
