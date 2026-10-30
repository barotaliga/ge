---
type: Changed
title: ge goals claim writes the row's status file
---

`ge goals claim "<what>"` now claims a row by writing its status file, `docs/goals/<row>.md`, with `Status: open` and a `Milestone: **<what>**` line, instead of a line in `docs/goals/reserved.md`. It commits that one file and pushes it to the current branch at once. A claim that loses the race to an earlier one on another branch removes its file and takes the next row. The row is then in the roadmap from its first commit, and no shared file is edited.
