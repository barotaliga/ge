# The baseline mod

Shooter's Edge's game logic as a QC mod: a complete deathmatch that the game ships in source,
for modders to copy and change. The server runs it as it runs any multiplayer `progs`; nothing
in the engine knows it is ours.

## Build it, run it

```sh
ge qc build games/shooters_edge/qc/progs.src        # writes progs.dat and progs.dat.lines here
ge qc disasm games/shooters_edge/qc/progs.dat --function=damage
make lab LAB=se-baseline ARGS="--bots=4 --timelimit=2"   # bots play it on the test arena
```

`progs.src` names the output, then the sources in the order they are compiled. Inside this
repository, `make lab LAB=se-mod ARGS="--progs=games/shooters_edge/qc/progs.dat"` runs a build
of it like any other mod, and a server takes it with `--progs=<file>` or `sv_progs <file>`
(`docs/shooters-edge.md` § Running your own mod). The built files are not committed.

The language is written down in `porting/shooters-edge/qc-language.md`; the compiler is
`plugins/progs/qc_compile.hpp`. A mistake is reported at its file and line.

## The files

| File | What is in it |
| --- | --- |
| `defs.qc` | The engine-side globals and fields (in the order the engine reads them: never reorder), the builtins this mod calls, the constants, the mod's own fields and globals |
| `util.qc` | Helpers: removing an entity later, a mover's straight move (`calc_move`), editor angles, firing targets (`use_targets`), effects |
| `combat.qc` | Damage (armour first, then health, the push), blasts (`radius_damage`), frags and obituaries |
| `weapons.qc` | The eight weapons: costs, rates, ranks, impulses, and how each fires |
| `items.qc` | Health, armour, ammunition and weapons on the map, picked up and coming back |
| `player.qc` | Joining, spawn points, the frame around each command, pain, death, respawn, water, slime and lava, the kill command, leaving |
| `world.qc` | The world, the frag and time limits, the map's entities: spawn points, walls, doors, plats, buttons, triggers, teleporters |

## The game

- **Weapons** (impulse 1..8, 10 the next one carried): blade (no ammunition), scattergun
  (1 shell), double scattergun (2 shells), spiker (1 nail), heavy spiker (2 nails), grenade
  launcher (1 rocket, a bouncing grenade), rocket launcher (1 rocket), arc gun (cells, a beam).
  A player spawns with the blade, the scattergun and 25 shells. A weapon out of ammunition gives
  way to the best loaded one.
- **Items**: health 15, 25 and 100 (the big one may go over the maximum to 250 and decays back
  a point a second), armour at 30 %, 60 % and 80 % of the damage, ammunition boxes (spawnflag 1
  doubles one), every weapon. Items come back after 20 s, weapons after 30 s.
- **Deaths**: a kill is +1 frag, a death by one's own hand or the world's −1, each with an
  obituary. A dead player respawns on a free spawn point when he presses a button after a
  second, or by himself after five.
- **The match**: `deathmatch`, `fraglimit` and `timelimit` are read from the server's cvars every
  frame; when a limit is hit the scores are printed and the level changes five seconds later.
- **The map's entities**: `info_player_deathmatch`, `info_player_start`, `func_wall`,
  `func_illusionary`, `func_door` (opens for a player near it, or when triggered), `func_plat`,
  `func_button`, `trigger_multiple`, `trigger_once`, `trigger_teleport` with
  `info_teleport_destination`, `trigger_push`, `trigger_hurt`, and every item and weapon.

## Changing it

- **Keep `defs.qc`'s first two parts as they are.** The engine reads those globals and fields by
  offset; the compiler checks them by name, type and place and writes the layout's crc only when
  they match, and a server refuses a progs without it. Add your own after `end_sys_fields`.
- **Order matters**: a function must be declared before it is called. Put a prototype
  (`void() my_think;`) near the top when two functions call each other.
- **`self` and `other`** are the entity a hook or a think is for, and the one touching it. A
  function that changes `self` to call another puts it back before returning.
- **Sounds and models** are named by the classic content paths; the game maps each to a free
  replacement (`games/shooters_edge/content/`).
- **Check your change**: `ge qc build` until it compiles clean, then the `se-baseline` lab before
  and after (`--tag=before`, then `--compare=before`). Inside this repository the mod is also
  held by `tests/shooters_edge_baseline_mod_test.cpp`.
