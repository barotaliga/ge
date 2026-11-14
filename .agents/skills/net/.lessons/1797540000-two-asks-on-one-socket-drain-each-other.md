---
title: Two account asks reading one socket drain each other's answers; run them in turn
date: 2026-11-14
scope: games/shooters_edge/net/classic_remote.cpp
concepts: [client-presence, client-login, ask-read, shared-transport]
---

`client_login_update` and `client_presence_update` each read every datagram waiting on their
transport and hand it to their own `ask_read`, which drops what is not its answer. In the game
each has its own UDP socket, but a test or lab passes one simulated link (`RemoteLogin::link`) to
both, and whichever reads first eats the other's answer. The remote's presence therefore waits for
the login's ask to end (`l.done`) before its first ask; keep any third ask on that link in turn too.
