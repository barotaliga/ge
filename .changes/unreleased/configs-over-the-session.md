---
type: Added
title: A server's settings followed by its clients, stuffed text under the client's rules, fake loss behind sv_cheats
---

A session now carries the server info (sent to every player when it changes and as each joins), each player's user info, and console text a server stuffs into a client. `engine/net/session_config.hpp` ties them to a console, so a client follows a server's `sv_` change within a round trip and a stuffed `quit` is refused. `net_fakeloss`, `net_fakelag` and `net_fakejitter` are cheats that move a client's own sends only while the server's `sv_cheats` is 1. The new `net-config` lab measures all of it over each network preset.
