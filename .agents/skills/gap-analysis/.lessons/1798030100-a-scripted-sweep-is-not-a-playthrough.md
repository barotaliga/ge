---
title: A scripted co-op sweep is not a playthrough; report which check a result is
date: 2026-11-21
scope: games/shooters_edge/labs, porting/shooters-edge
concepts: [sweep, playthrough, scripted, plumbing, bot, report, coop]
---

The Quake II co-op sweep reported "39 of 39 maps played through" while the bot's playthrough was
stuck on the first map. The sweep puts each player on every key and then on the exit: it checks
co-op's plumbing (joins, deaths, rejoins, carried bodies, level changes), not that anyone can get
through a map. The owner read it as a playthrough. Word each result by what it checks ("pass the
scripted co-op check"), and keep a bot's or a recorded playthrough as the separate proof that a
map can be played.
