# The campaign mod

**Frozen: no new QC.** Our own rules are written in C++ now (`docs/mods.md`, AGENTS.md's C++
rule). These sources are being carried over to C++ and deleted (row M52h, `docs/goals/M52h.md`);
until then a fix here is a fix the C++ port must carry too, and new rules go in C++ only. The
deathmatch (the arena) is carried over already: its rules are `games/shooters_edge/rules/arena*.cpp`,
a native program the server runs where a compiled progs would run.

What is left in QC:

- **`campaign/`**: the classic maps' single-player game, on the single-player form, played by the
  campaign host as it plays any campaign program (porting notes: `campaign-rules.md`). Today it
  has the campaign's flow (starts, exits and the intermission, secrets, counters, relays, skill,
  episode gates, runes, keys, the inventory carried from map to map) and the first monsters (the
  rifleman and the hound); the rest of the roster comes kind by kind.

- **`common/`**: what the campaign shares with the arena's rules (the arena's C++ carries the
  same rules, file for file: `arena_combat.cpp` is `combat.qc`, and so on).

Nothing in the engine knows it is ours.

## Build it, run it

```sh
ge qc build games/shooters_edge/qc/campaign/progs.src  # writes progs.dat and progs.dat.lines there
ge qc disasm games/shooters_edge/qc/campaign/progs.dat --function=damage
make lab LAB=se-campaign ARGS="--game=<the classic game> --progs=games/shooters_edge/qc/campaign/progs.src --map=e1m1 --exit=1"
```

`progs.src` names the output, then the sources in the order they are compiled. The built files
are not committed.

The language is written down in `porting/shooters-edge/qc-language.md`; the compiler is
`plugins/progs/qc_compile.hpp`. A mistake is reported at its file and line.

## The files

The campaign compiles its `engine.qc`, then `common/defs.qc`, its `glue.qc`, the rest of
`common/`, then its own files (`progs.src` lists them in order), `world.qc` last.

| File | What is in it |
| --- | --- |
| `campaign/engine.qc` | The engine-side globals and fields of the single-player program form (in the order the engine reads them: never reorder), the optional fields, the builtins it calls |
| `common/defs.qc` | The constants, the mod's own fields and globals, forward declarations |
| `campaign/glue.qc` | What the single-player form does: printing, effects and hits, frags, a missile's first run, the spawn parms, the spawn point, the inventory a player starts with, a respawn |
| `common/util.qc` | Helpers: removing an entity later, a mover's straight move (`calc_move`), editor angles, firing targets (`use_targets`), effects |
| `common/combat.qc` | Damage (armour first, then health, the push), blasts (`radius_damage`), frags and obituaries |
| `common/weapons.qc` | The eight weapons: costs, rates, ranks, impulses, and how each fires |
| `common/items.qc` | Health, armour, ammunition and weapons on the map, picked up and coming back |
| `common/player.qc` | Joining, the frame around each command, pain, death, water, slime and lava, the kill command, leaving |
| `common/map.qc` | The map's things: teleport destinations, lights, walls, doors, plats, buttons, triggers, teleporters, pushes, hurts |
| `campaign/ai.qc` | The monsters' minds: spawning, the senses, noticing a player, stand, walk a path, run, attack, anger, pain, death; path corners |
| `campaign/monsters.qc` | The roster: each kind's numbers, attacks, pain and sounds (the rifleman, the hound) |
| `campaign/monsters_close.qc` | The roster, continued: the swordsman, the brute and the ghoul |
| `campaign/monsters_air.qc` | The roster, continued: the flyer and the eel |
| `campaign/monsters_big.qc` | The roster, continued: the leaper, the giant and the caster |
| `campaign/monsters_more.qc` | The roster, continued: the trooper, the spawn and the heavy swordsman |
| `campaign/bosses.qc` | The bosses and their set pieces: the lava boss and the lightning, the final boss and the teleport train, and the campaign's teleporting |
| `campaign/set_pieces.qc` | The other set pieces: trains, secret doors, exploding boxes, fireball spouts, spike and laser traps, bubbles, the ambient sounds |
| `campaign/world.qc` | The world, the intermission and the next map, the campaign's map things |

## The game (the campaign's, and the arena's in C++)

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
- **A loaded game's words** (the arena): with a classic game's packs open, the server's local
  info holds its item names (read by `name_of`) and its server words (`game_words` in
  `arena_glue.cpp`, the `words` builtin #640): its deaths, its clock, the end and the powers wearing
  off. Each returns "" when the game names none, and the mod then says its own words; a new
  message is a key in `profiles/words.hpp` and a `game_words` call.
- **The map's entities**: `info_player_deathmatch`, `info_player_start`, `func_wall`,
  `func_illusionary`, `func_door` (opens for a player near it, or when triggered), `func_plat`,
  `func_button`, `trigger_multiple`, `trigger_once`, `trigger_teleport` with
  `info_teleport_destination`, `trigger_push`, `trigger_hurt`, and every item and weapon.

## The campaign's monsters

- **A kind** is a spawn function in `campaign/monsters*.qc`: its precaches, its sight and death
  sounds (`noise`, `noise1`), its gib health and head, its walk and run steps, its `th_` hooks
  (`th_missile`, `th_melee`, `th_pain`, `th_die`, and `th_check` for an attack decision of its
  own), then `walkmonster_start(model, health, mins, maxs)` (dropped to the floor),
  `flymonster_start` or `swimmonster_start` (left where the map puts it). `no_monsters_here()`
  first takes it out of a deathmatch. A thing it throws or spits is `monster_missile(model, speed,
  damage, lob)`, aimed at its enemy's eye; a blow up close is `melee_blow(factor)`.
- **Its mind** is `campaign/ai.qc`'s: it stands or walks its path (`target` naming a
  `path_corner`) until it notices a player (far never, mid in front and seen, near seen and in
  front unless he fired in the last second, melee seen), then runs at him, attacking when the
  decision says so. Hurt by another monster it turns on it (riflemen even on their own kind);
  a trigger that fires it makes it angry at the player who set the trigger off.
- **A hit** reaches a monster through `damage` (`common/combat.qc`): a thing with `th_hurt` and
  `th_die` answers its own hits, a player's are `player_pain` and `player_die`. A thing with a
  `blast_share` takes only that share of a blast (`radius_damage`): the giant's is 0.5.

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
- **Check your change**: `ge qc build` until it compiles clean, then `se-campaign --progs=`
  before and after (`--tag=before`, then `--compare=before`). Inside this repository the campaign
  is also held by `tests/shooters_edge_campaign_mod_test.cpp` and
  `tests/shooters_edge_campaign_monsters_test.cpp`.
