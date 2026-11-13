---
title: A web build that plays is a second target, so the watching page stays light
date: 2026-11-13
concepts: [web, wasm, download-size, hub, play-in-browser, ge-add-web-viewer]
---

Linking the classic match server and the baseline mod's compiler into the web viewer took its
wasm from 4.2 MB to 7.1 MB (gzip 1.75 MB to 2.8 MB), every Demo and Watch page paying for a
server it never runs. `ge_add_web_viewer(<game> NAME <game>_play ... DEFINES SE_WEB_PLAY=1)` builds
the same `viewer.cpp` again with the play mode compiled in, into the same folder; the hub's Play
page loads `<id>_play.js`, the others `<id>_viewer.js`. Measure both files' `.gz` (the
`hub-viewer` lab's `viewer_sent_kb` and `player_sent_kb`) whenever either one's libraries change.
