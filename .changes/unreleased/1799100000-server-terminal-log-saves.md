---
type: Added
title: A line editor at the server's terminal, the console log, and saves on Quake II's dedicated server
---

At a terminal a dedicated server now edits the typed line in place: the arrow keys, Home, End, Delete and the usual cut keys work, Up and Down walk the lines typed before (kept across starts), and Tab lists what a word could be as the player's console does. Every line the server prints can also be kept in a log file under `logs/`, new each time or added to, with a time stamp before each line. Quake II's own names for these settings (`sys_history`, `com_completion_mode`, `logfile` and the rest) work too. Quake II's dedicated server takes `save`, `autosave` and `load` for a single-player or co-op game, and keeps each level's autosave.
