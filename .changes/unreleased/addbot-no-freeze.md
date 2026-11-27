---
type: Fixed
title: Adding a bot no longer freezes the game
---

In the game's window, a bot added to a match (or brought into the next map) now takes its seat at once while the map's navigation is worked out on another thread; it stands still until that is ready and then plays. Before, the whole game stopped for that time: about a second on dm6 and many seconds on larger maps.
