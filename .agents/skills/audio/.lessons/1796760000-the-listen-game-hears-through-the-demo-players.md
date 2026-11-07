---
title: The window's own game is heard by giving the demo players a DemoState built off the server
date: 2026-11-07
scope: games/shooters_edge/client/listen_sound.cpp, games/shooters_edge/client/listen.cpp, games/shooters_edge/client/demo_sound.cpp
concepts: [listen-sound, demo-sound, static-sound, ambient, listener, sound-event]
---

`DemoSounds` needs only a `DemoState`'s sound list, static sounds, entities (by number, for
following), eye and angles, so the listen game reuses it rather than a second player: build that
state each frame from `RulesCommon` (the sound list, the edicts) and the player's eye, and feed
it the `sound` events addressed to slot 0. The map's loops (`ambient` events) are written while
the map loads, before any frame's tap runs: read them off `server->out.events` in `entered()`,
where they still are, or the level is silent. Key a new level on a counter `entered()` bumps
(`Listen::entries`), not on the map's pointer, which a new allocation can reuse. Volume bytes
are 0..255 in both events, but a sound's attenuation is a float while an ambient's is already
times 64.
