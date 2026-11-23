---
title: No partial grade: it hid 416 Quake II rows that were mostly done or decided
date: 2026-11-23
scope: porting/shooters-edge, .agents/skills/port-from-ledger, .agents/skills/gap-analysis
concepts: [ledger, partial, regrade, missing-count, rank, not-taken, player-text, lift]
---

The Quake II client and server ledgers carried 287 and 129 `partial` rows while the Quake ledger
had one. The owner asked whether all of them had to be built. Sorted by the reason in each note,
they were: 78 lines saying the same thing in our words, 58 rows our own design replaces on
purpose, 35 menu rows lit since their settings landed (the ledger was never updated), 43 found by
name only, a 137-row tail (cosmetic, mission-pack effects, debug and admin extras, hosted game
libraries' edge cases) and only 65 real gaps.

- **Grade done, missing with a rank, or not-taken.** `partial` mixes "works, worded our way" with
  "broken where it matters", so its count says nothing about work left. The brief now forbids it.
- **A regrade raises the missing count once.** Say so in the PR: hidden gaps became rows; the
  count only goes down from there.
- **The owner decides the tail.** Put the cosmetic and niche rows to the owner as one choice
  (not-taken, rank 3, or leave), not row by row.
- **Note where the other game already has it** (`LIFT:` in the note) so the gap is built once in
  the shared layer, and tell the settings-lift work.
