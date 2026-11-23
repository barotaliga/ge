---
type: Added
title: Quake II reads its menu script, so a player or a mod changes the menu's pages as text
---

When Quake II's files hold a q2pro.menu, its pages and rows are read over the game's own menu when the game loads: a page it names replaces ours, a new page is added, and every row word works (choices, sliders, toggles of one bit, key rows, commands, save slots, text settings). A line it cannot use is told in the console with its line number. Set menu_script 0 to keep the game's own pages.
