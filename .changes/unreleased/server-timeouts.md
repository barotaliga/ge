---
type: Added
title: Timeouts in a server's match
---

A player in the match types `timeout` and the game stops for everyone for `sv_timeoutlength` seconds (60), then counts `sv_timeoutresume` seconds (5) back to play; the side that called it can type `timein` to resume sooner. Each team, or each player without teams, has `sv_timeouts` a map (1; 0 turns them off). Every game on the engine's server core gets them (`engine/server/timeout.hpp`); Shooter's Edge's servers take them now.
