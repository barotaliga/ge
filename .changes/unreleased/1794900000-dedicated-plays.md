---
type: Changed
title: The dedicated server plays the real game
---

`shooters_edge_server` now serves Shooter's Edge's own deathmatch (the baseline mod) over the classic protocol on its port, so a classic client connects to it and plays; before, its slots were filled by stand-in bots and nobody could join. The engine's server core hands the port to a game that speaks its own protocol (`ServerGame::serve` and `net_frame`), keeping the frame clock, the layered `server.cfg` and the flags its own. The server takes `--root` for the folder its maps are read from. Bots on it come with the engine's lobby. The `se-dedicated` lab has classic clients join it under each network preset.
