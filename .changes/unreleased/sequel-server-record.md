---
type: Added
title: The sequel's server records its matches
---

Shooter's Edge's sequel server fills the server core's broadcast and watch hooks, so `sesv -game 2 --record=PATH` keeps its matches and every player's view plays back as it was seen, from the start or after a seek. Each frame carries every slot's state whole (about 1.25 kB a second for two players), so a keyframe is the same bytes. The `se-server-record` lab now runs both games.
