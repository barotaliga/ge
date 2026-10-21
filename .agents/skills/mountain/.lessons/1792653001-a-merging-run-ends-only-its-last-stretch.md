---
title: Cut a merging run's corridor at its end only along its last stretch
date: 2026-10-21
scope: plugins/mountain/mapgen/network_stamp.cpp
concepts: [stamp-run, junction, corridor, packed, invariant]
---

A run that merges into another stops pressing past the plane through its last station, so the
other run's surface carries on there. Applied to every segment, that plane also cuts away the
corridor of an earlier stretch: a run that traverses back across the fall line puts parts of its
own line beyond its end-plane, and those parts were left unpacked. The cut now applies only to
segments within two corridor reaches of the end. The test's invariant ("packed along its line
but in its drifts") is what found it. When you port a stamp, add an invariant like this for
each mask it writes.
