---
title: Holding an area is measured as kept by a team until an enemy reaches it, not by distance
date: 2026-11-19
scope: games/shooters_edge/labs/areas_read.hpp, games/shooters_edge/labs/se_people_lab.cpp
concepts: [key-areas, holding, keeper, covered, open, contested, closeness, se-people, maintainer-rule]
---

The maintainer: a holder may stand outside his area, knowing nobody comes, to meet whoever does;
not being on the item is not losing it. Read an area off a recording (people's or the bots' alike)
as KEPT by the team that last reached it alone until an enemy reaches it alone; COVERED while one of
the keepers is within 768 units, OPEN (unsafe) while none is; CONTESTED with both teams in it
(se-people `people.area.<item>.*`, `labs/areas_read.hpp`). The holder's distance to his claimed item
(700+ off 72 % of the time it lay) had read as neglect; by the area the bots keep most areas as
people do and leave the RA, RL and mega OPEN two or three times as often.
