---
type: Added
title: The hub's match cards show each map from its intermission view
---

A hub given `--content=<dir>` shows each live match as a picture: its map seen from the map's own intermission view in the classic look, with the live board over it in the game's own letters (ping, frags, the team only in a team match, the name). Each map's picture is drawn once and kept in `--stills=<dir>`, and drawn again when the map or the renderer changes. Quake II servers now tell the hub their game, mode and players.
