---
title: A classic game uses its own assets alone; mixing is classic remix
date: 2026-11-02
scope: games/shooters_edge/client/held.cpp, games/shooters_edge/client/game_adapter.hpp
concepts: [classic-weapons, held-weapon, game-adapter, remix, assets, at-eye]
---

The maintainer's rule: with a classic game loaded, every asset is that game's own, read through
its adapter, and Shooter's Edge's own assets (the built weapons, its HUD art) are for its own
content only. So no cvar chooses between them and nothing falls back: a classic game whose files
lack a held weapon draws none and the status line names the file (`load_held_set`). Playing one
game with another's parts is classic remix (M18u), a classic-enhance feature behind one switch
shown only with two FULL games loaded. A classic held weapon's file is authored at the eye, so
it takes `Hand::at_eye` (the classic row, no look offset); the fixture model must be scaled down
and moved forward to be seen at the eye at all.
