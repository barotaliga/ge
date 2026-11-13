---
title: Open a server's accounts in one place, for every kind of host
date: 2026-11-13
scope: engine/server/accounts.cpp, games/shooters_edge/client/listen.cpp, games/shooters_edge/net/classic_coop.cpp
concepts: [server-accounts, listen-server-login, coop-login, own-master]
---

`server/accounts.hpp` opens what `sv_login` asks: the server's own service, or a socket to its
master. The dedicated process and the window's listen both use it, so a third kind of host only
calls `server_accounts_open`.

Two places on the listen path quietly turned logins off:

- `classic_game.cpp`'s `serve` opened the login only when `game.host == nullptr`, which meant only
  on a dedicated server. The window's player is a local seat, and the login already skips local
  seats, so that guard was not needed.
- The coop campaign runs on its own door (`classic_coop.cpp`), not on `classic_game`. It needed its
  own `logins` call in `CampaignDoor`, and its `game_command` set to send lines to the login.

In a test, the login prompt is told only on the frame after a client spawns, so run a few frames
before you look for it.
