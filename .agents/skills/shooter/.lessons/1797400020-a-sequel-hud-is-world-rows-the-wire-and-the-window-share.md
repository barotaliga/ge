---
title: The sequel's HUD lives in the world's rows, so the wire and the window draw the same bar
date: 2026-11-13
scope: games/shooters_edge/server/sequel_bar.cpp, games/shooters_edge/server/rules_world.hpp, games/shooters_edge/server/sequel_wire.cpp, games/shooters_edge/client/sequel_bar.cpp
concepts: [sequel, status-bar, stats, layout-language, world-rows, wire, configstring, hud]
---

A layout game's status bar is data the server sends, not a HUD the client owns: its program,
each player's 32 stats and the image and item strings the stats index. Put all of it in
`RulesWorld` (`status_bar`, `images`, `item_names`, `PlayerNumbers::stats`) and nothing else:
the wire copies them into configstrings 5, 544 + n and 1056 + n and the player state, and the
window runs the same program over the same rows (`client/sequel_bar.hpp`), so a recording, TV, a
stock client and our window show one bar. Number items as the game's own list does (health is
one item, 41) so a stock client's item strings and `use` line up. A blow's flash bit must
survive to the next rules frame's rows: a blow between frames (a move's fall) is otherwise gone
before the wire writes.
