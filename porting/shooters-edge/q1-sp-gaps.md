# Quake single player: the gaps to 1-1, and what has landed

Niclas, 2026-11-20: Quake's single player must play 1-1 with the original game, alone and in
co-op. With the player's packs our campaign runs the pack's own single-player program unchanged
on our VM (`client/game_folder.cpp`'s `load_programs`), so the game's rules are the original's by
construction; every difference is in what the engine gives the program and how the client shows
it. Two clean-room audits list them, ranked 1 (noticed at once on the stock campaign), 2
(sometimes) and 3 (rare or cosmetic):

- `q1-sp-host.md` (audit A): the physics frame, movetypes, pushers, the monster-movement and
  sight builtins, touch, sounds the engine starts, setmodel, cheats (24: 0 / 4 / 20).
- `q1-sp-flow.md` (audit B): the episodes, the screen's tints, the intermission and finale
  screens and camera, the death view, the centre print, pause, the console, music, the attract
  loop, saves, co-op over the network (22: 4 / 11 / 7), with a follow-up on the tints.

Rows are named `A<n>` and `B<n>` by audit and row. Work goes rank 1 first, many rows to a PR
(Niclas, 2026-11-20: as much as possible in each). Our own C++ campaign rules
(`games/shooters_edge/rules/campaign*`) run only without a pack program and are not audited here.

## Landed

| Date | PR | Rows |
| --- | --- | --- |
| 2026-11-20 | #1207 | B1, B2, B3, B5, B6, B7, B8, B9, B10, B11, B13, B14, B16, A1, A4, A19 (the cheats' words and flags), B4 in part |
| 2026-11-20 | #1216 | B12, B15, B17, B18, B19, B21 (the loading picture), B22, B4's rest, A3, A19's rest |

## In progress

Round 3 (the third PR): A2 (the brush models a unit wider on each side for a game's own content,
`CampaignOptions::wide_models`: doors, buttons and secret doors travel 2 more, a plat without a
height drops 2 more, a train rides a unit off its corners, a trigger reaches a unit further; the
fixture campaigns keep their bounds, so the co-op goldens stand), B20 (a keyboard player's pitch
drifts to the slope from rest, faster by `v_centerspeed` each second, stopped by a hand on the
pitch and started again by running forward for `v_centermove`, `centerview` or letting +mlook go
with lookspring), B21's rest (the console's level banner, `startdemos`, `stopdemo`), A16 (a
single-player program's setmodel gives an alias model a 32-unit cube and a sprite its half
sizes), A17 (every sound in the campaign starts at its entity's box centre), A18 (every sound in
the campaign goes to every player, the distance alone silencing it).

## Next

- The rank 3 rows of audit A left: A5 (pushers sweep), A6 to A15, A21 to A24; the shared physics
  rows (A5, A6, A7, A10, A14, A15) also reach QuakeWorld's server and its goldens, so each needs
  the same row answered for that game first. `demos` keeps listing the recordings (its meaning in
  the other games).
