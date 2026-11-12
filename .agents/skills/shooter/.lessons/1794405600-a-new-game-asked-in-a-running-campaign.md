---
title: A new game asked for while a campaign runs must restart the host, not change its map
date: 2026-11-11
scope: games/shooters_edge/client
concepts: [coopgame, newgame, sv-coop, new-game, start-map, listen-change, listen-campaign, addbot]
---

`start_map` (game_folder.cpp) used to send every map asked for in a running campaign to
`listen_change`, which moves the host to the map and keeps the server's cvars and the party, so
`coopgame` typed after the menu's COOPERATIVE (which ran the campaign with `coop 0`) changed the
map and nothing else: `addbot` stayed refused. A command that starts a NEW game on new settings
(`newgame`, `coopgame`, `sv_coop 1`) sets `ListenOptions::new_game`, and `start_map` then calls
`listen_campaign`, which builds the host afresh from `opt.console` and seats `opt.companions`.
Prove it end to end with the campaign stub and `start_map` (the game folder test), not only the
lines the command appends.
