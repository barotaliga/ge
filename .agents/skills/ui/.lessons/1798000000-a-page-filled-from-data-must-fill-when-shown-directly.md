---
title: A menu page filled from data must fill when shown directly, not only on its opened event
date: 2026-11-22
scope: games/shooters_edge/menu/game_menu.cpp, games/shooters_edge/menu/server_page.cpp
concepts: [server-page, game-menu-show, opened-event, classic-widths, lab-check]
---

SERVER SETTINGS' pages are built from the sheet when the menu's input emits `opened`; a
`GameMenu::show(host, "server/...")` (a bind, the console, a lab) emits none, so it opened an empty
page or fell back to MAIN for a subpage. `show` now fills and opens the first page before the
subpage. The classic look's value column is 12 glyphs and its label column 22 (the lab's
`classic_problems` fails wider ones): a value's meaning longer than the look's column shows the
raw value instead, and a game's words for its rules are kept short (`qw_sheet.cpp`, `words_of`).
Draw a new data-filled page in a lab over a real pack (`se-attract --basedir=`) before trusting it.
