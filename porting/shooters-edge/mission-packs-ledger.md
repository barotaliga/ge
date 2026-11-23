# The mission packs: the ledger

WORKING AREA. What the four official mission packs need from Shooter's Edge (Niclas,
2026-11-23): Scourge of Armagon and Dissolution of Eternity for Quake, The Reckoning and Ground
Zero for Quake II. One row for each thing a player meets that the base game lacks, marked against
our tree (main at #1382). The roadmap row is `docs/goals/M63.md`. The owner first wanted the
packs to wait for the base games' gaps, the settings lift and the manual, then started them at
once, in parallel with that work (Niclas, 2026-11-23 12:16).

**How it was surveyed.** From the packs' own files and the releases' printed notes, never from any
source code: every pack file listed, every map's entity lump read for the classnames it places
(compared with every map of the base game's packs), the Quake packs' programs read for their
header checksum, the builtins they bind and the fields they add, the Quake packs' `gfx.wad` for
the status bar's pictures, the Quake II packs' manual and release notes for the weapons, enemies,
`dmflags` and settings. Behaviour beyond a one-line meaning (numbers, timings, every word a
player reads) comes from clean-room readers when a pack's work starts; their questions are at the
end. The packs themselves are in the project's shared folder (`paks/quake1/hipnotic`,
`paks/quake1/rogue`, `paks/quake2/xatrix`, `paks/quake2/rogue`), never in the tree.

**What the survey found.**

- **The Quake packs already run their own programs.** Both `progs.dat` files carry the base game's
  header checksum and bind no builtin the base game's program lacks, so our QC VM runs them as it
  runs the base campaign (`docs/shooters-edge.md` § The campaign). Their monsters, weapons, items,
  traps and rotating brushes are the program's: those rows are `verify` until a playthrough lab
  reaches them. What the engine itself must add is small: the packs' status bar pictures, the
  team modes in the menu, `items2` reaching the HUD, and taking the packs in.
- **The Quake II packs need their rules in C++.** Each CD ships its game library as a Windows x86
  file, which our hosting of a game library (`server/q2_native.hpp`) cannot load on any platform
  we ship, and no tree may carry a rebuilt one. Their weapons, items, monsters and map things are
  built beside Quake II's own rules (`server/q2_*`) behind one switch each pack's profile sets,
  as `docs/porting/rules.md` asks. **The reference is the packs' own game source**, which the
  publisher released under the GPL: as with Quake II's base game, only a clean-room reader opens
  it and writes a facts page in our words; our C++ is built from that page (`clean-room` skill).
  The client half is mostly there already: the packs' muzzle
  flashes, entity effects, music numbering and infra-red view are done; 16 temporary entities
  the base game never sends are drawn as stand-ins today and are rows here.

Statuses: `done` (ours does it, the file named), `verify` (should work through a shared piece;
a lab must show it before it is `done`), `missing` (rank 1: the campaign cannot be played or looks
broken without it; 2: noticed in a normal game; 3: rare or cosmetic), `not-taken` (left out on
purpose, the doc named). The missing count only goes down; a row found later is added here first.

| Section | Rows | Done | Verify | Missing (rank 1 / 2 / 3) | Not taken |
| --- | --- | --- | --- | --- | --- |
| Taking the packs in (shared by all four) | 15 | 0 | 3 | 12 (9 / 3 / 0) | 0 |
| Scourge of Armagon (Quake, folder `hipnotic`) | 74 | 4 | 67 | 3 (0 / 1 / 2) | 0 |
| Dissolution of Eternity (Quake, folder `rogue`) | 61 | 7 | 50 | 4 (0 / 2 / 2) | 0 |
| The Reckoning (Quake II, folder `xatrix`) | 40 | 3 | 3 | 34 (20 / 12 / 2) | 0 |
| Ground Zero (Quake II, folder `rogue`) | 82 | 3 | 7 | 72 (36 / 31 / 5) | 0 |
| **All** | **272** | **17** | **130** | **125 (65 / 49 / 11)** | **0** |

Starting count (2026-11-23): **131 missing, 131 to verify, of 272 rows.**

Now: **125 missing, 130 to verify** (the Quake packs' status bars and `items2`).

The packs' sizes, for scale: Scourge of Armagon 18 maps and 140 files, Dissolution of Eternity 23
maps and 146 files, The Reckoning 25 maps and 832 files, Ground Zero 32 maps and 1983 files.

## Taking the packs in (shared by all four)

count: done 0, verify 3, missing 12 (9 / 3 / 0), not-taken 0

| id | what it is (our words) | status | rank | ours / note |
| --- | --- | --- | --- | --- |
| imp.q1_folder | a Quake pack's folder (`hipnotic`, `rogue`) taken in beside `id1` | missing | 1 | plugins/classic/content/import.hpp copies the base and the multiplayer folder only |
| imp.q2_folder | a Quake II pack's folder (`xatrix`, `rogue`) taken in beside `baseq2` | missing | 1 | same |
| imp.known | each pack's `pak0.pak` named by its SHA-256 and size (the four digests are in `/mnt/project-files/paks/README.md`) | missing | 1 | the known packs (plugins/classicenhance/source.hpp) and the content profiles |
| imp.release_zip | a release zip holding an installed tree (Scourge of Armagon's and Dissolution of Eternity's archives: `Quake/HIPNOTIC/PAK0.PAK` in capitals, the base game's packs beside it) | missing | 1 | engine/vfs zips are read; the tree inside is not walked as an install |
| imp.q1_cd | the Quake packs' CD image (`QPacks.BIN` with its sheet, in both archives) | verify | 2 | engine/vfs/disc.hpp reads BIN/CUE |
| imp.cd_layout | the Quake II packs' CD layout: the pack at `Data/all/pak0.pak`, the movies under `Data/max/<folder>/video`, the patch under `Data/patch`; known by the pack's digest, not by a game folder | missing | 1 | `find_game` looks for a base folder with its first pack |
| imp.mode2 | a BIN/CUE whose data track is `MODE2/2352` (The Reckoning's) | verify | 1 | engine/vfs/disc.hpp reads it; held once imp.cd_layout lands |
| imp.raw2448 | a raw image of 2448-byte sectors (2352 and 96 of subchannel) with its `.mds` sheet (Ground Zero's MDF) | missing | 1 | engine/vfs/disc.hpp reads 2048 and 2352 |
| imp.7z | a 7z holding the image (Ground Zero's, a 643 MB MDF) read without the whole file in memory | verify | 1 | engine/vfs's 7z reader; its size limit to check |
| imp.movies | the Quake II packs' movies copied into `<folder>/video/` | missing | 1 |  |
| imp.vwep | Ground Zero's patch: the player models' weapons for its guns (`players/*/w_*.md2`, `a_grenades.md2`) | missing | 2 |  |
| imp.tracks | each pack's CD audio tracks into its own `music/`, numbered as the digital release (`ogg_remap_tracks` is done) | missing | 2 | the soundtrack import reads a disc's audio into the base game's `music/` |
| imp.detect | found without being asked (Niclas, 2026-11-23): a pack in a store's library, on a mounted volume, as a disc image or archive in the folders the games page searches, or beside a base game being taken in, is offered and taken in | missing | 1 | client/take_in.hpp, plugins/classic/content/import.hpp's `store_folders` |
| imp.games_page | each pack its own row on the games page (Quake: Scourge of Armagon, ...) starting the base game with the pack's folder | missing | 1 | menu/select_game |
| imp.needs_base | a pack taken in without its base game: said in the game's words (it needs the full game) | missing | 2 |  |

## Scourge of Armagon (Quake, folder `hipnotic`)

count: done 4, verify 67, missing 3 (0 / 1 / 2), not-taken 0

| id | what it is (our words) | status | rank | ours / note |
| --- | --- | --- | --- | --- |
| hip.program | the pack's `progs.dat` read and run: the base game's header checksum (5927), no builtin the base game's lacks, 52 fields of its own (checked from the file) | done |  | loads and runs hip1m1 (`se-client --game=<folder>/hipnotic --map=hip1m1 --progs=pack`, 2026-11-23) |
| hip.campaign | NEW GAME from `start` through the five episodes to `hipend` (18 maps) | verify | 1 | a playthrough lab on the pack |
| hip.menu.episodes | the pack's episodes and levels in the menu | done |  | menu/episodes.cpp (C395) |
| hip.hud.weapons | the status bar's laser cannon, Mjolnir and proximity gun, plain, lit and flashing, from the pack's `gfx.wad` (`INV_LASER`, `INV_MJOLNIR`, `INV_PROX_GREN`, `INV_GREN_PROX`, `INV_PROX` and their `INV2_` and `INVA1_`..`INVA5_` sets) | done | 1 | hud/classic_art.cpp loads the pack's set by name (`pack_art`), hud/layout.cpp draws it off the items word (`armagon_bar`); tests/shooters_edge_hud_test.cpp; the se-client lab on hip1m1 (`--fields`) |
| hip.hud.items | the wetsuit's and the empathy shields' icons (`SB_WSUIT`, `SB_ESHLD`) | done | 1 | hud/classic_art.cpp loads the pack's set by name (`pack_art`), hud/layout.cpp draws it off the items word (`armagon_bar`); tests/shooters_edge_hud_test.cpp; the se-client lab on hip1m1 (`--fields`); the keys move to the main bar (`sbar_keys`) |
| hip.cfg | the pack's `quake.rc` and `default.cfg` run at start | verify | 2 |  |
| hip.demos | the attract loop plays the pack's four demos | verify | 3 |  |
| hip.conback | the pack's console background | verify | 3 |  |
| hip.endtext | `end2.bin`, the text screen shown on quitting | missing | 3 | the owner decides whether it is taken |
| hip.dm | `hipdm1` played by bots on the pack's program | verify | 2 |  |
| hip.coop | co-op on the pack's program, a companion beside the player | verify | 2 |  |
| hip.bot | the campaign bot plays the pack to 100% | missing | 2 | plugins/shooterbots' campaign module learns its monsters, weapons, items and traps |
| hip.remix | the pack's weapons and items as roles for remix | missing | 3 | plugins/classic/item_roles.hpp |
| hip.ce | classic enhance's caches with the pack layered over the base packs (its textures, models, sounds) | verify | 2 |  |

### What the maps place that the base game's maps never do

Each row below is a classname the pack's maps place that no map of the base game places, with how
many of the pack's maps place it and how many times. All of them are spawned by the pack's own
program on our VM; a row turns `done` when a lab or a playthrough shows it as a player sees it.

#### Monsters

count: done 0, verify 3, missing 0, not-taken 0

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| hip.ent.monster_armagon | `monster_armagon` | Armagon, the final boss: a walker with rockets and lasers | 1 / 1 | verify | 1 | runs in the pack's program |
| hip.ent.monster_gremlin | `monster_gremlin` | the gremlin: leaps, steals the player's weapon and fires it, multiplies by feeding on corpses | 9 / 74 | verify | 1 | runs in the pack's program |
| hip.ent.monster_scourge | `monster_scourge` | the centroid: a scorpion-like walker firing nails from its tail | 9 / 46 | verify | 1 | runs in the pack's program |

#### Weapons and items

count: done 0, verify 6, missing 0, not-taken 0

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| hip.ent.item_artifact_empathy_shields | `item_artifact_empathy_shields` | empathy shields: half the damage taken is dealt back to the attacker | 12 / 12 | verify | 2 | runs in the pack's program; HUD row hip.hud.items |
| hip.ent.item_artifact_wetsuit | `item_artifact_wetsuit` | the wetsuit: breathe and move freely under water for a while | 6 / 7 | verify | 2 | runs in the pack's program; HUD row hip.hud.items; remix role beside Quake II's rebreather |
| hip.ent.item_hornofconjuring | `item_hornofconjuring` | the horn of conjuring: calls up a monster that fights on the player's side | 6 / 6 | verify | 2 | runs in the pack's program |
| hip.ent.weapon_laser_gun | `weapon_laser_gun` | the laser cannon: laser bolts that bounce off walls, on cells | 17 / 19 | verify | 1 | runs in the pack's program; the HUD row hip.hud.weapons draws it |
| hip.ent.weapon_mjolnir | `weapon_mjolnir` | Mjolnir: a hammer that strikes lightning across the floor, on cells | 15 / 17 | verify | 1 | runs in the pack's program; LIFT: the lightning drawn as Quake's beam (C465 and the lightning gun's own beam) |
| hip.ent.weapon_proximity_gun | `weapon_proximity_gun` | the proximity gun: grenades that stick and wait for someone to come close, on rockets | 17 / 21 | verify | 1 | runs in the pack's program; LIFT: one proximity mine with Ground Zero's (gz.ent.weapon_proxlauncher) |

#### Rotating brushes

count: done 0, verify 9, missing 0, not-taken 0

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| hip.ent.func_movewall | `func_movewall` | an invisible box that moves with a rotating group to block and push | 15 / 292 | verify | 1 | runs in the pack's program |
| hip.ent.func_rotate_door | `func_rotate_door` | a door that swings open on a hinge | 13 / 47 | verify | 1 | runs in the pack's program; LIFT: our pushers rotate for Quake II's `func_rotating` (server/q2_map.hpp); a rotating brush's collision is the shared open gap (docs/porting/rules.md § Gaps) |
| hip.ent.func_rotate_entity | `func_rotate_entity` | a brush model that turns continuously (fans, gears) | 6 / 13 | verify | 1 | runs in the pack's program; same LIFT as func_rotate_door |
| hip.ent.func_rotate_train | `func_rotate_train` | a train that turns as it follows its path | 5 / 12 | verify | 1 | runs in the pack's program; same LIFT |
| hip.ent.info_rotate | `info_rotate` | the point a rotating group turns about | 16 / 70 | verify | 1 | runs in the pack's program |
| hip.ent.path_follow | `path_follow` | a path corner a follower walks | 1 / 12 | verify | 2 | runs in the pack's program |
| hip.ent.path_follow2 | `path_follow2` | the second kind of path corner a follower walks | 1 / 12 | verify | 2 | runs in the pack's program |
| hip.ent.path_rotate | `path_rotate` | a path corner that also sets a rotating train's turn | 6 / 46 | verify | 1 | runs in the pack's program |
| hip.ent.rotate_object | `rotate_object` | a brush model turned by its group's centre | 16 / 77 | verify | 1 | runs in the pack's program; needs the client to draw brush models at their angles |

#### Movers and walls

count: done 0, verify 9, missing 0, not-taken 0

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| hip.ent.func_bobbingwater | `func_bobbingwater` | a water brush that bobs up and down | 2 / 5 | verify | 2 | runs in the pack's program |
| hip.ent.func_breakawaywall | `func_breakawaywall` | a wall removed when triggered | 2 / 6 | verify | 1 | runs in the pack's program; LIFT: debris with Quake II's `func_explosive` |
| hip.ent.func_clock | `func_clock` | a clock whose hands turn with the level's time | 1 / 3 | verify | 3 | runs in the pack's program |
| hip.ent.func_rubble | `func_rubble` | throws rubble when triggered | 3 / 9 | verify | 2 | runs in the pack's program |
| hip.ent.func_rubble1 | `func_rubble1` | throws the first kind of rubble | 4 / 10 | verify | 2 | runs in the pack's program |
| hip.ent.func_rubble2 | `func_rubble2` | throws the second kind of rubble | 3 / 10 | verify | 2 | runs in the pack's program |
| hip.ent.func_rubble3 | `func_rubble3` | throws the third kind of rubble | 2 / 24 | verify | 2 | runs in the pack's program |
| hip.ent.func_togglewall | `func_togglewall` | a wall switched solid and back by a trigger | 3 / 4 | verify | 2 | runs in the pack's program |
| hip.ent.func_train2 | `func_train2` | a train with its own speed and wait at each corner | 11 / 68 | verify | 1 | runs in the pack's program |

#### Effects and sounds

count: done 0, verify 16, missing 0, not-taken 0

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| hip.ent.ambient_fan_blowing | `ambient_fan_blowing` | a fan's looping sound | 6 / 28 | verify | 2 | runs in the pack's program |
| hip.ent.ambient_flouro_buzz | `ambient_flouro_buzz` | a fluorescent light's buzz | 2 / 3 | verify | 2 | runs in the pack's program; ours names it (rules/common_quake.cpp) |
| hip.ent.ambient_humming | `ambient_humming` | a machine's hum | 6 / 21 | verify | 2 | runs in the pack's program |
| hip.ent.ambient_light_buzz | `ambient_light_buzz` | a light's buzz | 4 / 7 | verify | 2 | runs in the pack's program; ours names it (rules/common_quake.cpp) |
| hip.ent.ambient_riftpower | `ambient_riftpower` | the rift's power hum | 1 / 1 | verify | 3 | runs in the pack's program |
| hip.ent.ambient_running_water | `ambient_running_water` | running water | 4 / 13 | verify | 2 | runs in the pack's program |
| hip.ent.ambient_rushing | `ambient_rushing` | rushing air or water | 1 / 2 | verify | 3 | runs in the pack's program |
| hip.ent.effect_finale | `effect_finale` | the last level's end sequence | 1 / 7 | verify | 1 | runs in the pack's program |
| hip.ent.effect_teleport | `effect_teleport` | a teleport's sparkle at a point | 1 / 2 | verify | 2 | runs in the pack's program |
| hip.ent.func_earthquake | `func_earthquake` | shakes everyone standing while it is on | 7 / 21 | verify | 1 | runs in the pack's program; LIFT: one quake shake with Dissolution's and Quake II's `target_earthquake` |
| hip.ent.func_exploder | `func_exploder` | an explosion when triggered | 7 / 94 | verify | 1 | runs in the pack's program |
| hip.ent.func_multi_exploder | `func_multi_exploder` | a run of explosions over an area | 2 / 3 | verify | 2 | runs in the pack's program |
| hip.ent.func_particlefield | `func_particlefield` | a field of sparkles while on (a force field's look) | 3 / 12 | verify | 2 | runs in the pack's program |
| hip.ent.info_startendtext | `info_startendtext` | where the end text starts | 1 / 1 | verify | 2 | runs in the pack's program |
| hip.ent.play_sound_triggered | `play_sound_triggered` | plays a sound when triggered | 5 / 16 | verify | 2 | runs in the pack's program |
| hip.ent.random_thunder_triggered | `random_thunder_triggered` | thunder at random while on | 4 / 6 | verify | 3 | runs in the pack's program |

#### Triggers and traps

count: done 0, verify 17, missing 0, not-taken 0

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| hip.ent.func_counter | `func_counter` | counts its triggers and fires by the count | 11 / 25 | verify | 1 | runs in the pack's program |
| hip.ent.func_oncount | `func_oncount` | fires when a counter reaches its number | 8 / 62 | verify | 1 | runs in the pack's program |
| hip.ent.func_spawn | `func_spawn` | spawns a monster or an item when triggered | 9 / 304 | verify | 1 | runs in the pack's program |
| hip.ent.func_spawn_small | `func_spawn_small` | spawns a small thing when triggered | 2 / 3 | verify | 2 | runs in the pack's program |
| hip.ent.info_notnull | `info_notnull` | a named point with no class of its own (a target) | 5 / 34 | verify | 2 | runs in the pack's program |
| hip.ent.trap_gods_wrath | `trap_gods_wrath` | lightning on everyone in its area when triggered | 1 / 2 | verify | 2 | runs in the pack's program |
| hip.ent.trap_gravity_well | `trap_gravity_well` | pulls whoever is near toward it | 2 / 3 | verify | 2 | runs in the pack's program |
| hip.ent.trap_lightning_switched | `trap_lightning_switched` | a lightning arc switched on and off | 2 / 19 | verify | 2 | runs in the pack's program; LIFT: Quake's beam drawing |
| hip.ent.trap_lightning_triggered | `trap_lightning_triggered` | a lightning arc fired when triggered | 5 / 17 | verify | 2 | runs in the pack's program |
| hip.ent.trap_shooter | `trap_shooter` | a shooter of spikes or lasers on a timer | 1 / 2 | verify | 2 | runs in the pack's program; ours names it |
| hip.ent.trap_spike_mine | `trap_spike_mine` | a floating mine that drifts toward a player and bursts into spikes | 4 / 29 | verify | 2 | runs in the pack's program |
| hip.ent.trap_tesla_coil | `trap_tesla_coil` | a coil that zaps whoever comes close | 1 / 2 | verify | 2 | runs in the pack's program |
| hip.ent.trigger_damagethreshold | `trigger_damagethreshold` | fires only after taking enough damage | 3 / 3 | verify | 2 | runs in the pack's program |
| hip.ent.trigger_decoy_use | `trigger_decoy_use` | draws monsters to a decoy | 1 / 7 | verify | 2 | runs in the pack's program; LIFT: a decoy monsters chase, with Ground Zero's doppleganger |
| hip.ent.trigger_setgravity | `trigger_setgravity` | sets the gravity of whoever is inside | 2 / 7 | verify | 1 | runs in the pack's program; the entity's gravity field is read by our physics (plugins/shooter/entity.hpp); LIFT with Quake II's trigger_gravity |
| hip.ent.trigger_usekey | `trigger_usekey` | a trigger that needs a key | 1 / 2 | verify | 1 | runs in the pack's program |
| hip.ent.trigger_waterfall | `trigger_waterfall` | pushes whoever is inside like falling water | 1 / 1 | verify | 2 | runs in the pack's program |

## Dissolution of Eternity (Quake, folder `rogue`)

count: done 7, verify 50, missing 4 (0 / 2 / 2), not-taken 0

| id | what it is (our words) | status | rank | ours / note |
| --- | --- | --- | --- | --- |
| rog1.program | the pack's `progs.dat`: the base game's checksum, no new builtin, 63 fields of its own (`items2` among them) | done |  | loads and runs r1m1 (`se-client --game=<folder>/rogue --map=r1m1 --progs=pack`, 2026-11-23) |
| rog1.campaign | NEW GAME from `start` through both episodes to the end (23 maps with `ctf1` and the six ammo box maps) | verify | 1 | a playthrough lab on the pack |
| rog1.ammo_maps | the six `b_` maps (lava nails, multi rockets and plasma boxes, big and small) loaded as brush models | verify | 1 | the base game's `b_` boxes load the same way |
| rog1.items2 | the program's `items2` reaching the player's HUD (the new weapons and powerups) | done | 1 | net/qw_frame.cpp's `items_word` (`items2` from bit 23, in place of the runes), client/view.cpp's and client/timedemo.cpp's status; the se-client lab on r1m1 |
| rog1.menu.episodes | the pack's episodes and levels in the menu | done |  | menu/episodes.cpp (C395) |
| rog1.menu.modes | MULTIPLAYER ▸ NEW GAME's teamplay row with the pack: Tag, Capture the Flag, One Flag CTF, Three Team CTF | missing | 2 | facts in porting/shooters-edge/q1-qw-console-facts.md |
| rog1.hud.weapons | the lava nailguns, the multi launchers and the plasma gun on the status bar (`R_LAVA`, `R_SUPERLAVA`, `R_GREN`, `R_MULTIROCK`, `R_PLASMA`, the bars `R_INVBAR1`, `R_INVBAR2`) | done | 1 | hud/classic_art.cpp (`pack_art`), hud/layout.cpp (`eternity_bar`); tests/shooters_edge_hud_test.cpp; the se-client lab on r1m1 (`--fields`) |
| rog1.hud.ammo | their ammunition (`R_AMMOLAVA`, `R_AMMOMULTI`, `R_AMMOPLASMA`) | done | 1 | hud/classic_art.cpp (`pack_art`), hud/layout.cpp (`eternity_bar`); tests/shooters_edge_hud_test.cpp; the se-client lab on r1m1 (`--fields`); the main bar's icon off bits 26-28 |
| rog1.hud.items | the power shield and the anti-grav belt (`R_SHIELD1`..`5`, `R_AGRAV1`..`5`) | done | 1 | hud/classic_art.cpp (`pack_art`), hud/layout.cpp (`eternity_bar`); tests/shooters_edge_hud_test.cpp; the se-client lab on r1m1 (`--fields`); only `R_SHIELD1` and `R_AGRAV1` are drawn, as the pack drew them |
| rog1.hud.team | the team colour's border in team games (`R_TEAMBORD`) | done | 2 | hud/layout.cpp (`sbar_team`, `sbar_team_frags`): teamplay 4 to 6 in a game of more than one; the colours by the palette; tests/shooters_edge_hud_test.cpp |
| rog1.runes | deathmatch's runes | verify | 2 |  |
| rog1.grapple | the grappling hook in team games, its chain drawn | verify | 2 | the beam model is drawn (C465) |
| rog1.motd | the pack's message of the day on joining | verify | 3 |  |
| rog1.cfg | the pack's configs run at start | verify | 2 |  |
| rog1.demos | the attract loop plays the pack's three demos | verify | 3 |  |
| rog1.conback | the pack's console background | verify | 3 |  |
| rog1.endtext | `end1.bin`, `end2.bin`, the text screens on quitting | missing | 3 | the owner decides whether it is taken |
| rog1.dm | `ctf1` and the team modes played by bots | verify | 2 |  |
| rog1.coop | co-op on the pack's program | verify | 2 |  |
| rog1.bot | the campaign bot plays the pack to 100% | missing | 2 | plugins/shooterbots |
| rog1.remix | the pack's weapons and items as roles for remix | missing | 3 | plugins/classic/item_roles.hpp |
| rog1.ce | classic enhance's caches with the pack layered over the base packs | verify | 2 |  |

### What the maps place that the base game's maps never do

Each row below is a classname the pack's maps place that no map of the base game places, with how
many of the pack's maps place it and how many times. All of them are spawned by the pack's own
program on our VM; a row turns `done` when a lab or a playthrough shows it as a player sees it.

#### Monsters

count: done 0, verify 10, missing 0, not-taken 0

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| rog1.ent.dragon_corner | `dragon_corner` | a corner of the dragon's flight | 1 / 36 | verify | 1 | runs in the pack's program |
| rog1.ent.info_overlord_destination | `info_overlord_destination` | a spot the Overlord teleports to | 4 / 25 | verify | 1 | runs in the pack's program |
| rog1.ent.monster_dragon | `monster_dragon` | the dragon: flies a path of corners, breathes fire | 1 / 1 | verify | 1 | runs in the pack's program |
| rog1.ent.monster_eel | `monster_eel` | the electric eel: swims and shocks | 6 / 58 | verify | 1 | runs in the pack's program |
| rog1.ent.monster_lava_man | `monster_lava_man` | Hephaestus, the lava man: throws lava balls | 1 / 4 | verify | 1 | runs in the pack's program |
| rog1.ent.monster_morph | `monster_morph` | the guardian that takes three looks | 3 / 3 | verify | 1 | runs in the pack's program |
| rog1.ent.monster_mummy | `monster_mummy` | the mummy: a tougher zombie | 1 / 5 | verify | 1 | runs in the pack's program |
| rog1.ent.monster_super_wrath | `monster_super_wrath` | the Overlord: a big wrath that teleports and throws seeking balls | 2 / 2 | verify | 1 | runs in the pack's program |
| rog1.ent.monster_sword | `monster_sword` | the phantom swordsman: a knight statue come alive | 3 / 20 | verify | 1 | runs in the pack's program |
| rog1.ent.monster_wrath | `monster_wrath` | the wrath: floats and throws seeking balls | 8 / 56 | verify | 1 | runs in the pack's program |

#### Weapons and items

count: done 0, verify 9, missing 0, not-taken 0

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| rog1.ent.item_lava_spikes | `item_lava_spikes` | lava nails: the nailguns fire lava spikes | 17 / 154 | verify | 1 | runs in the pack's program; HUD row rog1.hud.weapons |
| rog1.ent.item_multi_rockets | `item_multi_rockets` | multi rockets: the launchers fire clusters | 17 / 103 | verify | 1 | runs in the pack's program; HUD row rog1.hud.weapons |
| rog1.ent.item_plasma | `item_plasma` | plasma cells: the lightning gun becomes the plasma gun | 16 / 71 | verify | 1 | runs in the pack's program; HUD row rog1.hud.weapons |
| rog1.ent.item_powerup_belt | `item_powerup_belt` | the anti-grav belt: low gravity for a while | 16 / 21 | verify | 1 | runs in the pack's program; HUD row rog1.hud.items; LIFT: the entity gravity factor |
| rog1.ent.item_powerup_shield | `item_powerup_shield` | the power shield: takes damage in front | 17 / 24 | verify | 1 | runs in the pack's program; HUD row rog1.hud.items |
| rog1.ent.item_random_powerup | `item_random_powerup` | a powerup chosen at random each time it comes back (deathmatch) | 8 / 12 | verify | 2 | runs in the pack's program |
| rog1.ent.item_sphere | `item_sphere` | the vengeance sphere: follows the player and strikes his killer | 15 / 27 | verify | 2 | runs in the pack's program; LIFT: Ground Zero's vengeance sphere |
| rog1.ent.item_time_core | `item_time_core` | the time machine's core, carried to the finale | 1 / 1 | verify | 1 | runs in the pack's program |
| rog1.ent.item_time_machine | `item_time_machine` | the time machine of the last level | 1 / 1 | verify | 1 | runs in the pack's program |

#### Game modes

count: done 0, verify 7, missing 0, not-taken 0

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| rog1.ent.dmatch_tag_token | `dmatch_tag_token` | the tag game's token: who holds it scores more | 16 / 16 | verify | 2 | runs in the pack's program; LIFT: one tag mode with Ground Zero's (gz.ent.dm_tag_token) |
| rog1.ent.func_ctf_wall | `func_ctf_wall` | a wall that stands only in some team modes | 14 / 185 | verify | 2 | runs in the pack's program |
| rog1.ent.info_player_team1 | `info_player_team1` | team one's start spot | 15 / 105 | verify | 2 | runs in the pack's program |
| rog1.ent.info_player_team2 | `info_player_team2` | team two's start spot | 15 / 105 | verify | 2 | runs in the pack's program |
| rog1.ent.item_flag | `item_flag` | the flag of one-flag capture the flag | 15 / 15 | verify | 2 | runs in the pack's program |
| rog1.ent.item_flag_team1 | `item_flag_team1` | team one's flag | 15 / 15 | verify | 2 | runs in the pack's program; LIFT: one capture the flag with Quake II's CTF |
| rog1.ent.item_flag_team2 | `item_flag_team2` | team two's flag | 15 / 15 | verify | 2 | runs in the pack's program |

#### Movers and traps

count: done 0, verify 13, missing 0, not-taken 0

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| rog1.ent.buzzsaw | `buzzsaw` | a saw blade running a path | 2 / 17 | verify | 1 | runs in the pack's program |
| rog1.ent.earthquake | `earthquake` | the earthquake a trigger starts | 2 / 2 | verify | 1 | runs in the pack's program; LIFT: one quake shake (hip.ent.func_earthquake) |
| rog1.ent.func_elvtr_button | `func_elvtr_button` | an elevator's call button | 1 / 4 | verify | 1 | runs in the pack's program |
| rog1.ent.func_new_plat | `func_new_plat` | a plat with more ways to move | 15 / 60 | verify | 1 | runs in the pack's program |
| rog1.ent.info_notnull | `info_notnull` | a named point with no class of its own | 1 / 1 | verify | 2 | runs in the pack's program |
| rog1.ent.light_candle | `light_candle` | a candle model with its light | 8 / 113 | verify | 3 | runs in the pack's program |
| rog1.ent.light_lantern | `light_lantern` | a lantern model with its light | 1 / 13 | verify | 3 | runs in the pack's program |
| rog1.ent.ltrail_end | `ltrail_end` | a lightning trail's last link | 11 / 44 | verify | 2 | runs in the pack's program |
| rog1.ent.ltrail_relay | `ltrail_relay` | a lightning trail's middle link | 2 / 3 | verify | 2 | runs in the pack's program |
| rog1.ent.ltrail_start | `ltrail_start` | a lightning trail's first link | 11 / 72 | verify | 2 | runs in the pack's program; LIFT: Quake's beam drawing |
| rog1.ent.pendulum | `pendulum` | a swinging blade | 4 / 12 | verify | 1 | runs in the pack's program |
| rog1.ent.trigger_earthquake | `trigger_earthquake` | starts the earthquake | 4 / 7 | verify | 1 | runs in the pack's program |
| rog1.ent.trigger_earthquake_kill | `trigger_earthquake_kill` | stops the earthquake | 1 / 1 | verify | 2 | runs in the pack's program |

## The Reckoning (Quake II, folder `xatrix`)

count: done 3, verify 3, missing 34 (20 / 12 / 2), not-taken 0

| id | what it is (our words) | status | rank | ours / note |
| --- | --- | --- | --- | --- |
| xat.switch | the pack's rules behind one switch its profile sets, Quake II's own rules, goldens and digests unchanged without it | missing | 1 | server/q2_*; the CD's game library is built for one old system, so hosting it (server/q2_native.hpp) cannot run here |
| xat.brain_beta | the brain's beta form: its tentacles pull, its eyes fire lasers (no classname of its own) | missing | 1 | which brains are the beta: reader question |
| xat.items_list | the item list grown by the pack's items, in its order (the inventory, the icons by name) | missing | 1 | reader question: the order |
| xat.obits | the new weapons' and monsters' obituaries, word for word | missing | 1 | reader question |
| xat.dmflag | `dmflags` 65536: dual fire dropped on death | missing | 2 |  |
| xat.campaign | the four units in order, the help computer's lines, the end | missing | 1 | server/q2_campaign.hpp chains the units |
| xat.movies | the pack's movies (`xin`, `xu1`..`xu4`, `xout`, the logos) played where the campaign asks | verify | 2 | client/cinematic.hpp plays a chain |
| xat.hud | the pack's icons on the HUD and in the inventory (`a_mslugs`, `a_trap`, `k_green`, `p_quadfire`, `w_phallanx`, `w_ripper`) | verify | 2 | pictures named by the rules' configstrings |
| xat.menu | the pack's own pictures (console background, quit, victory) | verify | 3 |  |
| xat.music | the pack's CD tracks found by the digital release's numbers | done |  | plugins/classic/content/soundtrack.cpp |
| xat.flashes | the pack's muzzle flashes, the player's and the monsters' | done |  | server/q2_flash.cpp |
| xat.effects | the ion ripper's, blue hyperblaster's, spinning light's, plasma's and trap's entity effects | done |  | client/world_fx_q2.cpp (Quake II client ledger `ef.*`) |
| xat.te.plasma_explosion | the phalanx's blast (temporary entity 28) | missing | 1 | drawn as a plain explosion today (client ledger `te.plasma_explosion`, not taken for the base game) |
| xat.te.greenblood | the gekk's green blood (temporary entity 26) | missing | 2 | drawn as red blood today (`te.greenblood`) |
| xat.dm | the seven deathmatch maps played by bots | missing | 2 |  |
| xat.coop | co-op through the units | missing | 2 |  |
| xat.save | saves of the pack's campaign | missing | 2 | server/q2_save.hpp |
| xat.bot | the campaign bot plays the pack to 100% | missing | 2 | plugins/shooterbots |
| xat.remix | the pack's weapons and items as roles for remix | missing | 3 |  |

### What the maps place that the base game's maps never do

Each row below is a classname the pack's maps place that no map of the base game places (counts
as above). Our Quake II rules spawn none of them: each is built in C++ beside the base game's.

#### Weapons and items

count: done 0, verify 0, missing 6, not-taken 0

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| xat.ent.ammo_magslug | `ammo_magslug` | mag slugs for the phalanx | 25 / 88 | missing | 1 | not in our rules |
| xat.ent.ammo_trap | `ammo_trap` | the trap: laid on the floor, pulls monsters in and leaves a power cube to eat | 14 / 24 | missing | 1 | not in our rules |
| xat.ent.item_quadfire | `item_quadfire` | dual fire: the weapons fire twice as fast for a while | 17 / 27 | missing | 1 | not in our rules; LIFT: the powerup timer and its HUD icon as the quad's |
| xat.ent.key_green_key | `key_green_key` | the green key | 2 / 2 | missing | 1 | not in our rules; the keys' code done for the base game (server/q2_items.hpp) |
| xat.ent.weapon_boomer | `weapon_boomer` | the ion ripper: glowing bolts that ricochet off walls, on cells | 24 / 33 | missing | 1 | not in our rules |
| xat.ent.weapon_phalanx | `weapon_phalanx` | the phalanx particle cannon: two magnesium slugs that burst | 24 / 35 | missing | 1 | not in our rules |

#### Monsters

count: done 0, verify 0, missing 8, not-taken 0

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| xat.ent.monster_boss5 | `monster_boss5` | the super tank beta: the super tank with energy armour | 2 / 4 | missing | 1 | not in our rules; LIFT: the base super tank |
| xat.ent.monster_chick_heat | `monster_chick_heat` | the iron maiden beta: heat-seeking rockets | 1 / 14 | missing | 1 | not in our rules; LIFT: the base iron maiden with a seeking rocket |
| xat.ent.monster_fixbot | `monster_fixbot` | the repair bot: welds broken things, and can wake the dead | 2 / 11 | missing | 1 | not in our rules |
| xat.ent.monster_gekk | `monster_gekk` | the gekk: an amphibian that leaps, claws, spits and swims | 4 / 119 | missing | 1 | not in our rules |
| xat.ent.monster_gladb | `monster_gladb` | the gladiator beta: fires phalanx rounds, wears energy armour | 3 / 23 | missing | 1 | not in our rules; LIFT: the base gladiator |
| xat.ent.monster_soldier_hypergun | `monster_soldier_hypergun` | the hyperblaster guard: a soldier with blue hyperblaster bolts | 13 / 107 | missing | 1 | not in our rules; same LIFT |
| xat.ent.monster_soldier_lasergun | `monster_soldier_lasergun` | the laser guard: a soldier with a tracking laser | 12 / 111 | missing | 1 | not in our rules; same LIFT |
| xat.ent.monster_soldier_ripper | `monster_soldier_ripper` | the ripper guard: a soldier with an ion ripper | 13 / 147 | missing | 1 | not in our rules; LIFT: the base soldier's code with another gun |

#### Map things

count: done 0, verify 0, missing 7, not-taken 0

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| xat.ent.func_object_repair | `func_object_repair` | a broken thing that sparks until the repair bot fixes it | 1 / 6 | missing | 2 | not in our rules |
| xat.ent.misc_amb4 | `misc_amb4` | an ambient sound | 1 / 1 | missing | 3 | not in our rules |
| xat.ent.misc_nuke | `misc_nuke` | the end level's nuke flash | 1 / 1 | missing | 2 | not in our rules |
| xat.ent.misc_transport | `misc_transport` | a transport ship moving along its path | 1 / 1 | missing | 2 | not in our rules |
| xat.ent.misc_viper_missile | `misc_viper_missile` | a missile a fly-by ship fires | 1 / 16 | missing | 2 | not in our rules |
| xat.ent.rotating_light | `rotating_light` | a rotating warning light (spinning lights effect done in the client) | 3 / 11 | missing | 2 | not in our rules |
| xat.ent.target_mal_laser | `target_mal_laser` | a laser that pulses on and off | 1 / 32 | missing | 2 | not in our rules; LIFT: the base `target_laser` |

## Ground Zero (Quake II, folder `rogue`)

count: done 3, verify 7, missing 72 (36 / 31 / 5), not-taken 0

| id | what it is (our words) | status | rank | ours / note |
| --- | --- | --- | --- | --- |
| gz.switch | the pack's rules behind one switch its profile sets, Quake II's own unchanged | missing | 1 | as xat.switch |
| gz.ai | every monster's changed behaviour in the pack (dodging and ducking shots, jumping and dropping, firing blind at where the player was, the medic's and the hover's changes) | missing | 1 | reader question: the whole list, per monster |
| gz.items_list | the item list grown by the pack's items, in its order | missing | 1 | reader question |
| gz.obits | the new weapons' and monsters' obituaries, word for word | missing | 1 | reader question |
| gz.tesla | the tesla mine thrown from the inventory, its zaps | missing | 1 | ammo row `ammo_tesla` places it |
| gz.nuke | the A-M bomb thrown, its countdown and blast | missing | 1 | ammo row `ammo_nuke` places it |
| gz.tag | the tag game (`gamerules 2`): the token, the bonus, three frags a kill while it, the lines | missing | 2 | LIFT with Dissolution's tag |
| gz.dmflag.131072 | `dmflags` 131072: no A-M bombs, teslas or prox mines | missing | 2 |  |
| gz.dmflag.262144 | `dmflags` 262144: quad and double damage do not stack | missing | 2 |  |
| gz.dmflag.524288 | `dmflags` 524288: no A-M bombs | missing | 2 |  |
| gz.dmflag.1048576 | `dmflags` 1048576: no spheres | missing | 2 |  |
| gz.cvar.huntercam | `huntercam` (1): the hunter sphere's chase camera | missing | 2 | a setting of the pack's rules |
| gz.cvar.strong_mines | `strong_mines` (0): mines laid under quad or double keep it their whole time | missing | 3 |  |
| gz.cvar.randomrespawn | `randomrespawn` (0): a deathmatch item comes back as another of its kind | missing | 3 |  |
| gz.cvar.gamerules | `gamerules` (0; 2 is tag) | missing | 2 |  |
| gz.campaign | the five units in order (the release's `runit2`..`runit5` configs start one), the help computer's lines, the end | missing | 1 | server/q2_campaign.hpp |
| gz.movies | the pack's movies (`rintro`, `reu1_`..`reu4_`, `rend`, the logo) | verify | 2 | client/cinematic.hpp |
| gz.hud | the pack's icons on the HUD and in the inventory (`w_chainfist`, `w_etf_rifle`, `w_proxlaunch`, `w_disintegrator`, `w_heatbeam`, `w_tesla`, `p_double`, `p_ir`, the spheres, `p_doppleganger`, `p_nuke`, `a_flechettes`, `a_prox`, `a_tesla`, `a_disruptor`, `i_tagtoken`) | verify | 2 | pictures named by the rules' configstrings |
| gz.menu | the pack's own menu pictures (the main menu, banners, the cursor), console background, `conchars_blue` | verify | 3 | pictures by name over the base pack's |
| gz.vwep | the new guns in other players' hands (the patch's `w_*.md2`) | verify | 2 | plugins/morph/carry.hpp; needs imp.vwep |
| gz.music | the pack's CD tracks found by the digital release's numbers | done |  | plugins/classic/content/soundtrack.cpp |
| gz.flashes | the pack's muzzle flashes, the player's and the monsters' | done |  | server/q2_flash.cpp |
| gz.effects | the tracker's and the other entity effects, the disguise's figure, infra-red goggles' view | done |  | client/world_fx_q2.cpp, client/q2_look.hpp |
| gz.te.blaster2 | a green blaster's hit (temporary entity 30) | missing | 2 | client ledger `te.blaster2`, not taken for the base game |
| gz.te.lightning | a lightning beam between two things (33; the tesla) | missing | 1 | `te.lightning`; LIFT: Quake's lightning beam drawing |
| gz.te.plain_explosion | an explosion with no specks (35) | missing | 2 | `te.plain_explosion` |
| gz.te.heatbeam | the player's heat beam (38) | missing | 1 | `te.heatbeam`; LIFT: our own lightning beam (the smooth weapon thread's `cl_fakeshaft`) |
| gz.te.monster_heatbeam | a monster's heat beam (39) | missing | 1 | `te.monster_heatbeam`; same LIFT |
| gz.te.moreblood | a bigger burst of blood (42; the chainfist) | missing | 2 | `te.moreblood` |
| gz.te.heatbeam_sparks | the heat beam's sparks (43) | missing | 2 | `te.heatbeam_sparks` |
| gz.te.heatbeam_steam | the heat beam's steam (44) | missing | 2 | `te.heatbeam_steam` |
| gz.te.electric_sparks | blue electric sparks (46) | missing | 2 | `te.electric_sparks` |
| gz.te.tracker_explosion | the disruptor's dark burst (47) | missing | 1 | `te.tracker_explosion` |
| gz.te.nukeblast | the A-M bomb's growing blast (51) | missing | 1 | `te.nukeblast` |
| gz.te.explosion1_big | a bigger explosion (53; the bosses) | missing | 2 | `te.explosion1_big` |
| gz.te.explosion1_np | an explosion with no specks (54) | missing | 2 | `te.explosion1_np` |
| gz.te.flechette | a flechette's hit (55) | missing | 1 | `te.flechette` |
| gz.dm | the fourteen deathmatch maps played by bots | missing | 2 |  |
| gz.coop | co-op through the units (`info_player_coop_lava`) | missing | 2 |  |
| gz.save | saves of the pack's campaign | missing | 2 | server/q2_save.hpp |
| gz.bot | the campaign bot plays the pack to 100% | missing | 2 | plugins/shooterbots |
| gz.remix | the pack's weapons and items as roles for remix | missing | 3 |  |

### What the maps place that the base game's maps never do

Each row below is a classname the pack's maps place that no map of the base game places (counts
as above). Our Quake II rules spawn none of them: each is built in C++ beside the base game's.

#### Weapons and items

count: done 0, verify 3, missing 18, not-taken 0

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| gz.ent.ammo_disruptor | `ammo_disruptor` | rounds for the disruptor | 12 / 41 | missing | 1 | not in our rules |
| gz.ent.ammo_flechettes | `ammo_flechettes` | flechettes for the ETF rifle | 21 / 76 | missing | 1 | not in our rules |
| gz.ent.ammo_nails | `ammo_nails` | a classname 24 maps place: probably dropped at spawn | 24 / 113 | verify | 3 | not in our rules; reader question |
| gz.ent.ammo_nuke | `ammo_nuke` | the A-M bomb: thrown, a huge blast after a delay | 15 / 15 | missing | 1 | not in our rules |
| gz.ent.ammo_prox | `ammo_prox` | prox grenades | 32 / 179 | missing | 1 | not in our rules |
| gz.ent.ammo_tesla | `ammo_tesla` | the tesla mine: thrown, zaps whatever comes close | 32 / 114 | missing | 1 | not in our rules |
| gz.ent.item_doppleganger | `item_doppleganger` | a decoy of the player that monsters attack | 21 / 23 | missing | 2 | not in our rules; LIFT: Scourge of Armagon's decoy |
| gz.ent.item_double | `item_double` | double damage for a while | 24 / 28 | missing | 1 | not in our rules; LIFT: the quad's multiplier and timer |
| gz.ent.item_ir_goggles | `item_ir_goggles` | infra-red goggles: warm bodies glow (the client's view bit is done) | 11 / 11 | missing | 2 | not in our rules |
| gz.ent.item_sphere_defender | `item_sphere_defender` | the defender sphere: floats by the player and shoots at enemies | 28 / 30 | missing | 2 | not in our rules |
| gz.ent.item_sphere_hunter | `item_sphere_hunter` | the hunter sphere: hunts down the player's attacker (its chase camera `huntercam`) | 26 / 28 | missing | 2 | not in our rules |
| gz.ent.item_sphere_vengeance | `item_sphere_vengeance` | the vengeance sphere: strikes whoever killed the player | 22 / 25 | missing | 2 | not in our rules; LIFT: Dissolution's sphere |
| gz.ent.key_nuke | `key_nuke` | the nuke key | 2 / 2 | missing | 1 | not in our rules |
| gz.ent.key_nuke_container | `key_nuke_container` | the container the nuke key is taken from | 1 / 1 | missing | 1 | not in our rules |
| gz.ent.weapon_chainfist | `weapon_chainfist` | the chainfist: a chainsaw fist | 25 / 33 | missing | 1 | not in our rules |
| gz.ent.weapon_disintegrator | `weapon_disintegrator` | the disruptor: a slow bolt that tracks and disintegrates | 13 / 14 | missing | 1 | not in our rules |
| gz.ent.weapon_etf_rifle | `weapon_etf_rifle` | the ETF rifle: fires flechettes | 18 / 25 | missing | 1 | not in our rules |
| gz.ent.weapon_heatbeam | `weapon_heatbeam` | the plasma beam: a continuous beam on cells | 22 / 26 | missing | 1 | not in our rules; LIFT: the beam drawn and aimed as our own lightning beam (cl_fakeshaft, the smooth weapon thread) |
| gz.ent.weapon_nailgun | `weapon_nailgun` | a classname 21 maps place that is not one of the pack's weapons: probably dropped by the library at spawn | 21 / 27 | verify | 3 | not in our rules; reader question |
| gz.ent.weapon_plasmabeam | `weapon_plasmabeam` | a second name the maps use for the plasma beam | 12 / 14 | verify | 1 | not in our rules; which name the library spawns: reader question |
| gz.ent.weapon_proxlauncher | `weapon_proxlauncher` | the prox launcher: grenades that stick and wait | 32 / 44 | missing | 1 | not in our rules; LIFT: one proximity mine with Scourge of Armagon's proximity gun |

#### Monsters

count: done 0, verify 0, missing 8, not-taken 0

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| gz.ent.monster_carrier | `monster_carrier` | the carrier: a flying boss that launches flyers | 1 / 1 | missing | 1 | not in our rules |
| gz.ent.monster_daedalus | `monster_daedalus` | the daedalus: a faster hover with a shield | 10 / 96 | missing | 1 | not in our rules; LIFT: the base hover |
| gz.ent.monster_medic_commander | `monster_medic_commander` | the medic commander: calls in reinforcements | 6 / 30 | missing | 1 | not in our rules; LIFT: the base medic |
| gz.ent.monster_stalker | `monster_stalker` | the stalker: a spider that walks on ceilings | 8 / 96 | missing | 1 | not in our rules |
| gz.ent.monster_turret | `monster_turret` | a wall turret: blaster, machine gun or rockets | 10 / 216 | missing | 1 | not in our rules |
| gz.ent.monster_widow | `monster_widow` | the black widow, the last boss's first form | 1 / 1 | missing | 1 | not in our rules |
| gz.ent.monster_widow2 | `monster_widow2` | the black widow's second form | 1 / 1 | missing | 1 | not in our rules |
| gz.ent.turret_invisible_brain | `turret_invisible_brain` | the hidden brain that drives a turret | 1 / 1 | missing | 2 | not in our rules |

#### Game modes

count: done 0, verify 0, missing 1, not-taken 0

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| gz.ent.dm_tag_token | `dm_tag_token` | the tag game's token (`gamerules 2`) | 4 / 4 | missing | 2 | not in our rules; LIFT: one tag mode with Dissolution's |

#### Map things

count: done 0, verify 0, missing 10, not-taken 0

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| gz.ent.func_plat2 | `func_plat2` | a plat with more ways to move | 28 / 130 | missing | 1 | not in our rules |
| gz.ent.hint_path | `hint_path` | a path hint monsters follow (830 placed) | 12 / 830 | missing | 1 | not in our rules; LIFT: our bots' nav (plugins/shooterbots) may read the hints too |
| gz.ent.info_player_coop_lava | `info_player_coop_lava` | a co-op start used once the lava has risen | 1 / 26 | missing | 2 | not in our rules |
| gz.ent.misc_nuke_core | `misc_nuke_core` | the reactor core of the finale | 1 / 1 | missing | 1 | not in our rules |
| gz.ent.target_anger | `target_anger` | makes monsters angry at a target | 9 / 18 | missing | 2 | not in our rules |
| gz.ent.target_blacklight | `target_blacklight` | a black light's look | 1 / 1 | missing | 3 | not in our rules |
| gz.ent.target_killplayers | `target_killplayers` | kills every player (a failed objective) | 1 / 1 | missing | 2 | not in our rules |
| gz.ent.target_orb | `target_orb` | an orb's look | 1 / 1 | missing | 3 | not in our rules |
| gz.ent.target_steam | `target_steam` | a steam jet | 9 / 69 | missing | 2 | not in our rules |
| gz.ent.trigger_disguise | `trigger_disguise` | gives the disguise: monsters take the player for one of them (the client's figure is done) | 2 / 2 | missing | 2 | not in our rules |

## Lift before port: what is shared

The port-from-ledger rule (step 2): before a row is built, look for the same feature in a game
already ported, lift it to the lowest layer every game means the same by, and reuse it; never
build it twice, never change how an old game feels. The packs meet each other and the base games
here:

| Feature | Rows | Where it is today | The shared piece |
| --- | --- | --- | --- |
| A continuous beam held on a target | `gz.ent.weapon_heatbeam`, `gz.te.heatbeam`, `gz.te.monster_heatbeam` | Quake's lightning gun, our own beam that follows the aim (`cl_fakeshaft`, the smooth weapon thread) | one beam drawn and aimed by the client for every game; Ground Zero's spiral is its look |
| A lightning arc between two points | `gz.te.lightning`, Mjolnir, `trap_lightning_*`, `ltrail_*` | Quake's beam temporary entities (C465) | the same beam drawing |
| A proximity mine | Scourge's proximity gun, Ground Zero's prox launcher | none | one sticky mine in the shooter kit (`plugins/shooter`), each game's numbers as data |
| A tag game | Dissolution's `dmatch_tag_token`, Ground Zero's `gamerules 2` | none | one mode in the server core's modes, each game's words and scores |
| Capture the flag | Dissolution's flags and team starts | Quake II's CTF content is known to the profiles | one capture the flag rule set, each game's flags and words |
| A vengeance sphere | Dissolution's `item_sphere`, Ground Zero's spheres | none | one follower that strikes back, each game's numbers |
| A decoy monsters chase | Scourge's `trigger_decoy_use`, Ground Zero's doppleganger | none | one decoy target in the monsters' choice of enemy |
| A view shake over an area | `func_earthquake`, `earthquake`, `trigger_earthquake` | Quake II's `target_earthquake` (server/q2_lines.cpp) | one shake, its strength and length as data |
| A gravity factor per thing | `trigger_setgravity`, the anti-grav belt | the entity's gravity field (plugins/shooter/entity.hpp), Quake II's `trigger_gravity` | already shared: verify |
| Rotating brushes that push and block | Scourge's `func_rotate_*`, `rotate_object` | Quake II's `func_rotating` (server/q2_map.hpp); a rotating brush's own hull is the open gap in `docs/porting/rules.md` | one rotating pusher with an exact hull, for every game |
| Breakable walls and debris | `func_breakawaywall`, `func_rubble*`, `func_exploder` | Quake II's `func_explosive` | the debris thrown as data |
| A powerup's multiplier and timer | Ground Zero's double damage, The Reckoning's dual fire | the quad in both games | one timed powerup with its HUD icon and its end warning |
| A soldier with another gun | The Reckoning's three guards, the beta monsters | Quake II's soldier, iron maiden, gladiator and super tank | the base monster's code with the gun as data |
| Paths monsters follow | Ground Zero's `hint_path` | our bots' nav (`plugins/shooterbots`) | the hints read by the bots too |
| The status bar's extra icons | Scourge's and Dissolution's pictures | the classic status bar (hud/classic_art.cpp) | the pictures a pack's `gfx.wad` adds, by name, for any Quake pack or mod |

## Questions for the clean-room readers

Asked when a pack's work starts; the answers go into this page's rows or a facts page beside it.
For the Quake II packs the reader reads the pack's released game source (GPL, never opened by
whoever builds); for the Quake packs, the engine's released source and the packs' programs.

- **Scourge of Armagon**: which item bits light the laser cannon, Mjolnir and the proximity gun
  on the status bar, and in which slots the bar draws them; what the engine does differently
  with the pack's switch on (the status bar, the menu, anything else).
- **Dissolution of Eternity**: how `items2` reaches the client (which stat, which bits); the status
  bar's layout with the pack (the weapons' places, the shield's and belt's frames, the team border);
  the teamplay values 3 to 6 in the menu and what the server does with each.
- **The Reckoning**: every new weapon's, item's and monster's numbers (damage, speed, ammunition,
  timings), the item list's order with the pack, every line a player reads (pickups, obituaries,
  the help computer), what the brain beta, the iron maiden beta, the gladiator beta and the super
  tank beta change, which base monsters behave differently, `dmflags` 65536.
- **Ground Zero**: the same for its weapons, items, spheres and monsters; the whole list of what
  every base monster does differently in the pack (dodging, ducking, jumping, blind fire, the
  medic's reinforcements); which of `weapon_nailgun`, `ammo_nails`, `weapon_plasmabeam` and
  `weapon_heatbeam` the library spawns and as what; the tag game's rules and lines; `huntercam`,
  `strong_mines`, `randomrespawn`; the four `dmflags`.

## The order of the work (M63)

Started at once, beside the gap threads (Niclas, 2026-11-23 12:16), one big PR each; The
Reckoning and Ground Zero each in a thread of their own:

1. **M63a, taking the packs in** (the `imp.*` rows): found and offered without being asked, from
   every form the releases come in (a zip of an install, a BIN/CUE, a 7z of an MDF), each a row on
   the games page. First because nothing else can be tried by a player without it.
2. **M63b, the Quake packs as they shipped** (`hip.*`, `rog1.*`): the status bar's pictures, the
   team modes in the menu, `items2`, then each campaign played through and every `verify` row
   settled by labs.
3. **M63c, The Reckoning's rules in C++** (`xat.*`) behind its switch, the beta monsters on the
   base monsters' code, its two temporary entities.
4. **M63d, Ground Zero's rules in C++** (`gz.*`) behind its switch: the weapons on the shared beam
   and mine, the spheres, the monsters' new behaviour, tag, the `dmflags` and settings, its
   fourteen temporary entities.
5. **M63e, the bots and co-op on all four**: the campaign bot to 100% on each pack, co-op, the
   deathmatch maps, remix roles.

## What the shared folder holds

Taken from the four releases Niclas linked (2026-11-23), each downloaded into its own empty
folder; nothing in them was run. Only game data was kept: the packs, the Quake II packs' movies
and their printed notes, and Ground Zero's patch models for the new guns in other players' hands.
`paks/README.md` in the shared folder gives each file's size, digest and source.
