---
type: Added
title: C++ mods on Shooter's Edge's sequel maps
---

A C++ mod package now stacks on the sequel's rules as it does on the classic ones, from `sv_mod` or a runner: its hooks run at the sequel's own points (the 100 ms rules frame, each command, every spawn, connect and disconnect), the hook table grows `damage` (answer the points a blow deals) and `death` at its end (an older mod still loads), and the services reach the sequel's players and map entities. A sample mod, `vampire`, heals a player by half the points of each blow he lands, on either game's rules.
