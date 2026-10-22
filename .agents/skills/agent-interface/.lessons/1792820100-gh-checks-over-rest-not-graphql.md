---
title: Read a PR's checks over gh's REST calls; a check run's id is its job's id
date: 2026-10-22
scope: engine/cli/ci.cpp
concepts: [gh, github-actions, ci, check-runs, job-log, rest]
---

`gh pr checks` and `gh run view --log-failed` go through GitHub's GraphQL endpoint, which a
proxied or narrowly scoped token can refuse (a 403) while REST works. `ge ci` therefore uses `gh
api` alone: `repos/O/R/pulls/N` for the head sha, `commits/SHA/check-runs` for the checks, and
`actions/jobs/ID/logs` for a failed one's log, where ID is the check run's own id (an Actions
check run and its job share it). The raw log puts an ISO timestamp ending in `Z ` before every
line and a BOM before the first; strip both before matching a finding line. Test the reader on a
saved log (`--log`), never on the network.
