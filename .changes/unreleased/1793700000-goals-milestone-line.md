---
type: Changed
title: ge goals reads a row's name from its status file's Milestone line
---

`ge goals free` and `ge goals claim` name a row from the bold text of the `Milestone:` line in its status file, `docs/goals/<row>.md`, when the file has one, on the base, on every branch and in the working tree. The row's table line in `docs/goals.md` still names it while the table exists. So the roadmap's table can be generated from the row files without `ge goals` losing a single name.
