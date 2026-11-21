---
title: Grep a command's name across the tree before adding it; the last add_command wins silently
date: 2026-11-21
scope: engine/console/console.cpp, games/shooters_edge/client/team_chat.cpp
concepts: [add-command, duplicate, override, macrolist, registration-order]
---

`Console::add_command` replaces a command of the same name without a word, so an engine built-in
added in the console's constructor is overridden by any game file that registers the same name
later (Quake II's `macrolist` written in `console_more.cpp` was shadowed by the team-play
`macrolist` in `team_chat.cpp`, and its test passed only because the test console had no game).
Before adding a command, `git grep 'add_command("<name>"'` across engine, plugins and games; when a
game already has it, extend that one or settle which game's words win, never write a second.
