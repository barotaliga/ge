---
type: Added
title: Quake II's live network play, client and server
---

Shooter's Edge speaks Quake II's live protocol: a scripted client (`net/q2_client.hpp`) joins a server through its whole signon, reads every packet as a block of Quake II's own recording (so it records as it plays), and sends moves that acknowledge the newest frame it read; our Quake II net server (`net/q2_net_server.hpp`) serves Quake II's rules to such clients, the signon by the client's own commands, frames as deltas from what each client acknowledged, prints and changed settings on the reliable channel. The `se-q2-net` lab runs both on the simulated network, or the client against a live server with `--server=`.
