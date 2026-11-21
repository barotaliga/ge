---
type: Added
title: Record a Quake II server you joined, keep each match's console log, and a demo clock
---

`record`, `stop` and `easyrecord` now work on a Quake II server you joined too, as a `.dm2` that plays from the level's start even when you begin late.

Each match followed on its own now keeps the console's lines from its start to its end: `match_auto_logconsole 2` saves them beside the recording as `<match name>_NNN.log`, 1 (the default) keeps them for `match_save`, 0 turns it off.

`cl_democlock 1` shows the time since the demo began over a demo you watch, 2 the recording's own clock; `cl_democlock_x` and `cl_democlock_y` move it. `cl_demowait 1` holds a demo paused on its last frame instead of ending it.
