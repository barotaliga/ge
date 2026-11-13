---
title: A client's ask to the master runs on the wall clock, apart from its tick clock
date: 2026-11-13
scope: games/shooters_edge/net/classic_remote.cpp, engine/account/client_login.cpp
concepts: [autologin, client-login, ticket-code, accountkey, tick-clock, sim-network]
---

The window's `Remote` keeps a tick clock that starts at 0 on every join, so a session over the
simulated network replays. The account ask is different: a ticket's life and the master's resends
are wall-clock things. So `RemoteLogin` takes its own `unix_us` function and its own socket
(`link`, or one it opens). It never shares the game's transport, because `client_tick` reads every
datagram on that one.

In a test or a lab, the master's sim transport must be advanced and updated on that same wall
clock (`kUnix0 + server now`). The se-login lab already ran `service_update(master, g_unix)` on a
network advanced by server time, and that works.

On the classic wire, a ticket would log anyone in until it runs out. So the client trades it for a
one-time code (`Op::ticket_code`) and sends the line a player would type. The server needs nothing
new except the `*accountkey` advert.

Two players in one lab logged in to the same account on the same server made the second one look
broken. Give each player in a lab its own account.
