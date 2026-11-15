---
title: A player's view off a recording is checked against the live game's bytes at every frame
date: 2026-11-15
scope: engine/server/playback.cpp, engine/server/sample.cpp, tests/server_record_test.cpp, engine/lab/labs/server_record_lab.cpp
concepts: [playback, player-view, watch-hooks, seek, keyframe, server-record]
---

"Plays back from any player's view" is proven only when the game gives the same view twice: once
off its live state (`ServerGame::player_view`) and once off the viewer's state kept from the
stream (`watch_view`), compared byte for byte at every frame and after seeks that land between
keyframes. Keep both writers on one helper (the sample game's `view_of`) so they cannot drift.
A recording needs a keyframe at frame 0 for `playback_open`; begin recording before the first
server frame. The relay half reuses the TV trial with the recording as its `source` and
`block_ms` 1000: the lag then reads straight against the source's delay.
