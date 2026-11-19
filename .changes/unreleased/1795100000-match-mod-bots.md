---
type: Added
title: A match mod's own bots and map changes on the dedicated server
---

A match mod shipped as a compiled module now adds and runs its own bots on Shooter's Edge's dedicated server: they take a slot every client sees, run in bot frames at the server's packet rate and steer off walls through the mod's blocked handler. A map change keeps what the mod keeps in the server (its settings, local and server info) and every player's spawn parms, so a match mod's mode, bot settings and ready marks survive it. The dedicated server also passes a mod its config's own `set` lines, hands console lines it does not know (`map`) to the game, and prints the mod's console lines.
