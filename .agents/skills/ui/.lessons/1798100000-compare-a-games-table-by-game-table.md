---
title: A game's menu table may be a script's table read over it; compare by game_table
date: 2026-11-23
scope: games/shooters_edge/menu/main_menu.hpp, games/shooters_edge/menu/menu_script.cpp, games/shooters_edge/menu/game_menu.cpp
concepts: [menu-table, menu-script, game-table, set-table, entry-id]
---

`GameMenu::set_table` returns the table it built, which is the game's own table or, when the game
names a menu script (`MenuTable::script`) found on its files, a `ScriptMenu`'s table read over it
(`MenuTable::over`). Code asking "is this Quake II's menu" compares `&game_table(t) == &q2_menu()`,
never `&t` itself, and the window keeps the returned table as the host's. A row's `id` is the cvar a
changed setting writes (`cvar_of`: the `needs` for a bit row), so a script's two bit rows of one
cvar take ids `cvar.bit`, and a second plain row of one cvar on a page is dropped.
