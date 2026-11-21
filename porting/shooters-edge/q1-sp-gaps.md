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
| 2026-11-20 | #1221 | A2 (for a game's own content), B20, B21's rest, A16, A17, A18 |
| 2026-11-21 | #1226 | A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A21, A22, A24 (on the single-player clock alone) |

## Co-op on every map

The `se-coop-sweep` lab puts two players in a co-op game of the pack's own program on each of the
32 campaign maps (`start`, the four episodes, `end`): both join, the second dies to the program's
own damage and stands again, leaves and rejoins, both take every key where it lay, the first walks
into each exit and both reach the next map alive with the weapons they carried (the keys stay
behind, as in the original), and on `end` the last foe falls and the ending plays for both. Every
map passes with no program fault (2026-11-21). The one fault the bot episodes showed (e2m1's
doors "cross connected") was the `se-coop-episode` lab's own: it sized the brush models as built,
not a unit wider as the window does for a game's own content; it now does.

## Kept by choice

- A20: the campaign steps at a fixed 1/72 s, the original's fastest frame, so a run replays to
  the bit; no `host_framerate`.
- A23: co-op can be switched on during a map and the program's globals follow at once, so a
  single-player game opened to friends plays as co-op without a restart.
- `demos` keeps listing the recordings (its meaning in the other games).
