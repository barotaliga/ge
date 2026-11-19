---
title: A game loaded under a running window keeps its players' bodies only if the reconnect takes them over
date: 2026-11-15
scope: games/shooters_edge/server/q2_save.cpp, games/shooters_edge/server/q2_server.cpp, games/shooters_edge/client/listen.cpp
concepts: [load, restored, reconnect, sequence, digest, contract]
---

Quake II's save holds every slot's client record and body; the window then syncs its host,
which connects slot 0 again, and a plain connect builds a fresh body: 0 of 299 frames matched.
Mark the restored slots (`Q2Server::restored`) and let the connect take a restored body over.
The window also numbers its packets from 0 on every map, so the rules reset each player's
`last_sequence` at every map start, and a load lifts the window's sequence to the saved one; a
player whose moves are all "old" stands frozen with no error. Hold the contract with a digest of
the whole game (`q2_digest`) compared frame by frame against the game never stopped, in a
test and in the lab, on the real maps too: the fixture hall missed both faults.
