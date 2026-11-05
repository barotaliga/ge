---
title: A listen server's own player must hear each line once
date: 2026-11-05
scope: games/shooters_edge/net/classic_server.cpp, games/shooters_edge/client/listen.cpp
concepts: [listen-server, local-seat, local_tap, local_print, route-events, lobby]
---

The window's player is a local seat of the classic endpoint. The endpoint hands the window the
rules' frame (`local_tap`) before it routes it, and it hands over every line printed to a local
seat (`local_print`). Routing a print event to slot 0 goes through `client_print`, so a tap that
also read the print events made every line appear twice. Read prints from one door only: the
routed one, the same door every remote client gets them through. Read from the tap only what
no print carries (centre prints, effects, blows).

The lobby's answers are announced after the frame that runs the lobby. A player's word between
frames reaches him a frame later, so a test checks the answer after one `listen_frame`.
