---
type: Added
title: The server browser lists servers that announce themselves to a community master
---

JOIN A GAME asks the community masters in `sb_hubmasters` (the master `sesv +set sv_master <host>` beats to, which a hub reads) for Shooter's Edge's servers and lists them beside the classic masters' and the LAN's. `ge::net::ServerListConfig` gains `own_masters` and `ge::net::ServerQuery` gains `protocol`, the game's id at the engine's masters.
