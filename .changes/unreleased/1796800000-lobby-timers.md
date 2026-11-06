---
type: Added
title: The dedicated server's lobby takes sv_countdown and sv_intermission
---

A server with `sv_lobby 1` now reads `sv_countdown`, the seconds from everyone ready to the match (5), and `sv_intermission`, the seconds the scores stay up after a match before the next map (0, none). Either can be set in a `server.cfg`, on the command line (`+set sv_intermission 10`) or at the console. The engine's lobby has an intermission phase of its own, and the `se-dedicated-lobby` lab charts it.
