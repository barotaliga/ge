---
title: A module reads the server it runs on; a cvar it reads and the server lacks is a silent bug
date: 2026-11-06
scope: games/shooters_edge/server/qvm_server.cpp, games/shooters_edge/server/qvm_calls.cpp
concepts: [qvm, cvar, cvar-string, rate-cap, version, text-cvar, module-server, interop]
---

A compiled match mod reads settings the server is expected to have, not ones it registers:
`sv_maxrate` (it checks every client's `rate` against it; at 0 it flips a client between 0 and
its minimum forever), `version` (its about page), and the description cvars it sets at init
(`cvar_set` on an unknown name is dropped). Register each as the server's, and register text
ones with `CvarKind::text`: a cvar's kind defaults to number, which turns "Shooter's Edge 0.1"
into "0" with no error. The quickest way to find the next missing one is a real client on the
server: `se-qvm` shows the module ran with 0 faults, but only the client's screen shows the
wrong text or the looping print.
