---
title: Filling a row re-checks the done rows its facts touch
date: 2026-11-22
scope: porting/shooters-edge/q1-qw-ledger.md
concepts: [ledger, done-row, partial, ammo-boxes, cl-sbar, recount]
---

Building the old ammunition box (`item_weapon`) from its facts showed that the big boxes it shares
models with lay as the small model, though the ammunition row (G86) was `done` by its amounts; the
facts for `cl_hudswap` showed that `cl_sbar` (C348), `done` by its name, is QuakeWorld's overlay of
columns, which `hud_sbar 0` is not. A row marked done by a name or by one of its numbers hides the
rest. When a reader's facts for a missing row describe a neighbour, check that neighbour against
them too: a new row for what was never listed (G246), `partial` with the gap named for a done row
that falls short (C348), and the missing count reported with both, even when it goes up.
