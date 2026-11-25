---
type: Added
title: QuakeWorld's captain mode
---

Our QuakeWorld server plays captain mode before a team match: a player types `captain` and the others elect him with `yes` (at least two votes and `k_vp_captain` percent of the players, 60 seconds); with two captains the rest are numbered (the number shown as their frags) and picked in turn by the number keys, first captain once, then the second twice, then one each, the last player going by himself. A pick takes the captain's team name and colours. Nobody fires or changes team while the captains pick, and a team name cannot change during a match. The engine gains one election for any role (a captain, a coach, an admin) and a turn-by-turn draft, so every game can use them.
