---
type: Added
title: Bots and the lobby on the dedicated server
---

`shooters_edge_server --bots=N` seats N bots that every connected player sees on the scoreboard, with their teams and frags. `--lobby=1` (the `sv_lobby` cvar) opens every map in warmup with the frag and time limits held. Players type `ready`, `break`, `team red|blue|auto|switch`, `start` or `lobby` in their console, and the leader can `addbot [novice|average|expert]` or `removebot`. When everyone is ready, a 5 s countdown restarts the map into the match, and the next map opens in warmup again. Bots alone wait for a player. A remote player's scoreboard now shows frags as they change; before, it only updated when someone joined or left.
