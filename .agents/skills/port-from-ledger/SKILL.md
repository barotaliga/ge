---
name: port-from-ledger
description: "Use before building ANY piece of any classic game in Shooter's Edge (Quake, QuakeWorld, Quake II, and the games after them: Quake III Arena, Doom) (a command, a setting, a menu page, an effect, a monster, a server feature, a whole system): find the piece's rows in the game's checklist ledger (porting/shooters-edge/<game>-ledger-*.md), check whether a game already ported has the feature and lift it to the shared layer instead of building it again, build every missing and partial row of that area in one PR, mark them done in the ledger, and report the missing count. Also owns making a ledger for a game or a system that has none (reader subagents over the chosen reference), keeping it honest, and why a port never runs a gap analysis afterwards."
---

# Port from the ledger: the checklist first, never an audit after

A classic game is ported against a **ledger**: one line for every thing the reference has that a
player, an admin or a mod can see (each command, setting with its default, entity, item, monster,
message word for word, menu row, protocol message, flag, layout word), each marked against our
tree `done`, `missing` (ranked) or `not-taken` (there is no `partial`: see the brief). The ledger is written ONCE, before the work, by
clean-room readers, and committed. Every port then works FROM it: the rows of the area you build
are your spec and your test list, and your PR turns them `done`.

**Why.** Quake II was ported system by system and then audited three times. Each audit found
dozens of new gaps, because each compared only what its questions named: a reader answers what it
is asked, nothing more. A complete list from the reference ends that: the missing count can only
go down, and nobody has to ask "what else is there?" again (`docs/porting-games.md` § Port from a
ledger).

Load `clean-room` beside this skill whenever a reader is needed, and the skill of the subsystem
you build. Read the lessons first: `make lessons ARGS=port-from-ledger`.

## 1. Find your rows

| Game | Ledger |
| --- | --- |
| Quake and QuakeWorld (client, server, game, tools) | `porting/shooters-edge/q1-qw-ledger.md` (rows `C`, `S`, `G`, `T`); the game rows' facts `qw-dm-rules.md` |
| Quake II, the game rules | `porting/shooters-edge/q2-ledger-game.md` |
| Quake II, the server | `porting/shooters-edge/q2-ledger-server.md` |
| Quake II, the client | `porting/shooters-edge/q2-ledger-client.md` |
| The mission packs (Scourge of Armagon, Dissolution of Eternity, The Reckoning, Ground Zero) | `porting/shooters-edge/mission-packs-ledger.md` (rows `imp`, `hip`, `rog1`, `xat`, `gz`) |
| A game with no ledger yet (Quake III Arena, Doom, any later one) | write it first: § 5 |

The row ids are stable (`cmd.give`, `cvar.sv_gravity`, `ent.func_door`, `msg.obit.rocket`,
`menu.options.video`, `svc.muzzleflash2`, `dmf.1024`): `grep -n "cvar.cl_" <ledger>` lists an
area. The ranked gap lists (`<game>-gaps2-*.md`) order the work by what a player notices first;
the ledger is what is complete. A gap list's row names its ledger rows and the other way round.

Take a WHOLE area at once (every row of the menu's options page, every muzzle flash, every server
setting of the rate family), not one row: the owner wants big PRs, and an area built together
shares its tests and its lab.

## 2. Already in another game? Lift it, never build it twice

Before building ANY row, in any game, ask: does a game we already ported have this feature or
setting (Quake, QuakeWorld, Quake II, and every game after them)? A newer game's row is often an
older game's feature under another name, and the older game's code may sit in that game's own
module (a QuakeWorld server's print filter, a Quake client's demo pause).

1. **Look.** `make find Q="<the setting, the command, the words it does>"`; the row's own note
   (`LIFT:` and "a <game> name for a setting ours has" point at what exists); the other games'
   ledgers for the same thing; `porting/shooters-edge/common-settings.md` for a setting; the
   system's page under `docs/porting/`.
