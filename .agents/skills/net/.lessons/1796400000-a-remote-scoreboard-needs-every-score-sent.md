---
title: A remote scoreboard sees a score only when the endpoint sends it
date: 2026-11-04
scope: games/shooters_edge/net/classic_server.cpp, games/shooters_edge/net/classic_signon.cpp
concepts: [classic-protocol, updatefrags, scoreboard, bots, local-seat, dedicated-server]
---

The classic endpoint sent `svc_updatefrags` only from `full_client_update`, which runs when a
player joins or leaves. A remote client's scoreboard therefore showed 0 for everyone for the
whole match. The listen server hid this because its client reads the frags in-process. It
showed up only once bots on the dedicated server fought and the `se-dedicated-lobby` lab read
the scoreboard from a client. The frame now sends every score that moved (`tell_frags`).

Any new seat kind (a bot, a spectator) must be checked from a remote client's side. Read what
the client keeps (`ClassicClientEnd::frags`, `userinfos`), not the server's own state.
