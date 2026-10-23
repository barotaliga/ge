---
title: Check a save by loading it into a run already played on, not a fresh one
date: 2026-10-23
scope: engine/play/saves.cpp, games/fall_line/labs/save_run_lab.cpp
concepts: [save, load, check_saves, describe, missing-field, digest, dirty-load]
---

A field left out of a describe is not loaded, so it keeps whatever the run it was loaded into
had. In a FRESH run that is the field's start value, which is often the value it still has at
the save (a counter that has not moved, a flag not yet raised), and the check passes by luck.
`check_saves` and the `save-run` lab load each save into a run played on 360 steps first, so
such a field holds a wrong value of its own and the digests part. They also save both runs at
the log's end and compare the bytes, which catches a field the digest never folds in.
Measured: with `yaw_rate` taken out of the skier's describe every save from step 1 on parted
the next step; a field that never changes the run (`air_peak`, reset on every take-off) stays
silent, which is right: it is not part of what the run does next.
