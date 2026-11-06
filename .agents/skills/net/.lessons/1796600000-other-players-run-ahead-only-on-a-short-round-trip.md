---
title: The other players run ahead only while the round trip is under 20 ms
date: 2026-11-06
scope: games/shooters_edge/net/classic_remote.cpp, plugins/shooter/predict.hpp
concepts: [classic-protocol, prediction, predict-other, interpolation, lerp, latency, remote-client]
---

The classic rule for another player draws him at the window's time less the round trip plus
0.02 s, run forward from his newest record's time (its arrival less the record's `msec`). Past
a 20 ms round trip that time falls before the record, `other_msec` clamps to 0, and he is
drawn where the frame put him: on a 40 ms link the `se-remote` lab shows no change from the
prediction, only from the entities' lerp. That is the rule, not a bug: do not "fix" it by
extrapolating further without a person deciding it.

Test it with the window drawing faster than the server's 77 frames a second (the lab and
`watch_other` step the window at 240): with one window frame a tick, a frame-held place and a
predicted one look the same. And restore the entities' newest places before each lerp when
no frame was read: the drawn list is the reader's, rebuilt only when a frame arrives.
