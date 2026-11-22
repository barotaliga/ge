---
title: A game's menu table is its own rows plus the pages it shares, built once at start
date: 2026-11-21
scope: games/shooters_edge/menu/q2_pages.cpp, games/shooters_edge/menu/main_menu.cpp
concepts: [menu-table, q2-menu, pages, bit-toggle, key-row, in-level-page]
---

`MenuTable` holds spans, so a game whose pages differ builds a static vector once: its own
constexpr rows first, then every row of `entries()` whose page it does not define (classic
enhance, the held weapon, the questions). A page id the game defines replaces Quake's whole; a
row to splice into a shared page (START SERVER's flags) is pushed before the row it precedes.
`Entry::bit` toggles one bit of the cvar named by `needs` (its id stays unique per page);
`Entry::bind` makes a key row on any page, and `bind_command` gives the game menu the command it
waits to bind. ESCAPE in a level opens `MenuTable::in_level` when `Host::in_level` is set.
