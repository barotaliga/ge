---
type: Added
title: QuakeWorld's admin in the game
---

Our QuakeWorld server's players can run a match as its admin, on the engine's referee in QuakeWorld's own words: `admin <code>` against `k_admincode` (`k_admins 0` turns admins off; a wrong code holds the next try 5 seconds), `forcebreak`, `forcemap <map>`, `kick <id or name> [reason]` (the address kept out 30 seconds), `forcestart` in the lobby's warmup (a 10-second count, then whoever is not ready is dropped) and `lock` (nobody joins as a player for 15 seconds). The engine's referee takes a game's own words and lines (`RefereeWords`), so each game keeps its own; Quake II's are unchanged.
