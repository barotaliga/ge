---
title: A line every pull request edits conflicts every parallel pull request
date: 2026-10-22
scope: AGENTS.md, docs/goals.md, tests/labs_digests.txt
concepts: [merge-conflict, hot-spot, parallel-threads, auto-merge, digests]
---

With several threads open at once, a pull request that edits a line every other pull request
also edits turns red on each merge to main, and auto-merge then waits on it forever. Three lines
did this. The first was AGENTS.md's "Where things live" rows, which listed every file. They now
name directories only, at most 240 characters, and `make agents-check` holds them to that. The
second was a milestone row in `docs/goals.md` that gains a clause per change. The third was the
`cli-loop` row of `tests/labs_digests.txt`, which moves whenever a `ge` command or flag is added.
Before you edit a shared line, check whether a new line or a file of its own would do. After a
conflict on a digest row, take main's file and rerun `ge digest <lab> --write` on the merged
tree. Never pick either side's hash.
