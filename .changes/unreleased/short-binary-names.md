---
type: Changed
title: Shooter's Edge's programs have short names
---

Shooter's Edge's executables are now `se` (the game's window), `sesv` (the dedicated server; `sesv -game 2` serves the sequel's rules on its own protocol), `sefwd` (the forward), `seproxy` (the proxies; `seproxy near` and `seproxy far` for one end each) `setv` (the TV node; `setv -game 2` for the sequel's), `sehub` (the hub), `sebots` (a bot team) and `secli` (the player's command line), all at the build's root. They replace `shooters_edge`, `shooters_edge_server`, `shooters_edge_sequel_server`, `shooters_edge_hops <kind>`, `shooters_edge_hub`, `shooters_edge_bots` and `shooters_edge_cli`; `-game 1` (the default) or `-game 2` picks the game on `sesv` and `setv`; the flags are unchanged.
