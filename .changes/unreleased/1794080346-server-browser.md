---
type: Added
title: An in-game server browser
---

MULTIPLAYER ▸ JOIN A GAME in Shooter's Edge lists servers on the LAN, at the masters (`sb_masters`, and those the loaded install names) and the player's own (`sb_servers`), ours or any other classic server: name, mode (coop, team, ffa), map, players and slots, ping and skill, sorted by any column; ENTER joins. The engine's `net/server_list.hpp` does the querying for any game that hands it its status protocol, and `resolve_ipv4` resolves host names (`connect` takes them now). Our server's info now carries `map`, `deathmatch`, `teamplay`, `coop`, `skill` (where monsters play it) and `needpass`.
