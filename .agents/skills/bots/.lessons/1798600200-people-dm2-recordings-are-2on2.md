---
title: Compare bots in the people's match size before any per-player number
date: 2026-11-20
scope: games/shooters_edge/labs/se_people_lab.cpp, games/shooters_edge/labs/se_team_lab.cpp
concepts: [2on2, 4on4, size, per-player, se-people, se-team, yardstick, launcher-carried]
---

People's dm2 recordings are 2on2 (their file names say `2on2_...[dm2]`); a session compared them
with 4on4 bot sets all day. Two launchers among four players or among eight halve a player's
carry (people 57 %, 4on4 bots 27 %, 2on2 bots 41 %); bursts, deaths and fights a player-minute
move the same way. Rates per carried minute or per in-view minute moved less but still moved.
Before any per-player comparison, read the recordings' size (`ls <demos>/<map> | head`) and run
`se-team --size=<n>` to match; keep the 4on4 sets for head to head only.
