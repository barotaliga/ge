---
title: A push after a queue ejection may run no pull_request CI; the queue then drops the PR for a missing `required`
date: 2026-11-05
scope: .github/workflows/ci.yml
concepts: [merge-queue, ejection, required-check, pull-request-ci, empty-commit, skip-ci, auto-merge]
---

- PR 545 was ejected (a GCC `-Wshadow` failure), fixed and pushed, and auto-merge re-armed. The
  push started only the `pull_request_target` labeller, no `pull_request` CI, so the new head had
  no `required` check at all; the PR waited in the queue for 40 minutes and was removed with no run
  of its own (`removed_from_merge_queue` by the queue's bot, nothing failed).
- After every push, confirm a `pull_request` run started for the new head
  (`gh run list --branch <branch>`: event `pull_request`, its sha). If none, push an empty commit
  (`git commit --allow-empty`); its run reported `required` in a minute and the queue merged the PR
  at once.
- A PR's `mergeQueueEntry` null with no merge and no failed run of its own is this case, or the
  queue dropping the PRs behind a failed group; `gh api repos/<o>/<r>/issues/<n>/timeline` tells
  which.
- The same happens when a draft is marked ready seconds after a push: PR 624's push, then
  `ccr/ready_for_review` at once, started the `ready_for_review` run on the PREVIOUS head, so the
  new head had no `required` and sat in the queue with no group run. Converting to draft and back
  started nothing (a ready from that route runs no workflow). Push, confirm the `pull_request` run
  shows the new sha, and only then mark ready; when it is missing, the next real commit (a lesson,
  a doc line) starts it.
