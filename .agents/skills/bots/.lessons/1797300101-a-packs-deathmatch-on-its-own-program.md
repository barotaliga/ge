---
title: A pack's deathmatch runs on the campaign's server, the fighters on the map's nav
date: 2026-11-24
scope: games/shooters_edge/bots/pack_fight.cpp, games/shooters_edge/labs/se_pack_dm_lab.cpp
concepts: [pack-fight, deathmatch, mission-pack, map-nav, items-taken, spawned-as]
---

`run_fight` is the test arena and the baseline mod only; a real map under a pack's own progs.dat
needs the campaign's server (`campaign_new_game` with `deathmatch`, `coop 0`) and the nav from
`build_map_nav` with `info_player_deathmatch`. Dead fighters must press attack every other command,
or Quake's rules never stand them again. Count pickups from the server's entities by
`QcServer::spawned_as` (weapon_ and item_ classes), not the fighters' item list: the fighters know
only the base game's arsenal, so a pack's own weapons (the laser cannon) are taken only by chance
until the bots learn them (M63e).
