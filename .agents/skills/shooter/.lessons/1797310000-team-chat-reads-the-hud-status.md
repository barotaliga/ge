---
title: Team chat's codes read the HUD's status, so every way of playing has them
date: 2026-11-14
scope: games/shooters_edge/client/team_chat.cpp, games/shooters_edge/client/main.cpp
concepts: [team-chat, percent-codes, hud-status, event-triggers, main-cpp-line-cap]
---

The window already builds a `hud::Status` once a frame for every way of playing: its own
server, a server it joined and a demo. Read the player's state from that. A code such as `%h` or
an event such as `f_death` then works in all three with one `team_frame` call where each status
is built, and no reading of protocol stats is needed. `client/main.cpp` is near the 2000-line
cap. Move a block out of it into its own file (the chat commands went to `team_chat.cpp`) before
you add a call to it.
