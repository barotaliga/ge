---
title: Text that repeats the state is predicted from the state, not from the text
date: 2026-11-14
scope: games/shooters_edge/client/demo_v2.cpp
concepts: [replay-v2, field-codec, text-template, team-info, coder-revision, measure]
---

A 4on4 recording sends each team's info lines (`//tinfo <slot> <x> <y> <z> <health> <armor>
<items> "loc"`) to every viewer, three copies a frame. Predicting the place from the same
template's last numbers missed by the player's whole movement since the last line; predicting it
from that player's origin (eighths, truncated to whole units) took the text 17% down, but only
once a copy in the same frame (the template's number remembered with its time) was taken from
the copy before: the first try, origin alone, made every copy pay the same residual and was 35%
worse. Read the strings before guessing: the slot there is the slot itself, not slot + 1, and
the line has no leading tab. Every codec change goes behind the coder's revision
(`kCoder`, the header's `game_version`) so files packed before still open.
