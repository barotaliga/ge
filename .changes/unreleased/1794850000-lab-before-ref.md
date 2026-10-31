---
type: Added
title: ge lab takes its before run from a git ref
---

`ge lab <id> --before=<ref>` builds the lab at a git ref in a scratch worktree (`build/before/<sha>/`, kept for the next call at that commit), with this build's `GE_*` options and its downloaded dependencies, runs it into `previews/<id>@before/`, then runs this tree's lab compared with it: the before-and-after without a checkout, rebuild and restore. With ccache on PATH the two trees share the cache. A ref that names no commit is refused before anything builds, and a failure at the ref says it was the ref's.
