---
title: The lobby's keys are console words already; a recipe needs no new command for them
date: 2026-11-23
scope: games/shooters_edge/client/lobby_console.cpp, engine/server/listen_console.cpp
concepts: [recipe, lobby, togglewarmup, listen-server, sv-cvars]
---

A Quake II window opens its own listen game on base1's lobby, and a recipe that took pictures of
the map sat on the lobby's screen: `sv_lobby 0`, `start` (two players needed) and `addbot` (no bot
world on base1) all failed. Every lobby key is a console word (`lobby_keys` sends them):
`togglewarmup` is F4, `toggleready` F1, `team switch` F2, `addbot`/`removebot` F5/F6, `start`
ENTER. Look for the word a key sends before adding a recipe command for it. Server settings take
`sv` in front (`sv sv_cheats 1`).
