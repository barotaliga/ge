---
type: Added
title: Record a QuakeWorld server you joined, and have each match recorded on its own
---

`record <name>` and `stop` now work on a QuakeWorld server you joined: the file holds the whole level from its start, even when you begin late. `easyrecord` names the file by the match (players, teams and map, through the `match_format_*` patterns and `match_name_*` words) with the next free number.

With `match_auto_record 2` each match is recorded from its start to a few seconds into the scores and saved on its own; with 1 it waits for `match_save`. `match_auto_minlength` and `match_auto_spectating` choose what is kept, and `match_forcestart` starts one by hand.

`demo_jump` moves a playing demo to a time (`demo_jump 1:30`) or on and back (`demo_jump +15`); in Quake II it is also `seek`.
