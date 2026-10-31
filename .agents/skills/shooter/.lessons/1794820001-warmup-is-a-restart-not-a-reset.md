---
title: Start a match after a warmup by restarting the map, not by resetting scores
date: 2026-10-31
scope: games/shooters_edge/server/lobby.cpp, games/shooters_edge/client/lobby_room.cpp, games/shooters_edge/client/listen.cpp
concepts: [lobby, warmup, match-start, restart, frags, limits, bots, reliable-streams]
---

The lobby's warmup plays the real map under the real mod, so whatever it scores lives in the
program's fields (frags, items taken, the mod's own timers). Zeroing frags by hand at the start
misses the rest and differs per mod. Restart the map instead: a new QC server on the same loaded
map, the player connected first, then every seat in slot order with its team applied again
(`room_enter`), and the bots' fighters made again off the new server. Hold the frag and time
limits at 0 during warmup (`fraglimit 0`, `timelimit 0` after the settings' lines), or a busy
warmup ends the map before anyone is ready; the restart runs the settings' lines again and brings
the limits back. On a listen server nothing reads a bot slot's reliable stream, so clear
`out.one[k]` for k > 0 after each frame or it grows for the whole session. Check a restart by every
seat's health at 100 and frags at 0 on the first frame of the match.
