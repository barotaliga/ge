---
title: A game's clone is checked by seeking off it, not by comparing copies
date: 2026-10-23
scope: engine/replay/player.cpp, games/fall_line/sim/play.cpp, games/fall_line/labs/replay_lab.cpp
concepts: [clone, keyframe, seek, digest, copy-constructor, budget]
---

Fall Line's `clone` is its run's copy constructor (the driver, the pilot's hands, the event
backlog) with the trace switched off; `GameState` and the tape recorder are values and the map is
a shared pointer, so nothing needed writing by hand. What proves a clone is complete is a seek
that starts from a keyframe landing on the straight run's digest at many marks (a step either side
of every keyframe catches a part copied one step stale), so keep `tests/fall_line_play_test.cpp`'s
seeks and the lab's `mismatches`. Cut the lab's run with `--seconds` for the smoke pass: the cost
is the pilot's race and the straight pass, not the seeks.
