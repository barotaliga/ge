---
title: A second rules runner mirrors its world into sv.edicts and spends a forced turn once adopted
date: 2026-11-11
scope: games/shooters_edge/server/sequel_server.cpp, games/shooters_edge/net/classic_game.cpp, games/shooters_edge/client/listen.cpp
concepts: [sequel, rules-runner, sv-edicts, mirror, delta-angles, forced-view, listen, spin]
---

`RulesWorld` is the runners' shared shape, but the classic endpoint (`classic_server.cpp`'s
baselines), the listen's frame (`sv.clients[0]`, `sv.edicts[1]`) and the hit tests still read the
loop's pool by slot. A runner that keeps its own state (the sequel's) must size `sv.edicts` and
`sv.clients` at the map's start and mirror its world rows into them after every call, or the host
crashes on the first frame. The listen adopts the server's view whenever `v_angle` differs from
the angles it sent (`l.forced`); a runner that turns players by delta angles must zero the delta
once the client sends the turned view back, or the turn is added every frame and the player
spins (visible as a walk of ~7 units/s whose direction rotates). Keep the native rule behind an
option (`clients_take_view`) so the runner's golden stays the native model's.
