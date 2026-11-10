---
title: An item left lying is broken down by what the bots made of it before its worth is tuned
date: 2026-11-08
scope: games/shooters_edge/net/classic_team.cpp, games/shooters_edge/labs/se_team_lab.cpp, games/shooters_edge/labs/se_people_lab.cpp, games/shooters_edge/bots/fighter.cpp, plugins/shooter/pilot.cpp, games/shooters_edge/bots/map_nav.cpp
concepts: [item-control, lying, untaken, armour, launcher, firepower, shots-per-min, owned, nav, tread, grid, run-up, failed-jump, pit, se-team, se-people]
---

Bots lost dry fights because they fought with the scattergun at 61 health and 12 armour while
people fought with the launcher at 98 and 29 (`se-people`: `owned.<weapon>`, `health`, `armour`,
`shots-per-min.<sound>`; people's recordings keep neither the active weapon nor the view model up
to date, so what is fired is read off the weapon sounds). The fix looked like item control, and the
first guess at why the yellow armour lay 72 % of the match was the plan's own: a holder standing
on it. `se-team`'s `team.lying.<item>` breakdown (nobody believed it there, nobody wanted it, the
one who did was fighting, one on his way far or near, one standing on it) said otherwise: on it
0-2 %, on the way two thirds of the time. The way failed, not the want. Following one bot's
route found it: the nav grid (32 units) stepped over a stair tread narrower than itself, so one
side of the armour's walkway had no way up and the route took a long jump across a pool instead.
Two fixes aimed round that (forgetting missed jumps, cutting the jumps over pools) each cost a
frag a minute and moved nothing; the fault is the missing node. Before tuning an item's worth or
the bots' timing, break its untaken time down by cause, and when it is "on the way", follow one
route node by node (a temporary print of the pilot's next links) and ask people's recordings
(`se-rj --spot=`) how they get there.
