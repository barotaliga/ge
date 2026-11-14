---
type: Added
title: Shooter's Edge's window watches a live broadcast
---

`se -tv <host:port>` joins a TV node as a native spectator and plays its broadcast as a growing demo, two blocks behind, holding the last frame while the next block is late (`games/shooters_edge/client/tv_live.hpp`, shared with the web viewer). The game CLI opens a hub's `tv` link with it, so a hub's Watch page's "Open in the game" starts the window on the match.
