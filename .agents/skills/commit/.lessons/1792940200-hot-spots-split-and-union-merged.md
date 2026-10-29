---
title: The roadmap and the digests no longer share lines between threads
date: 2026-10-23
scope: docs/goals.md, docs/goals/, tests/digests, .gitattributes
concepts: [merge-conflict, hot-spot, parallel-threads, union-merge, status-file]
---

Two of the hot spots in the lesson above are fixed in the tree's structure. A milestone's status
is its own file, `docs/goals/<row>.md`. The table's cell is only the status word linking it, and
`tools/check-goals.mjs` holds the two to each other. The `ge` wishes are
`docs/goals/ge-backlog.md`, one a line, merged with git's union driver. `tests/labs_digests.txt`
is union-merged too. After a merge, a lab that both sides moved has two rows, and the labs'
digest check fails naming it. Run `ge digest <lab> --write` to keep one. Write a status into the
row's file, not into the table, and add a wish as a new line, never by editing another's line.
