---
type: Added
title: Quake II's recorder keeps its own spectator; the relay shows the intermission; movies between maps play over the network
---

Quake II's multiview recorder keeps a spectator of its own in the game, so a recording or a relay always carries a scoreboard and the messages a viewer reads: its begin and score commands, `mvdstuff`, auto-recording through it, which players are captured and which messages are kept are all settings. A relay copies the stats it names from that spectator to a viewer chasing a player and turns to it for the intermission. A movie or picture between maps is now a level of its own on the network, played by the clients until they ask for the next server; `sv_cinematics 0` skips a movie.
