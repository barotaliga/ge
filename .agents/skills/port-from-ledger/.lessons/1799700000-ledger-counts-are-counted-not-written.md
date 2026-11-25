---
title: A ledger's counts are counted from its rows, never written in it
date: 2026-11-23
scope: porting/ledger-count.mjs
concepts: [ledger, missing-count, totals, merge-conflict, make-ledger]
---

Every ledger used to carry a totals table, a count line under each table and a "now N missing"
sentence. Each PR that flipped a row edited them, so any two open PRs on one ledger conflicted,
and the hand counts drifted from the rows (the Quake II game ledger still said "partial 13" after
the partial grade was gone). The counts now come from the rows alone: `make ledger` (and
`ARGS=--missing`) reads each table's `status` and `rank` columns. Flip the row and nothing else;
report the count `make ledger` prints on main and on the branch.
