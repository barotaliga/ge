---
title: Grep the tree for a missing row's words before building it; other threads fill rows too
date: 2026-11-21
scope: porting/shooters-edge/q1-qw-ledger.md
concepts: [ledger, stale-row, parallel-threads, snap, heartbeat, recount]
---

Six "missing" rows of the Quake ledger (`snap`, `snapall`, the upload message, the refused
screenshot, `heartbeat`) had been built by other threads since the ledger was written. Before a
fill, grep the tree for each missing row's command or setting name (a loop over the names prints
the files that hold each in seconds), read what you find, and flip the rows that are already done
with the file that does them. The count then says what is really left, and nobody builds a row
twice.
