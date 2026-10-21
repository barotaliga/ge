---
title: make index run mid-conflict lists a conflicted file once per stage
date: 2026-10-21
scope: docs/index.md, tools/agents-index.mjs
concepts: [index, rebase, merge-conflict]
---

The index reads git's file list, and while a rebase or merge has a file conflicted
(`tools/check-plugins.mjs` here) that file is listed once per stage, so its row lands
in `docs/index.md` three times and CI's agents job calls the index stale. Rerun
`make index` after `git rebase --continue` (or the merge commit), not before it.
