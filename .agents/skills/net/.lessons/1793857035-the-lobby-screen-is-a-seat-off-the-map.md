---
title: A player on the lobby's screen is a seat with no client on the server
date: 2026-11-05
scope: engine/server/match.cpp, games/shooters_edge/client/listen.cpp
concepts: [lobby, warmup, lobby-screen, warm_on_join, listen-server, connect, drop]
---

Keeping a player off the map while he sits on the lobby's screen is done by not connecting him
to the rules at all (`rules.drop(0)`, `connect` again on `warmup`), not by a spectator flag: the
baseline mod has no spectator, and a connected but idle player is a free kill for the bots. The
seat stays in `ge::server::Lobby` (ready, team), so the countdown and the match reach him, and the
match's map restart connects every seat. Every place that reads slot 0 must then tolerate an
inactive client: the frame's packet list (send none for him), the forced-view check, the frags.

A dedicated server's clients have no lobby screen, so `LobbySettings::warm_on_join` defaults on
and `sitout` is refused there (`no_screen`); only a game with its own screen turns it off.
