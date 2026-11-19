---
type: Added
title: The sequel's server can hold back a player's moves past his time budget
---

With `sv_speedcheck 1` (the sequel's `sv_enforcetime` is its synonym; off by default, as in the sequel) the sequel's rules give each player 1800 ms of moves every 16 frames and do not run a move past it, so a client that sends time faster than the clock does not move faster.