2. **It exists: lift it.** Move the code to the lowest layer whose games all mean the same by it
   (engine, shooter kit, `plugins/classic`, never one game's module), keep the first game's
   behaviour bit for bit, and let the new game reuse it. Only what truly differs stays per game,
   as data or a flag (Quake II's print filter is QuakeWorld's without chat's exception). A setting
   becomes one setting in our name, the new game's spelling a synonym (with a value map when only
   the values differ), its default a `same` row (`docs/porting-games.md` § One name, one meaning).
3. **It does not exist: build it in that same lowest layer** when another game would want it, so
   the next game reuses it; only behaviour no other game has goes in the game's module.
4. **Make it work in every game it serves.** Test the lifted piece in the new game AND in the
   games that had it. A bug found while porting is fixed in the shared piece, so every game gets
   the fix. When the new game does it better, its way becomes the shared one and every game
   improves.
5. **Never change how an old game feels.** A lift or a better way is an improvement only if a
   player of the old game would notice nothing but the fix: the same moves, timings, numbers,
   words and look by default. Hold it with the old game's goldens and labs before and after
   (`labs` skill). A difference a player would feel (a feel, a default, a timing) stays per game
   as data or a flag, each game keeping its own by default, and the owner decides whether an old
   game takes it.
6. **Say so.** The row's note names the shared piece it reuses or lifted, and the PR says which
   game's feature was lifted, which fixes or improvements reached the other games, and the
   before-and-after that shows their feel unchanged.

## 3. Build each row from facts, never from the source

- A row says what the thing does in our words and quotes what a player reads. When that is not
  enough to build it, ask a reader a written question about those rows (`clean-room` § questions);
  its answers go into the row's note or a `porting/` facts page the row links.
- Place each piece by `docs/porting-games.md` (engine, shooter kit, classic plugin, game) and name
  each setting by § One name, one meaning; a shared piece another game has is used, not copied.
- Before building a row that more than one game would have (a setting, a console command, a HUD
  element, a server feature), find who else is building it: another game's ledger may list it, and
  another thread may have it in hand. Agree on one builder, build it once in the lowest layer whose
  games all mean the same by it, and mark the row in every game's ledger that it serves. Only
  behaviour no other game has goes in the game's own module.
- A row is `done` only when a test or a lab holds it as a player would see it: the exact words, the
  default, the menu row's place. A name in our tree is not proof.

## 4. Mark the ledger in the same PR

- Something found while filling (a refusal, a wrong default, a crash) gets a new row first, then
  its fix, so the count stays the whole truth.
- A game's own rule on top of shared rules goes behind one switch its profile sets, so the shared
  rules' goldens and digests do not move for the other games.

- Each row built: `done`, the last column the file of ours that does it. A row you find wrong
  (ours already did it, the reference does something else): fix it and say so in the PR.
- `not-taken` only with a doc that decides it (name the doc), never because it is hard.
- Never write a count into a ledger: no totals table, no count line under a table, no "now N
  missing" sentence (hand totals went stale and conflicted in every PR that touched a ledger).
  `make ledger` counts the rows (`make ledger ARGS=--missing` prints each ledger's missing count by
  rank); run it on `origin/main` and on your branch and put both in the PR description and the
  report ("Quake II ledger: 956 missing, now 902").
- Update the system's page in `docs/porting/` and `ledger.md` as `porting-games.md` asks.

## 5. A game or a system without a ledger

Before the first line of a new game's port (or a system the ledger lacks), write the ledger:

1. **Name the reference with the owner**, by its repository URL and commit, and write it in the
   ledger's header. A fork is not its upstream: Quake II's client and server ledgers were first
   read from a mod's fork, and 94 rows were the fork's own.
2. **One reader per half** (the game's rules, the server, the client), each with the brief in
   `references/ledger-brief.md` and one row shape for all (id, the name a player sees, its
   meaning in a line, the status, our file or why not; a rank 1 to 3 where the order matters):
   enumerate EVERYTHING by kind, mechanically, from the source's own tables
   (the command table, the setting registrations, the spawn table, the item list, every print),
   not from memory of the game.
3. **Readers write table by table to the file**, so a stopped reader (a usage limit, a timeout)
   leaves its finished tables; restart it on what is left.
4. **Mark against `origin/main`** (the readers read our tree with `git show origin/main:<path>`),
   so work still in a branch reads `missing, in progress` until it lands. Fold the facts pages
   already in `porting/` into rows instead of reading the source again.
5. **Check what they wrote**: the screen for code (`clean-room`), no paths under a home directory,
   rows whose status was taken by a name match marked `verify`.
6. Every table has a `status` column (and a `rank` column where missing rows are ranked), so
   `make ledger` counts it; name the file `<game>-ledger-<half>.md` (or pass it to `make ledger`).
   Write down the starting missing count once, in the header, so progress reads as missing going
   down; no other count goes in the file. Commit the ledger with the work that starts from it and
   report `make ledger`'s counts.

## What is not a check

- **A scripted lab is not a playthrough.** A lab that puts the player on each key and exit tests
  the plumbing (joins, carries, level changes), not that a person or a bot can get through. Say
  which it is wherever its result is reported.
- **A status found by name is a guess.** A setting with the same name may mean another thing in
  our tree (`docs/porting-games.md` § One name, one meaning); check its default and behaviour.
