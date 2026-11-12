---
type: Added
title: our TV node relays a classic match to viewers who join late
---

Shooter's Edge has a TV node that pulls a match from a classic stream port (our server's or any other's), holds it for a delay and serves it to classic viewers and relays over the same protocol. A viewer who joins mid-match is caught up from the match as the node keeps it (the level, what changed since, every player and entity), so it sees the match as it stands within a few hundred bytes. Viewers' `say` stays on the node and `say_game` reaches the players; the node has its own password and dials the server again when the connection falls. A server's stream now cuts its signon into records a relay takes. The `se-tv-node` lab shows each viewer's catch-up and that every stream plays to the same end.
