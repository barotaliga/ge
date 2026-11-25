---
type: Added
title: The broadcast camera follows the play
---

Shooter's Edge's classic server takes `ptrack <slot>` from spectators: the tracked player's stats, weapon frame and record go to his tracker wherever he is, a bad slot is refused, a bare `ptrack` stops it. The watcher behind `setv --watch` rides the top fragger with it (`games/shooters_edge/net/qw_follow.hpp`), and a spectator's client recording plays back from the tracked player's eyes, so a live broadcast on the hub shows a player's view instead of a spectator standing still.
