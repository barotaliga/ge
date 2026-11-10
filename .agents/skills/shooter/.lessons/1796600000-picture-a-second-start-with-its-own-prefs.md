---
title: A second start is pictured with its own per-user folder, the kept game in it
date: 2026-11-05
scope: games/shooters_edge/client/game_folder.hpp, games/shooters_edge/client/main.cpp
concepts: [kept-game, remember-game, choose-folder, prefs, xdg-data-home, second-start]
---

The window keeps the game loaded last in `<prefs>/game_folder.txt` (`profile <id>`, `basedir
<folder>`), and `prefs` is SDL's per-user folder: `$XDG_DATA_HOME/gameengine2/shooters_edge/` on
Linux, `~/Library/Application Support/gameengine2/shooters_edge/` on a Mac. Every start that
opens a game writes it, a `-basedir` start and a `--frames` run included, so a run in the
container changes what the next run opens. Give each picture of a start its own
`XDG_DATA_HOME` in scratch: one empty (nothing kept: the arena and the game's own menu), one
written by a first `-basedir` run, then a plain start under it shows what the player sees on
their second launch. `+basedir` does not work as a start word (the command is made after the `+`
lines run), so the first run picks the game with `-basedir`.
