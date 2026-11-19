---
type: Changed
title: Shooter's Edge's window picks the game by number too
---

`se -game 1` plays the classic game and `se -game 2` Quake II, each from the install kept for it on this computer. With none kept, the window asks for the folder the game is installed in, checks that it is that game's install, asking again with the reason when it is not, and keeps it for the next start. A mod's folder is now `-mod <folder>` (it was `-game <folder>`). `secli` takes `--game=N` and `--mod=<folder>` the same way, and it now starts the window under its new name, `se`.
