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

## In progress

Round 2 (the second PR): B12 (`pause` for the campaign with who paused it, `pausable`, the pause
picture and `showpause`), B17 (the save slots' "--- UNUSED SLOT ---", the 39-character comment
with the kills at column 22, "Saving game to" and "Loading game from"), B18 (NEW GAME asks first
while a game runs), B19 (a co-op game with one player saves), B15 (a recording that ends under
the menu waits for it to close, then the next plays), B21 in part (the loading picture), B22
(`host_framerate`), B4's rest (a joiner's window draws the intermission, the finale and the pause
picture from the protocol's messages), A3 (a level's loop that lost its sound channel starts again
when one frees), A19's rest (a noclipping player touches triggers; a flying one slides along
walls without gravity).

## Next

- B20 (pitch drift by `v_centerspeed`, `v_centermove`, `centerview`), B21's rest (the console's
  level banner, `startdemos`, `stopdemo`, `demos` back to the loop: `client/main.cpp` split
  first, it is at the line cap), A20.
- A2 (brush models a unit wider, which moves the set pieces and the co-op goldens), then the
  rank 3 rows of both audits.
