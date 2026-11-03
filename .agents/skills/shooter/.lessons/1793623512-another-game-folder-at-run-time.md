---
title: Another game folder at run time is the listen stopped and the world dropped before the search path is replaced in place
date: 2026-11-02
scope: games/shooters_edge/client
concepts: [game-folder, basedir, load-game, search-path, listen-stop, map-change]
---

The listen server's map and the world draw view into the packs' bytes, so replacing `files` while
either lives leaves them reading freed memory. LOAD GAME (`main.cpp`'s `open_folder`) opens the new
folder into a fresh `SearchPath` first (nothing changes if it does not open), then `world.reset()`
and `listen_stop(listen)`, then moves it into `files` and reloads the programs into the same
`GameContent` object. Everything else captured `files`, `game_content` and `next_map` by
reference (the console's commands, `listen.opt.files`), so filling those objects in place keeps
every capture valid; never replace them with new objects. The first map is then started by the
ordinary map change, which takes `listen_start` because `listen_stop` left `listen.progs` null.
