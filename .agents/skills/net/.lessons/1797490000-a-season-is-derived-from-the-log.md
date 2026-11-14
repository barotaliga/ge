---
title: A ladder's seasons are worked out again from the master's log, so keep every input in it
date: 2026-11-13
scope: engine/account/ladder.cpp
concepts: [ladder, season, duo, reports-log, replay]
---

The ratings, the duos and the past champions are not stored as such: the master reads
`reports.log` back at start, and every report and every `duo` line in it, in order, rebuilds them.
So anything that changes a ladder must be a line in that log (a duo ask is, even one not yet
answered), and anything that only depends on the time (a season closing with no match in it) must
be done again by the same rule on the way back (`ladder_close_due` from the host's frame). The
season length is an input too: `host_open` sets it before the log is read, or a master started
with another `--season-days` would close different seasons.

A wrong password on the Account page holds the address for a while (the throttle), so a test that
posts several forms from one address puts its refusal last.
