---
title: A view the runner fills is stale after any call that is not a frame
date: 2026-11-04
scope: games/shooters_edge/server/rules_world.cpp, games/shooters_edge/server/commands.cpp, games/shooters_edge/net/
concepts: [rules-world, fill-world, rules-runner, commands, connect, save-load, digests]
---

The wire reads the rules' world between frames too: a client's command (`kill`, `team`), a console
line, a connect or a save's load each change frags, names or the whole pool before the next send.
Fill the world at the end of every public entry point of the server (`qc_frame`, `load_map`,
`connect`, `disconnect`, `client_command`, `set_user_info`, `server_command`, the save loads), not
only at the frame's end, or a reader moved onto it differs from the program by one call and the
digests drift. Guard the fill before the first map: the VM's globals are empty until `load_map`
creates the program, and a console line can run before it.
