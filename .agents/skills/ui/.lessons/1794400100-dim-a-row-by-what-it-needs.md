---
title: Dim a menu row by the cvar or command it needs, not by a list of what is built
date: 2026-10-30
scope: games/shooters_edge/menu/main_menu.cpp
concepts: [menu, dimmed, disabled, cvar, command, needs, console]
---

A row of Shooter's Edge's main menu names the cvar or command it needs (`Entry::needs`) and is
dimmed while the console has no such name, so it lights up by itself the day the cvar lands, and
no list of finished features goes stale. A name the game has but ignores (the server's `skill`,
which the baseline mod never reads) still needs `built = false` and a hint saying why.
