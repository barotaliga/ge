# The ledger reader's brief

Give one reader per half (the game's rules, the server, the client) this brief, with the
reference's local path, its repository and commit, the half it reads, and the one file it writes
(in scratch, never in the tree).

> You are a clean-room reader (`clean-room` skill): you read the reference, you write facts in your
> own words, and no code, identifier-as-code or comment leaves the source. Names a player or an
> admin types or reads (commands, settings, messages quoted exactly) are facts and are wanted.
>
> Build a COMPLETE checklist of your half: one line per thing a player, a server admin or a mod
> can see. Enumerate it mechanically from the source's own tables (the command registrations, the
> setting registrations with their defaults, the spawn table, the item list, every print and
> centre print, every menu row, every protocol message, every flag bit, every layout word), never
> from memory of the game.
>
> One Markdown table per kind, nothing between its rows:
>
> | id | thing | what it is / does (one line, our words) | status | ours / note |
>
> - id: stable and short: `cmd.give`, `cvar.sv_gravity`, `ent.func_door`, `item.weapon_railgun`,
>   `mon.monster_tank`, `msg.obit.rocket-direct`, `menu.options.video`, `svc.muzzleflash2`,
>   `dmf.1024`.
> - status: exactly one of `done` (ours does the same as a player would see it; the same meaning in
>   our own words counts, the player-text rule makes our words the intended ones), `missing` (ours
>   lacks it or differs where a player would notice: the note starts with its rank, 1 noticed at
>   once, 2 sometimes, 3 rare or cosmetic, and says what differs), `not-taken` (only where one of our
>   docs or the owner decides it: name the doc, or "our design: <what replaces it>"). There is NO
>   `partial`: it hid 416 Quake II rows (our words, our design, unchecked names) behind one grade.
>   Judge against `origin/main` (`git show origin/main:<path>`); work still in a branch is
>   `missing, in progress`. A row found by name only: check it now by reading our code; if that
>   cannot settle it, `missing` with "verify: <what>".
> - ours / note: the file of ours that does it (a path from the repository's root, never a home
>   directory), or the gap in a few words, and the ranked gap list's row (R7, S3, C12) where one
>   describes it. Fold the facts pages already in `porting/` into rows.
>
> Under each table's heading a count line: `done N, missing N, not-taken N`. At the top
> of the file: the reference's repository and commit, and a totals table of the four counts per
> kind. Write the file table by table as you finish each, so a stop loses nothing. When done, reply
> with the totals table only.
