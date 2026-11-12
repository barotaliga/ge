---
type: Added
title: classic relays can stream a match from our server
---

Shooter's Edge's dedicated server now offers the classic broadcaster's stream port: set `qtv_streamport` and a classic relay can connect over TCP and be sent the match live, with its catch-up when it joins and again at every map. `qtv_password` asks the relay for a SHA3-512 answer to a challenge (or the plain password), `qtv_maxstreams` limits the relays, and `qtv_pendingtimeout`, `qtv_streamtimeout` and `qtv_sayenabled` tune it. Viewers' chat comes back to the players (spectators only unless `qtv_sayenabled 1`) and into every stream, and their comings and goings are told to the players. The `se-tv` lab shows the catch-up and the stream's bytes a second.
