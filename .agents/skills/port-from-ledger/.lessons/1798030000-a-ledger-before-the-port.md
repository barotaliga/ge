---
title: A ledger before the port: three audits of Quake II kept finding more until one was written
date: 2026-11-21
scope: porting/shooters-edge, games/shooters_edge
concepts: [ledger, gap-analysis, reference, fork, reader, usage-limit, checklist, missing-count]
---

Quake II was ported system by system, then audited three times; each audit found dozens of new
gaps because each reader answered only the questions it was given. The full ledger (2,316 rows,
956 missing) ended that. What it cost to get there:

- **The first client and server ledgers were read from a mod's fork** of the reference the owner
  chose (the link in the notes named the fork). 94 rows were the fork's own and the muzzle flashes
  were the mod's. Pin the repository AND the commit with the owner before a reader starts.
- **Three readers stopped on a usage limit before writing a line**; the restart had them write
  table by table, so a stop costs only the table in hand.
- **A status found by a name match is a guess**: the readers marked such rows `verify`, and the
  client reader, rechecking against the upstream, kept the unchanged rows' statuses unchecked; say
  so in the report rather than present the count as exact.
