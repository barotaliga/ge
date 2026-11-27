---
type: Changed
title: The game played and each game's install are settings, fs_basegame and fs_basedir_q1/q2
---

Shooter's Edge picks the game it plays with `fs_basegame` (`""` its own game, `q1` Quake, `q2` Quake II) and finds each classic game in `fs_basedir_q1` and `fs_basedir_q2`, the folders they are installed in. They are ordinary settings kept in the series config: SELECT GAME, LOAD GAME and `games <n>` set them, setting `fs_basegame` in the console opens that game, and the window, the dedicated server (`sesv +set fs_basegame q2 +set fs_basedir_q2 <folder>`), the TV node and `secli` (`--fs_basegame`, `--fs_basedir_q2`) all read the same words. Classic remix on a server reads the other game's install from its own `fs_basedir` setting.

Removed, with nothing carried over: `-classic`, `-basedir`, the console's `basedir`, `cr_basedir`, and the `installs.txt` and `game_folder.txt` files. An install found beside the game is listed by `games` but no longer played on its own. Pick your game again once in SELECT GAME.
