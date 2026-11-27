---
type: Added
title: A game being copied in shows a progress bar; relative installs and a config beside the program
---

Taking a game into the data folder now runs in the background with the console down and a bar at its foot, as a map download does: "Copying Quake" by the share of the files' bytes copied, then "Taking Quake's music" by its tracks. A game whose `fs_basedir_q1` or `fs_basedir_q2` is new is copied the same way once the window is up.

A relative `fs_basedir_q1` or `fs_basedir_q2` starts from the folder the program is in (the one holding the app on a Mac), for the window, `sesv` and `secli` alike. A config beside the program, `data/settings/config.cfg`, is read after the player's own and wins over it, and the settings are written to it, so a copy of the game kept on a stick carries its own settings; `cfg_save_global` writes them to the player's own config instead. Settings beside the program are no longer copied into the player's folder.
