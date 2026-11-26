---
type: Fixed
title: On a QuakeWorld server, `team` sets your team and a map restart no longer crashes the game
---

Typing `team blue` while joined to a QuakeWorld server now sets your team the way Quake does, in your player info for everyone to see; before, the server was sent it as a command it did not know, and your team never changed. `team` alone says your team. Our own server takes `team <name>` the same way when it runs no lobby.

The game could crash when the server joined started its map again (as a match starting does): the next prediction of your moves read the doors and lifts of the copy of the map it had just let go.
