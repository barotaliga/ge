---
type: Added
title: A server can be locked
---

`sv_locked 1` refuses every new client while a player who drops can still come back to their seat, so a match's players stay and nobody else joins. An admin sets it while the server runs with `rcon sv_locked 1` (and `0` to open it again); both games' servers read it.
