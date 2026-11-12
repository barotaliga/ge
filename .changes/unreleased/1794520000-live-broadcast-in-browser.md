---
type: Added
title: The hub's Watch page plays a live broadcast in the browser
---

Shooter's Edge's web viewer joins a TV node's WebSocket (`--tv=ws://...`) and plays the match two
seconds behind as the node sends it. `unpack_more` unpacks a growing replay file one block at a
time, and `engine/web` gives a viewer the browser's WebSocket.
