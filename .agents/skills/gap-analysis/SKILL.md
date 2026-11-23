---
name: gap-analysis
description: "Use for an occasional audit of how far a ported classic game is from its reference (asked for by the owner, after a long run of fills, or when the reference moves to a new commit): re-check every row of the game's ledger against main, catch rows marked done that are not, add what the ledger never listed, compare behaviour black-box where reading cannot tell, and rank what is left. Ends with the ledger corrected in a PR and a report of the missing count before and after. Not for building a feature: that is port-from-ledger."
---

# Gap analysis: audit the ledger, not the game from scratch

A ported game's gaps live in its **ledger** (`port-from-ledger` skill): every thing the reference
has, marked `done`, `missing` (ranked) or `not-taken` against our tree (no `partial`: the `port-from-ledger` brief says why). Day to day, ports work
from it and turn rows `done`. Now and then the ledger itself is audited, because statuses drift:
a row was marked `done` by a name match, a later change broke a done row, the reference moved, or
a kind of thing was never listed. **A gap analysis audits the ledger.** It never starts a fresh
system-by-system comparison: those find only what their questions name, and each new one finds
"more gaps" the last one never looked for (`docs/porting-games.md` § Port from a ledger).

Load `clean-room` beside it (readers do the reading) and read the lessons first:
`make lessons ARGS=gap-analysis`.

## When

- The owner asks for one ("do a new gap analysis").
- A run of fill PRs has landed (say every five), or a release is near.
- The reference has a new commit we mean to follow.
- Play turns up something the ledger says is `done`.

## The passes

Run them in this order; each is a reader or a lab, never the implementer reading the source.

1. **Pin the reference.** The repository and commit in the ledger's header, confirmed with the
   owner if anything is in doubt (a fork is not its upstream). If the reference moved, a reader
   lists what changed between the two commits in the kinds the ledger holds; those become new,
   changed or dropped rows.
2. **Completeness.** A reader enumerates each kind again from the source's own tables (commands,
   settings with defaults, spawn table, items, prints, menus, protocol messages, flags, layout
   words) and diffs the ids against the ledger: every thing the ledger lacks is a new row. A kind
   the ledger has no table for gets one.
3. **Re-check the statuses** against `origin/main`, table by table, the `done` rows first: a
   `done` row must name the file that does it and a test or lab that holds it. A row with neither,
   or found only by a name, goes to `missing` with "verify: <what>". The `verify` rows are settled
   now, by reading our code or running it.
4. **Behaviour, black-box.** Where words cannot settle a row (a feel, a timing, what a player sees
   on screen), run the stock program and ours side by side in a lab (`docs/shooters-edge.md`
   § step 3: running and measuring is not reading). A lab that scripts the player (puts him on each
   key and exit) checks plumbing; a bot's or a recorded playthrough checks that a map can be
   played. Name which one each result is.
5. **Rank** each row still `missing`: 1 noticed at once in normal play, 2 noticed
   sometimes, 3 rare or cosmetic. The ranked gap list (`<game>-gaps*.md`) is regenerated from the
   ledger's rows, each naming its row ids, never written as a separate list.

## What it delivers

- The ledger corrected in one PR: new rows, changed statuses, the counts and totals recounted, a
  dated note at the top of what the audit changed (rows added, rows found not done, rows dropped).
- A report: the missing count before and after the audit and why it moved ("956 missing; the audit
  found 12 rows marked done that are partial and 30 settings the ledger never listed: 998"), the
  rank-1 rows, and which ones play surfaced.
- Lessons on why rows were wrong (`skill-reflection`): a mistake the ledger's brief allowed is
  fixed in `port-from-ledger`'s `references/ledger-brief.md` so the next ledger does not make it.

## Rules

- Readers save each table as they go; a stopped reader resumes on the tables left.
- Readers judge against `origin/main`; work in a branch stays `missing, in progress`.
- Never shrink the ledger to make the count go down: a row leaves only as `not-taken` with a doc
  that decides it, or when the reference no longer has the thing.
