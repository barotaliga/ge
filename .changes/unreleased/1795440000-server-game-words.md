---
type: Added
title: A server game's own console words, declared
---

`ServerGame::words` declares the console words a game takes through `ServerGame::console` (its rules' `map`, its admin's `kick`); the dedicated server registers them in the game's own layer (`add_game_words`), so Tab completes them and `cmdlist` and the reference manual list them. Shooter's Edge declares QuakeWorld's and Quake II's. `ManualInfo::more` adds settings that live outside the console to a manual pass: the window and the dedicated server list each game's rules' own settings (`deathmatch`, `coop`, `skill` and the rest).
