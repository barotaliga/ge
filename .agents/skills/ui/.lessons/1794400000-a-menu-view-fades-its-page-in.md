---
title: A menu view fades a page in; settle it before a still
date: 2026-10-30
scope: engine/ui/menu_view.cpp, games/shooters_edge/menu/game_menu.cpp
concepts: [menu, menu-view, animation, page-in, settle, lab, screenshot]
---

`MenuView` starts a page it has not drawn at `page_in` 0 and fades it in over the frames it is
stepped by, so a lab that opens a page and draws it once photographs the page almost
transparent, and the picture reads as "everything dimmed". A still calls `settle` (the game
menu's `settle()`, which settles on the next draw) before it draws; a window steps it by the
frame's time.
