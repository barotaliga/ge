---
title: A save in the window loads only into a run of its own setup, and its input log is not kept
date: 2026-11-14
scope: games/fall_line/controls/by_hand.cpp, games/fall_line/app/menus.cpp, games/fall_line/app/main.cpp
concepts: [load, setup, input-log, autosave, menu-picture]
---

The window draws one map, built once (its frame, its forest, its sky), so a save of another seed
cannot load into it without rebuilding the frame: compare the save's meta `setup` with the run's
(`play::setup_text`) and list and load only the matches, refusing the rest in a game's words. A
loaded run's recorder starts at the save, so its log plays back from no start: skip writing it
rather than write a log that replays wrong. The store's autosave spacing starts at minus
infinity, so the first frame of play is "due": hold the first autosave until a spacing of play
has passed.
