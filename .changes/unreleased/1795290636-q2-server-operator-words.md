---
type: Added
title: The server's operator words, player queries and connect checks for every endpoint
---

`engine/server/operator.hpp` gives any game's server the operator's way of naming a player (a slot, the exact name, or with `sv_partialnames` a name in any case or a unique part of it, each refusal in the server's words), a line's raw rest, and the connect and begin stuff lists (`addstuffcmd`, `delstuffcmd`, `liststuffcmds`). The classic layer adds the strict user info rule (`info_strict`) and `listmasters`. Quake II's server mounts them with `status` and its letters, `kickban`, `stuff`, `stuffall`, `printall`, `stuffcvar`, `killserver`, `pickclient`, `dumpents`, the player's `info`, `sinfo`, `lag`, `packetdup`, `stopdl` and `nogamedata`, a version probe, a forced reconnect and the connect's user info checks.
