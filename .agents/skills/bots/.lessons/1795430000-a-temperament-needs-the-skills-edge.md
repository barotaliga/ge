---
title: Weigh a fight by might and the skill's edge, and cap a retreat's hysteresis below full health
date: 2026-11-01
scope: plugins/shooter/temper.cpp, games/shooters_edge/bots/fighter.cpp, games/shooters_edge/bots/temper.cpp
concepts: [temperament, takes-fight, might, self-edge, retreat, hysteresis]
---

A fight weighed by might alone (body times arms against the enemy's) made balanced experts as
careful as novices, and experts fell below novices in frags: an expert's aim is worth doubling his
might (`self_edge`), so a fresh enemy is one he takes with the starting gun.

A retreat that ends at its threshold plus a margin never ends when that sum is above full health:
cowards turning at 70 with a margin of 30 stayed in retreat for the whole match. Cap the way out at
100 (`std::fmin(below + 30, 100)`).

An outmatched bot that goes only for health stands still where there is none; let it take any item,
penalising the ones nearer the enemy, and fall back on its nearest teammate.
