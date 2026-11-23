---
title: A queued PR with no merge_group run is waiting its turn; read the others' enqueue times before calling it stalled
date: 2026-11-23
scope: .github/workflows/ci.yml
concepts: [merge-queue, enqueue-order, stalled-queue, replacement-pr]
---

The queue runs one entry at a time, first in, first out, at about 15 minutes each, so with six
entries ahead of it a PR shows no `gh-readonly-queue/main/pr-<n>-…` run for an hour and a half.
PRs merging "past" it were queued earlier. Before you call an entry stalled, list
`gh api repos/<owner>/<repo>/issues/<n>/timeline` for the PRs that merged since and compare their
`added_to_merge_queue` times with yours. A replacement PR, or a removal and re-add, sends the work
to the back of the queue. A queued branch refuses every push, so if the PR also conflicts with
main by its turn, only a replacement from a new branch takes the merge (see #1419 and #1427).
