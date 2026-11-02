---
title: A game's main menu is its own table; a classic game's HUD has no free-art choice
date: 2026-11-02
scope: games/shooters_edge/menu/main_menu.hpp, games/shooters_edge/client/classic_pictures.hpp
concepts: [menu-table, game-adapter, hud-art, classic-assets-only, se-menu]
---

The main menu's pages and rows are a `menu::MenuTable` the game's adapter hands out; every reader
of the table (`find_entry`, `build_menu`, `refresh`, `apply_event`, `reset_defaults`) takes it
from the host (`Host::table`, null meaning the first game's), so a second game's table never
touches the first's. The maintainer's rule that a loaded classic game uses its own assets alone
removed the `hud_art` cvar and its VIDEO row: the window calls `hud_art_of`. Removing a row
changes the `se-menu` digest on purpose; rewrite it with `labs se-menu --smoke
--write-digests=tests/digests` and say so in the PR.
