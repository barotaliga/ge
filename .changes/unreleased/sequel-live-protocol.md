---
type: Added
title: The sequel's live network play, client and server
---

Shooter's Edge speaks the sequel's live protocol: a scripted client (`net/sequel_client.hpp`) joins a server through its whole signon, reads every packet as a block of the sequel's own recording (so it records as it plays), and sends moves that acknowledge the newest frame it read; our sequel net server (`net/sequel_net_server.hpp`) serves the sequel's rules to such clients, the signon by the client's own commands, frames as deltas from what each client acknowledged, prints and changed settings on the reliable channel. The `se-sequel-net` lab runs both on the simulated network, or the client against a live server with `--server=`.
