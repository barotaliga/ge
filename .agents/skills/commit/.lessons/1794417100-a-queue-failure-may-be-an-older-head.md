---
title: A merge-queue failure that a pushed fix did not move may be the queue testing an older head
date: 2026-11-11
scope: .github/workflows/ci.yml
concepts: [merge-queue, dequeued, stale-head, digest, windows-ci, gh-readonly-queue]
---

A PR re-armed right after a push can be queued on the head before the push: the queue's
`gh-readonly-queue/main/pr-<n>-<base>` commit is what CI tests, not the PR's head. When a
queue run fails exactly as the run before the fix did (the same digest, byte for byte), read
the file at the queue run's `head_sha` (`gh api repos/<o>/<r>/contents/<path>?ref=<sha>`)
before hunting a platform difference. If it lacks the fix, merge `origin/main` into the branch,
`make precheck`, push, and arm auto-merge again: the new head needs its own pull-request
checks (`ge ci <pr>` says "0 checks on <head>" until it has them).

To read a digest mismatch from CI's log alone: the digest is FNV-1a over the ledger the lab
writes as `digest.tsv` (each metric's key with its NUL and its double's bytes, each shot's id
with its NUL), so a script over the local ledger can test which change (a shot missing, a
metric's value) gives CI's number.
