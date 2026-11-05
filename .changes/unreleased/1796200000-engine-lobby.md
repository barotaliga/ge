---
type: Added
title: The lobby before the match is the engine's
---

The lobby before a match (seats on two teams, `ready`, `break`, `team`, the leader's `start`, warmup, the countdown, the match and back to warmup) now lives in the engine's server core (`engine/server/match.hpp`, `ge::server::Lobby`), for any game with matches. Shooter's Edge's lobby runs on it unchanged and adds only what a team means in its game (the colours and the mod's team). The `server-lobby` lab runs it on its own: everyone ready, a break, and the leader's early start, each replayed to the bit.
