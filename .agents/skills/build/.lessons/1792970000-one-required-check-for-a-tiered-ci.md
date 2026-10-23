---
title: A tiered CI needs one required check that passes on a skipped job
date: 2026-10-23
scope: .github/workflows/ci.yml
concepts: [ci, ruleset, required-checks, merge-queue, runners, paths]
---

On a free organization's shared runner pool, fifteen jobs on every pull request plus the same
fifteen again in the merge queue left PR runs queued for over an hour. Cutting jobs from the
pull request is only safe if the ruleset does not name them: a workflow skipped by `paths`, or a
matrix job skipped by a job-level `if`, leaves its required check pending forever (a skipped
matrix job reports under its unexpanded name). The fix is one aggregate job (`Required`) with
`needs:` on every job and `if: always()`, failing only on a `failure` or `cancelled` result, and
the ruleset requiring that one name. The matrix itself comes from the gates job's output
(`fromJSON`), so the pull request and the queue differ in rows, not in job names.
