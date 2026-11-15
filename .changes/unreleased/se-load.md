---
type: Added
title: Shooter's Edge's bots as the load on many of its servers
---

The `se-load` lab plays matches of the game's bots, each a classic-protocol client over the simulated network, on many servers for each count of players, and times each server's frame apart from the bots' thinking: mean, p99 and worst against the players, the bytes on the wire, and how many servers one core holds. `TeamMatchOptions::clock_ns` times a team game without changing it.
