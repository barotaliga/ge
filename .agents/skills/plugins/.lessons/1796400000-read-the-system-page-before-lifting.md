---
title: Before placing a piece of a classic game, read its system's porting page
date: 2026-11-19
scope: docs/porting
concepts: [plugin, port, lift, layer, ledger, benefit, drawback, classic-game, hook-points]
---

`docs/porting/<system>.md` lists, for one system (maps, picture, network, server, ...), the shared
files already there, each game's file beside them and the hook points a new game fills. Most
"new" pieces of a port already exist for an earlier game: the page says which layer holds them and
what the game adds (a row, a field, an adapter call, an extension bit). When a port lifts a
benefit to a shared layer or leaves a drawback behind, add the row to the system's page and to
`docs/porting/ledger.md` in the same PR; a page whose paths drift is fixed in the PR that moves
the file.
