---
type: Added
title: A server chooses what a status query shows
---

`sv_status` 1 answers a server browser's status query with the server's info alone (no player list) and 0 not at all; `sv_statusrate` limits how often it is answered (Quake II's server: 15 a second); `sv_uptime` 1 or 2 adds how long the server has run. Both games' servers read them; Quake II's `sv_status_show` and `sv_status_limit` are the same settings.
