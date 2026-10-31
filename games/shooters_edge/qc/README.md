# The baseline mod

Shooter's Edge's game logic as QC mods the game ships in source, for modders to copy and change.
One set of sources builds two VARIANTS:

- **`arena/`**: the deathmatch, on the multiplayer program form. The game runs it on its own
  maps and the server runs it as it runs any multiplayer `progs`.
- **`campaign/`**: the classic maps' single-player game, on the single-player form, played by the
  campaign host as it plays any campaign program (porting notes: `campaign-rules.md`). Today it
  has the campaign's flow (starts, exits and the intermission, secrets, counters, relays, skill,
  episode gates, runes, keys, the inventory carried from map to map); the monsters come next.

Nothing in the engine knows either is ours.

## Build it, run it

```sh
ge qc build games/shooters_edge/qc/arena/progs.src     # writes progs.dat and progs.dat.lines there
ge qc build games/shooters_edge/qc/campaign/progs.src  # the campaign variant
ge qc disasm games/shooters_edge/qc/arena/progs.dat --function=damage
make lab LAB=se-baseline ARGS="--bots=4 --timelimit=2"   # bots play the arena variant
make lab LAB=se-campaign ARGS="--game=<the classic game> --progs=games/shooters_edge/qc/campaign/progs.src --map=e1m1 --exit=1"
```

Each variant's `progs.src` names the output, then the sources in the order they are compiled.
Inside this repository, `make lab LAB=se-mod ARGS="--progs=games/shooters_edge/qc/arena/progs.dat"` runs a build
of it like any other mod, and a server takes it with `--progs=<file>` or `sv_progs <file>`
(`docs/shooters-edge.md` § Running your own mod). The built files are not committed.

The language is written down in `porting/shooters-edge/qc-language.md`; the compiler is
`plugins/progs/qc_compile.hpp`. A mistake is reported at its file and line.

## The files

A variant compiles its `engine.qc`, then `common/defs.qc`, its `glue.qc`, the rest of `common/`,
then its `world.qc`.

| File | What is in it |
| --- | --- |
| `<variant>/engine.qc` | The engine-side globals and fields of the variant's program form (in the order the engine reads them: never reorder), the optional fields, the builtins it calls |
| `common/defs.qc` | The constants, the mod's own fields and globals, forward declarations |
| `<variant>/glue.qc` | What the forms do differently: printing, effects and hits, frags, a missile's first run, the spawn parms, the spawn point, the inventory a player starts with, a respawn |
| `common/util.qc` | Helpers: removing an entity later, a mover's straight move (`calc_move`), editor angles, firing targets (`use_targets`), effects |
| `common/combat.qc` | Damage (armour first, then health, the push), blasts (`radius_damage`), frags and obituaries |
| `common/weapons.qc` | The eight weapons: costs, rates, ranks, impulses, and how each fires |
| `common/items.qc` | Health, armour, ammunition and weapons on the map, picked up and coming back |
| `common/player.qc` | Joining, the frame around each command, pain, death, water, slime and lava, the kill command, leaving |
| `common/map.qc` | The map's things: teleport destinations, lights, walls, doors, plats, buttons, triggers, teleporters, pushes, hurts |
| `arena/world.qc` | The world, the frag and time limits, the end of a match |
| `campaign/world.qc` | The world, the intermission and the next map, the campaign's map things |

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

- **Keep `engine.qc` as it is.** The engine reads those globals and fields by
  offset; the compiler checks them by name, type and place and writes the layout's crc only when
  they match, and a server refuses a progs without it. Add your own in `common/defs.qc` or later.
- **Order matters**: a function must be declared before it is called. Put a prototype
  (`void() my_think;`) near the top when two functions call each other.
- **`self` and `other`** are the entity a hook or a think is for, and the one touching it. A
  function that changes `self` to call another puts it back before returning.
- **Sounds and models** are named by the classic content paths; the game maps each to a free
  replacement (`games/shooters_edge/content/`).
- **Check your change**: `ge qc build` until it compiles clean, then the `se-baseline` lab before
  and after (`--tag=before`, then `--compare=before`); for the campaign variant `se-campaign
  --progs=`. Inside this repository the variants are also held by
  `tests/shooters_edge_baseline_mod_test.cpp` and `tests/shooters_edge_campaign_mod_test.cpp`.
