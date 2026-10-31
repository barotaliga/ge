---
title: Share QC between the two program forms through a glue file, and check every message format
date: 2026-10-31
scope: games/shooters_edge/qc/
concepts: [qc, program-form, single-player, multiplayer, glue, variant, temp-entity, campaign-host]
---

One set of QC sources builds both program forms when each variant compiles its own `engine.qc`
(the form's globals, fields and builtins) first and its own `glue.qc` after the shared
definitions: the shared code calls `say_all`, `say_to`, `effect_send`, `effect_impact`,
`note_frag` and `missile_now`, never `bprint`, `sprint`, `multicast`, `logfrag` or `newmis`,
whose signatures or existence differ by form. QC has no user variadics, so the glue takes a
fixed count of strings and callers pad with "". The forms also differ in MESSAGE BYTES, not just
builtins: the single-player gunshot carries no count and has no blood temp entity, which the
compiler cannot see; `se-campaign`'s "unread on the unreliable stream" row finds it. And the
campaign host's `deathmatch` defaults on: a test of a campaign program sets `deathmatch 0`, or
the intermission waits 5 s and a death respawns instead of restarting.
