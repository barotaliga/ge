---
title: A rules setting the arena reads must be a server cvar, and say_all takes six parts at most
date: 2026-11-15
scope: games/shooters_edge/rules/common_midair.cpp, games/shooters_edge/server/qc_server.cpp, games/shooters_edge/rules/arena_glue.cpp
concepts: [native-arena, k-midair, cvar, mod-settings, say-all, bprint, strcat, mod-match]
---

Midair on the arena read `k_midair` with `q.cvar`, and a match with the console line `k_midair 1`
played as an ordinary deathmatch with no error: the classic server's console drops a line whose
name is no cvar, and `cvar` of an unknown name reads 0. A setting the rules read goes in
`kModSettings` (`server/qc_server.cpp`) beside `deathmatch`; then the console line, a vote and a
`server.cfg` all reach it. A test of a mode should check something only the mode does (no
"You got" line on a midair map), not just that the match ran.

`say_all` and `say_to` copy their parts into a fixed array (six and four): a seventh part wrote
past it, and ASan caught it on the first midair kill. Fold the tail into one string with
`q.strcat({...})` first.
