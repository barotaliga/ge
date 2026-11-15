---
title: A stock client of the sequel drops keys held from its command line when it connects
date: 2026-11-15
scope: games/shooters_edge/server/sequel_native.cpp, games/shooters_edge/net/sequel_net_server.cpp
concepts: [black-box, stock-client, sequel, native-game-library, bot-input]
---

To make an unmodified client walk and fire against our server, `+forward +attack` after
`+connect` on its command line does nothing: the server saw only empty moves. Its connection
clears the held keys. A cfg in the client's own home folder (`~/.yq2/baseq2/go.cfg`, run by
`+exec go.cfg`) that waits a few thousand frames (`alias w100 "wait;...";` chained) and then
holds the keys works, but the waits also hold back the server's signon commands queued behind
them: the client enters the game only after the waits, so take the screenshots later (about
50 s in, not 20).
