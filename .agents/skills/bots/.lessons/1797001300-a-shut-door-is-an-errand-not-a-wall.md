---
title: A door only a button opens is an errand on the way, not a wall and not a wait
date: 2026-11-09
scope: games/shooters_edge/bots/fighter.cpp, games/shooters_edge/bots/buttons.hpp, plugins/shooter/nav.cpp, plugins/shooter/features.hpp
concepts: [door, button, locked, shut, errand, reach, no-route, mover-in-the-way, dm2, teleporter]
---

dm2's bots stood still half their time: `a mover in the way` 15-27 % of all commands, at doors
only a button opens (`func_door` with a `targetname`). Read who waits there before guessing why:
the trace's `goal` showed they were going for the QUAD and the LAUNCHER through a teleporter behind
the yellow armour's doors, not for the armour, so the items' button logic (`ItemSpot::button_*`)
never ran for them.

Taking every such door off the routes (`pilot.shut`, as the coop companion does) traded the waits
for `no route` 56-83 %: on a deathmatch map those doors ARE the ways. What works is what a person
does: when the planned route crosses one resting shut, go to its button round the door, press or
shoot it, plan the goal again. Three traps on the way: a button behind its own door (reached only
through it: leave the door off the routes a while and off the REACH search too, or the goal stays
"reachable" and the bot stands planning in vain); a door a walked-into trigger opens is not shut
(leave those to the pilot's wait and bar); and the campaign leader and the companion keep their
own door lists (`way_through`, `companion.cpp`), so a new rule that writes `pilot.shut` or the
reach must leave them alone or their goldens move.
