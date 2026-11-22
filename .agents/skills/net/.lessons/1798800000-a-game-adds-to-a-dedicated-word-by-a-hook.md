---
title: A dedicated-console word every game shares takes the game's part by a ServerGame hook
date: 2026-11-22
scope: engine/server/main.cpp
concepts: [dedicated-server, console, quit, recycle, showip, macros, server-game, hooks]
---

A word the engine's dedicated process registers itself (`quit`, `recycle`, `showip`,
`$com_maplist`) never reaches the game's `console` hook, which only sees words the core does not
know. When a game has its own part in such a word (Quake II's farewell text, its stream listener's
socket, its maps), add an optional function to `ServerGame` (`quit`, `sockets`, `maps`) and call
it from the word, rather than letting the game register the same word: one word, one place, every
game's part handed in. Words whose lambdas read the run's sockets or `Server` belong in the run
function beside `quit`, not in `main_body`, so they never outlive what they capture.
