---
title: Take a classic program's numbers from the program itself, and write stand-in text new
date: 2026-10-31
scope: games/shooters_edge/client/view.cpp, games/shooters_edge/labs/se_menu_lab.cpp
concepts: [clean-room, item-bits, constants, find-global, stand-in-text, campaign, hud]
---

The HUD needed the campaign's item bits (the keys, the cloak, the suit) and the clean-room notes
do not list them. A compiled program keeps its named constants among its global definitions, so
the client reads them by name from the program that is running (`find_global(progs, "IT_KEY1")`)
and falls back to nothing when a program has no such name. That is the player's own data, read
at run time; nothing is recalled from the original source and nothing is hard-coded.

The same goes for words: a lab's stand-in for an episode's closing text, a map's title or a
level name must be written new for the lab. Text recalled from the classic game is its content,
and a name from it is one of the names this tree never carries outside `porting/`. Check every
stand-in string before the commit as you would a picture.
