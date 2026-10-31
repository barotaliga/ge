---
title: a quiet merge hides its conflict, and precheck passes on the conflicted tree
date: 2026-10-22
scope: tools/precheck.sh, AGENTS.md
concepts: [merge, conflict, precheck, push, agents-md]
---

`git merge -q origin/main` prints nothing when it stops on a conflict, and `make precheck`
still passes on the half-merged tree (it builds the index's files, and a conflicted Markdown
table is not code). The push that follows sends only the commits that already existed, so the
pull request stays conflicted while it looks updated. Merge without `-q`, then run `git diff
--name-only --diff-filter=U` and resolve every file it lists before precheck. The usual
conflict is one row of AGENTS.md's table that two pull requests both extended: keep main's row
and add back this branch's words to it, never take one side whole.
