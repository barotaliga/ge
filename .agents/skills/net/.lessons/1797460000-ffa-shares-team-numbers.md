---
title: In a free-for-all every player may carry the same team number
date: 2026-11-13
scope: games/shooters_edge/net/classic_stats.cpp
concepts: [weapon-stats, team-damage, teamplay, match-stats]
---

The classic rules set a player's `team` field from their colour whether or not teamplay is on, so
bots in a free-for-all often share one team number. Anything that tells a team mate from an enemy
by `team` alone books ffa damage as team damage and loses kills. Read the `teamplay` cvar first:
off, everyone else is an enemy.

A report with a server's worth of weapon columns (16 players, 54 columns) passes the 1472-byte
datagram; the account service joins it from pieces (`geAC` 7) before checking the signature. Test
a big report whenever the columns grow.
