---
type: Added
title: Server administration from a player's seat: rcon, status, kick and bans
---

A dedicated server now has an administrator's tools any player can reach from their own console: `status` lists the players, and `rcon <password> <command>` runs a server command (`kick`, `ban`, `addip`, `listip`, a setting) and sends its answer back over the game's encrypted link. A wrong password is refused, and three in a row make that player's tries wait. Bans are kept in `listip.cfg` (`sv_banfile`) and read back when the server starts; a banned address gets no seat. With `sv_logfile` set, the server keeps a log of who joined, left, chatted and ran what, never with a password. Games build on the same list of server features without changing the server core.
