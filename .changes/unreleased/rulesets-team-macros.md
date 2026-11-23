---
type: Added
title: The strict rulesets keep scripts off the team macros
---

Under `smackdown`, `smackdrive`, `qcon`, `thunderdome` and Quake II's `duel`, a line that uses a team macro such as `$health` or `$loc_here` may only talk to the team (`say_team`, `if`, the `tp_msg` reports), so a bind cannot act on what the player holds. During a match message triggers, pattern triggers and the team events (`f_death`, `f_took` and the like) keep quiet, the team's sound words never play, and `qcon` allows ten `wait`s in a row.
