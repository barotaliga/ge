---
title: A draw that read no demo frame throws no effect again
date: 2026-11-06
scope: games/shooters_edge/client/timedemo.cpp, games/shooters_edge/client/demo_fx.hpp, games/shooters_edge/net/classic_remote.cpp
concepts: [demo-effects, playdemo-step, temp-entity, gather, joined-server-effects]
---

`next_frame` empties a frame's news (sounds, prints, effects) only when it reads one. A paced
playback (`playdemo_step`) or a joined server's step can draw several times between two 20 Hz
frames, and a draw that reads none still holds the last frame's lists: an effect taken from them
on every draw bursts again each time. Each stepping function therefore sets the state's
`effects` to what it read in that call (none when none), and `demo_fx_frame` appends them once a
draw. The same holds for anything else a draw takes from the frame's lists. Also: an effect list
kept a level at a time must be emptied with the FxState's counters on a new level, or the first
draw of the level throws all of the last level's at once.
