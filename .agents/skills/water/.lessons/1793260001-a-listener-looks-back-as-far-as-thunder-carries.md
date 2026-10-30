---
title: A thunder listener's first look goes back as far as thunder carries
date: 2026-10-30
scope: engine/wind/thunder_play.cpp
concepts: [thunder, listener, schedule, arrival, sample-voice, block]
---

Thunder arrives up to 75 s after its flash (25 km at the speed of sound). A player that only
looks at flashes from the moment it is first asked is silent for over a minute under a storm
already raging, then fills in. The first `hear_storm` call looks back `kSilent / speed` plus a
flash's length, and drops only the claps whose first sound has already gone by. To start a clap
on time inside a block, hand the synth the offset as the play's `delay_ms` rather than starting
it at the block's edge: played in 10 ms blocks, the onset is then within 2 ms at 1, 4 and 12 km,
not up to 10 ms early.
