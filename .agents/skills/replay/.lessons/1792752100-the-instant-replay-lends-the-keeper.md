---
title: The instant replay lends the live keeper and starts from its first copy
date: 2026-10-23
scope: engine/replay/live.cpp, engine/replay/player.cpp, games/fall_line/labs/replay_file_lab.cpp
concepts: [instant-replay, keeper, move, open, map, run-length, file-size]
---

A `Player` built on a log opens the game from its setup, and Fall Line's open builds the map, so
an instant replay that opened would stall for the map's build. The player adopting a keeper starts
from the keeper's copy at step 0 instead (a clone shares the map), and the keeper is moved in and
out, never copied: `tests/replay_live_test.cpp` counts the walker's opens to hold it. The replay
file's size is the hands', not the run's: the bot steers on a new figure nearly every step, so its
log is a line a step (about 95 KB a minute of race); held keys write a line a hold. Quote the
`replay-file` lab's `bytes-per-minute`, never "a few kilobytes".
