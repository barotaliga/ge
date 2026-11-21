---
type: Added
title: Remote mode, clearnotify, con_scale and the console's foot while joining
---

The console gains `remotemode` (what you type goes to a server's console through `rcon`, the
prompt `#`), `clearnotify` and `con_scale`, and a game can show what is loading on its foot
(`ConsoleView::set_foot`). Shooter's Edge shows the file coming with a bar (drawn with the game's own charset pieces) and its share, then
"Loading <map>...", while it joins a server; Quake II's `con_alpha` sets the panel's opacity.
