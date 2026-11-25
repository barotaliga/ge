---
type: Changed
title: Shooter's Edge's programs have short names
---

Shooter's Edge's executables are now `se` (the game's window), `sesv` (the dedicated server; `sesv -classic q2` serves Quake II's rules on its own protocol), `sefwd` (the forward), `seproxy` (the proxies; `seproxy near` and `seproxy far` for one end each), `setv` (the TV node; `setv -classic q2` for Quake II's), `sehub` (the hub), `sebots` (a bot team) and `secli` (the player's command line), all at the build's root. They replace `shooters_edge`, `shooters_edge_server`, `shooters_edge_q2_server`, `shooters_edge_hops <kind>`, `shooters_edge_hub`, `shooters_edge_bots` and `shooters_edge_cli`; `-classic q1` (the default) or `-classic q2` picks the game on `sesv` and `setv`; the flags are unchanged.
