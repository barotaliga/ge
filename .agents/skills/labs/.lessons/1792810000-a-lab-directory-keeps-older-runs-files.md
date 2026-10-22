---
title: A lab's directory keeps an older run's files; read its report.json for this run's shots
date: 2026-10-22
scope: engine/lab/diff.cpp, engine/lab/report.cpp
concepts: [previews, report, stale-files, diff, shots, smoke]
---

`write_report` writes a run's files into `previews/<id>/` and never empties it first, so a smoke
run after a full run leaves the full run's other shots beside its own. The first picture diff
walked every `<shot>.json` in the directory and compared shots the run never took (each one a
"different size" warning). Anything that reads a run back takes its list of shots from that
run's `report.json`, never from the directory listing.
