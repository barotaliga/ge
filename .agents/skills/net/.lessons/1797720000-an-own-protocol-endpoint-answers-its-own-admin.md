---
title: A game with its own protocol answers its own admin words; the engine keeps the list
date: 2026-11-18
scope: games/shooters_edge/net/q2_admin.cpp, engine/server/bans.hpp
concepts: [rcon, ban-list, admin, endpoint, listip-cfg, filterban]
---

The engine's admin features (`server/admin.hpp`) hook the core's own session, and a game that
serves its own protocol (the classic and Quake II endpoints) has none, so its `rcon`, `kick` and
`status` are the endpoint's to answer in its game's words. What two endpoints would write alike
is the engine's: the list (`ban_command`, `ban_refuses`) and its file (`ban_file_read`,
`ban_file_write`). Rebuild an rcon line from its words and log it without the password word, and
take the rules' printed lines from `RulesCommon::console` grown during the call.
