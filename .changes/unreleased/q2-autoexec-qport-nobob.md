---
type: Added
title: Quake II's configs on a game change, qport and cl_nobob
---

Quake II's `fs_autoexec` runs each game folder's autoexec.cfg and postexec.cfg again when another
game is loaded, and `fs_default` (both games) says whether the game's default.cfg runs at start-up
and on a game change. `qport` sets the number a server knows a client by (0 picks one from the name), in
both games. Quake II's `cl_nobob` keeps the items the game makes float lying still.
