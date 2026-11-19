---
type: Added
title: Servers can refuse chosen client commands and ignore chosen addresses entirely
---

`filtercmd_add <command> [ignore|log|print|stuff|kick] [text]` stops a client command from reaching the game. The server can just drop it, note it in the log, show the player a message, run a line on their console, or kick them. `filtercmd_remove` and `filtercmd_list` manage the list. `blackhole_addip <address>` makes a server ignore every packet from an address, queries included. Both work on QuakeWorld and Quake II servers, and Quake II also takes its own spellings (`addfiltercmd`, `addblackhole` and the rest).
