---
type: Added
title: Watch TV in a browser, fetch finished matches, find TV on the master
---

A TV host now serves WebSocket viewers beside its own (`engine/tv/websocket.hpp`), so a browser can watch a match live; it lists its finished replay files by SHA-256 and sends one on request (`engine/tv/library.hpp`); and it beats to the hub's masters as a TV node, answering status asks with its name, game, map and viewers.
