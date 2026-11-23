---
type: Added
title: A packet log on every dedicated server, and a game's errors handled as Quake II's server handles them
---

A dedicated server can write every packet it sends and receives to a log file as text, with the time, the address and the bytes, so an operator can see what reaches a live server. When a game hits an error it can recover from, such as a mod's game library stopping, the server now drops its players with "Server crashed:" and the reason, then waits for the next map. With `com_fatal_error` set it ends instead, so whatever runs it can start it again. `fs_autoexec 0` lets a mod switch change the folders without running the configs again.
