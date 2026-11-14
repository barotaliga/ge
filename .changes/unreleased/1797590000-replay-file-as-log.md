---
type: Added
title: a run's replay file plays wherever an input log does
---

`ge replay`, `ge state`, `ge diverge`, `ge why` and the saves now take a run's replay file (`.ger`) as well as its input log, read back into the log it was played from. A game's `play` executable turns this on with one line, `GE_REPLAY_LOGS();` (engine/replay/play_logs.hpp); Fall Line's does.
