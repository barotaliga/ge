---
title: msg_trigger fires only on lines the game feeds it, each with its level
date: 2026-11-22
scope: engine/console/console.hpp, games/shooters_edge/client/chat_ignore.cpp
concepts: [msg-trigger, fire-triggers, trigger-level, triggermatch, chat-lines]
---

The console only stores `msg_trigger`s; nothing fires them until a game calls
`Console::fire_triggers(line, level, team)` with each printed line. Before the common teamplay
PR no game line was fed at all, so every trigger in a player's config was silent and no test
said so. The shell feeds them in `drop_ignored` (after the ignore lists, so an ignored line fires
none), with the server print's level (`kTriggerLow`..`kTriggerChat`, `kTriggerTeam` for team
lines). The window's own listen game's lines are not fed yet. A line carrying an `f_` query word
fires none, so two clients answering each other cannot loop.
