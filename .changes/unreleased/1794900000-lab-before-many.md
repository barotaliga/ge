---
type: Added
title: ge lab before keeps several labs' befores from a git ref
---

`ge lab before <id>...` runs each named lab at `origin/main` (or `--ref=<ref>`), built once in the scratch worktree `ge lab --before` uses, at its smoke flags (`--full` for its full ones), and keeps each run as `previews/<id>@before/` (or `--tag=<name>`). The after is then `ge lab <id> --compare=before`, or `ge digest <id> --why --base=before`, with nothing stashed.
