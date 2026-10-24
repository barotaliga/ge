---
type: Added
title: The replay director cuts a run's moments into a highlight reel
---

`ge::replay::plan_cut` ranks a run's marks and spaces them into a cut of shots, each opening before its moment (a flight on its run-in), held after it, at its own time rate and through the game's named lens, by a `ShotRule` the game gives each kind of mark; the reel is held to a length and a count, and every mark left out is listed as covered, dropped or unruled. `reel_at` is the reel's clock: the shot up and the run step it shows. Fall Line files its flights (back-dated to the take-off), wipeouts, shoulders, missed gates and the finish as marks, and the `replay-cut` lab plays its race's reel.
