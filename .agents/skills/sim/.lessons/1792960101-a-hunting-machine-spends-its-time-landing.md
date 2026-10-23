---
title: An army helicopter in a test spends most of its hunt landing, not circling
date: 2026-10-23
scope: games/fall_line/sim/army_door.cpp, games/fall_line/sim/army_heli.cpp, tests/fall_line_army_door_test.cpp
concepts: [army, helicopter, door-gun, pylon-turn, echelon, landing, overwatch, staging, test]
---

From two stars a lone machine carries men, so it flies in, lands them and climbs back out: on the
flat strip it is on its pylon turn only from about 80 s, and a man running fast is chased by
landings ahead of him rather than flown in echelon. A test or a lab that reads what the machine
does on its circle (the door guns' line, the echelon's crab) has to run past the landing or
stage four stars, where the first machine stays up as the overwatch and never lands. Check the
machine's mode over the run (`ArmyHeli::mode`) before reading anything off its station.
