---
title: A per-map change lives in the settings every map is made from, put back before the next one
date: 2026-11-10
scope: engine/server/vote.cpp, games/shooters_edge/net/classic_game.cpp
concepts: [vote, per-map, revert, restart, settings, fraglimit, hold-limits, prediction, notify-feed]
---

Shooter's Edge builds every map's rules afresh from `Game::settings` (`open_map`), and the lobby's
match start is a RESTART of the same map: a vote's change set only in the running rules was lost
the moment the match began. So a passed change is written into `settings.rules` (a restart keeps
it) and put back from `vote_map_over` at the top of `move_to` when `match_over` (a real change),
before the next rules are made. Three traps: the warmup holds `fraglimit` and `timelimit` at 0
(`hold_limits`), so a vote on them waits for the match instead of ending the warmup; a setting the
clients predict with (the move's) reaches them only in the signon, so it restarts the map; and the
`before` to restore is the settings' value (empty: the mod's default), never the live cvar, which
the warmup has zeroed. The window's HUD feed clips a line near 40 characters: keep broadcast
lines short and put the detail in `vote` and `vote rules`.
