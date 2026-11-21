---
title: A map may hold two of a key item; read areas by spot, and check the nav can enter each
date: 2026-11-19
scope: plugins/shooterbots/mind.cpp, plugins/shooterbots/map_nav.cpp, games/shooters_edge/labs/areas_read.hpp
concepts: [item-control, key-areas, dm2, two-red-armours, first-of, south-complex, nav, touch-plate, start-open]
---

dm2 has two red armours, two launchers, two megas and three yellow armours. The team plan
(mind.cpp `best_armour`, `rocket_launcher`: `first_of(kind)`) knows only the first of each, and
by kind the item numbers average a spot the bots keep well with one they never enter: the ledge
RA lay 97 % of the time a bot claimed it, the other was kept better than people keep it. se-people's
table `each key area by its spot` splits them. The ledge (and the whole south complex) cannot be
entered in the bots' nav: map_nav takes a start-retracted plate with no name for one that "pulls
out when touched" and closes every link into it, though a touch runs such a plate OUT; people go
in by a curved strafe jump over the plates' pit. When an item lies, ask se-reach and a route test
from outside (nav_route between rooms, with the bars the pilot keeps) before tuning the bots.
