---
title: A viewer seated partway into a level needs the level's state, not just its frames
date: 2026-11-14
scope: games/shooters_edge/net/q1_tv_netgame.cpp
concepts: [netgame, single-player, tv, follow, catch-up, scoreboard, background-conversion]
---

The frames of a single-player recording carry every entity in full, so a viewer who joins a level
late sees the world right. But names, frags, colours, light styles and stats ride reliable
messages, sent only when they change. A viewer switched to another player's view, or seated
late, got the level's signon and then the frames, and its scoreboard read zero. The 1996 client
showed this at once: the followed player had 0 frags. Send the last of each of those messages
from the blocks played since the level began, when the viewer begins (`queue_catch_up`). A test
proves it only when its window misses every live change: follow after the kills and check before
the next kill, and make sure the check fails with the catch-up turned off.
Converting a 20-minute multiview recording to one more view took 16 s in the debug build, and the
whole TV stalled. Run the conversion on its own thread (`NetgameTvConfig::convert_in_background`),
keep the viewer on its old view until the new one is in, and leave it off in tests and labs so
they take the same steps on every run.
