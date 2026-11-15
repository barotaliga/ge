---
type: Added
title: The server records a match, and it plays back from any player's view
---

Recording a match moved into the server core (`engine/server/record.hpp`), and the dedicated server writes it with `--record=PATH`. A game that adds the watch hooks to its `ServerGame` plays a recording back from any player's view with seeks (`engine/server/playback.hpp`). The `server-record` lab checks every view against the live game's and relays the recording with a delay to viewers under every network preset.
