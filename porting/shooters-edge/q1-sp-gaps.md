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

## In progress

Round 1 (the first PR): B1 (episodes all open and no runes from earlier games; Niclas,
2026-11-20, chose the original's; the in-order flow kept as `sv_episodes_in_order`), B2 (powerup tints), B3 (the pickup flash: the program's stuffed `bf`, and
every other stuffed line run in the console), B5 (the damage tint), B6 (the contents tint by the
map at the eye), B7 (the intermission undimmed), B8 (the finale's picture, placement, 40-character
lines), B9 (the intermission camera: the mouse held, the idle sway, no weapon), B10 (dead: the
solo panel, the weapon gone, the program's eye), B11 (the classic look's centre print at 35%,
cut at 40), B13 and the cheats of A19 (`god`, `notarget`, `noclip`, `fly`, `give`, `kill`,
`restart`, `changelevel`), B14 (the program's tracks), B16 (the level time is the server's clock),
A1 (the splash and the landing thud the physics starts), A4 (co-op with another person never
pauses for the host's menu), B4 in part (a joiner is sent the intermission and track in the
protocol's form and the true totals; his window still draws no campaign level screens).

## Next

- B4's rest (the joiner's window draws the intermission, finale and solo panel), B12 (`pause`,
  `pausable`, the pause picture), A19's rest (noclip touching triggers, fly sliding), A3 (level
  loops on their own channels), B15 (the attract loop), B17 and B18 (save slots' words, NEW
  GAME's question), B19 (a one-player co-op game saves), B20 (pitch drift), B21, B22 and A20
  (loading plaque, console banner, `host_framerate`).
- A2 (brush models a unit wider, which moves the set pieces and the co-op goldens), then the
  rank 3 rows of both audits.
