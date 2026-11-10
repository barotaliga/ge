---
type: Changed
title: ge lab before shows a lab's digest at the ref
---

`ge lab before <lab>` now prints the lab's smoke digest at the ref (origin/main by default) beside the baseline that ref commits in `tests/digests/`. When the two differ, it says the baseline was stale there already, so a digest a change did not move is told apart from one main left stale. `ge help digest` points to it.
