---
title: A classic game's files are opened through plugins/classic/content, not the client
date: 2026-11-15
scope: plugins/classic/content/, games/shooters_edge/client/files.hpp, games/shooters_edge/client/load_order.hpp
concepts: [content-layer, search-path, load-order, client-folder, using-declarations, shooter]
---

The search path over a classic install and each game's load order live in
`plugins/classic/content/` (`ge::classic::content`): a new classic game adds its row to
`load_order.cpp` (its archive endings, whether loose files win, a bundled client's folder by name,
its startup steps) and its content profile, and every shooter game opens its files with no game
code. Shooter's Edge's `client/files.hpp` and `client/load_order.hpp` are only using-declarations
so the client's code keeps its short names; a change to how files are found is made in the plugin.
