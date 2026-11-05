---
type: Changed
title: The 1000-line cap is advice; 2000 is the cap
---

`ge doctor` now warns about a source file past 1000 lines and fails one only past 2000 (an `agents:allow-large-file: <reason>` comment still exempts it), so a file that grows a line past 1000 no longer forces a split mid-task. The `write-code` consumer skill says so.
