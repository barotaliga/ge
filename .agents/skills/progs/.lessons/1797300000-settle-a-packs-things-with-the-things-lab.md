---
title: Settle a pack's entity rows with se-campaign --things, by the map's class name
date: 2026-11-23
scope: games/shooters_edge/labs/campaign_things.hpp, games/shooters_edge/server/qc_server.hpp
concepts: [things-lab, spawned-as, mission-pack, verify-rows, level-change, use-targets]
---

A bot census tables the classes the program names at run time, and a pack renames what it spawns
(func_exploder becomes `exploder`), so ledger rows keyed by the map's class name do not line up.
`QcServer::spawned_as` keeps the map's name per entity, and `se-campaign --mod=<pack> --things=1`
watches every placed thing by it: about four minutes a map on debug, against 15 to 30 for the bot
census. Two traps found on the way: a used name or a touched trigger can end the level, and
`campaign_frame` then replaces the server under any reference you hold (ASan: heap use after
free), so the lab steps `c.server.frame` itself and clears `pending_map`; and the weapons must be
fired before anything is used, or an exit's intermission freezes the player and no shot leaves.
It is a scripted look, not a playthrough: rows that need the counter walked, a well's pull or a
coil's zap seen stay `verify` with what was seen.
