---
title: Compare the bots with people in the same team size; dm2's recordings are 2on2
date: 2026-11-19
scope: games/shooters_edge/labs/se_team_lab.cpp, plugins/shooterbots/bot.cpp
concepts: [people, recordings, team-size, 2on2, 4on4, se-team, size, aim, fast-flights, bearing, confound]
---

People's dm2 recordings (the se-moves track the bot plan reads) are 2on2 (3-4 players a demo); the
bots' sets were 4on4. Twice the enemies put one within 500 units in 53 % of the bots' fast flights
against people's 28 %, and the bots looked "twice as keen to aim at speed" (49 % of fast flights
against 26 %). Two switches were built on it (`fastway`: the aim given up at a fast takeoff with
the enemy 45+ degrees off the velocity) and both lost or did nothing. Played as a 2on2
(`se-team --size=2`), the bots aim in 30 % (people 29 %), band by band alike, and their distances
to the nearest enemy match. Before reading a gap off people's recordings, count the players a demo
and play the bots in the same size; a measure that depends on the enemies' number (distance,
sight, aim, bearing) is meaningless across sizes.
