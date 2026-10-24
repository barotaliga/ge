---
type: Added
title: Replay files, the replay index and the instant replay
---

A run can be kept as a replay file (`ge::replay::ReplayFile`, `*.ge-replay`): its input log with the moments the game marked, a clip's in and out, the game's behaviour version (`GameSpec::version`) and when it was recorded. A file recorded by another version of the game is refused with both versions named (`plays_on`). `scan_replays` lists a directory's replays for a browser. `Live` records a run as it is played, keeping keyframes and marks, and lends its keyframes to an instant replay (`watch`, `resume`) without touching the live run. The `replay-file` lab holds all of it on Fall Line.
