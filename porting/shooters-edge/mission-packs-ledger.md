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

Counts: `make ledger` counts this ledger's rows by section and the missing rows by rank (`porting/ledger-count.mjs`; `make ledger ARGS=--missing` for the missing count alone). No total is kept here by hand.

Starting count (2026-11-23): **131 missing, 131 to verify, of 272 rows**; `make ledger` gives the count now (the PR that moves it says what it filled).

The packs' sizes, for scale: Scourge of Armagon 18 maps and 140 files, Dissolution of Eternity 23
maps and 146 files, The Reckoning 25 maps and 832 files, Ground Zero 32 maps and 1983 files.

## Taking the packs in (shared by all four)

| id | what it is (our words) | status | rank | ours / note |
| --- | --- | --- | --- | --- |
| imp.q1_folder | a Quake pack's folder (`hipnotic`, `rogue`) taken in beside `id1` | done | 1 | plugins/classic/content/import.hpp § Mission packs (`find_packs`, by the folder's name); tests/classic_content_import_test.cpp; real: Quake with both packs taken in by `secli import` |
| imp.q2_folder | a Quake II pack's folder (`xatrix`, `rogue`) taken in beside `baseq2` | done | 1 | same; a name both games use (`rogue`) told by the base folder beside it; real: Quake II with both packs |
| imp.known | each pack's `pak0.pak` named by its SHA-256 and size (the four digests are in `/mnt/project-files/paks/README.md`) | done | 1 | plugins/classicenhance/profile.hpp `MissionPack`, `known_pack` (the four digests); tests/classicenhance_profile_test.cpp |
| imp.release_zip | a release zip holding an installed tree (Scourge of Armagon's and Dissolution of Eternity's archives: `Quake/HIPNOTIC/PAK0.PAK` in capitals, the base game's packs beside it) | done | 1 | import.hpp § Release archives (`open_tree` reads a `.zip` whole, its tree walked as an install); tests/classic_content_import_test.cpp |
| imp.q1_cd | the Quake packs' CD image (`QPacks.BIN` with its sheet, in both archives) | done | 2 | engine/vfs/disc.hpp reads BIN/CUE; both packs taken in off one image with no base folder beside them, the second pack's folder name (shared with Ground Zero) settled by the first (tests/classic_content_import_test.cpp `import_takes_both_quake_packs_off_their_cd_image`, with and without its sheet) |
| imp.cd_layout | the Quake II packs' CD layout: the pack at `Data/all/pak0.pak`, the movies under `Data/max/<folder>/video`, the patch under `Data/patch`; known by the pack's digest, not by a game folder | done | 1 | import.hpp § Mission packs, by digest: the pack, the shallowest folder named after it, `patch/<base>/`; real: The Reckoning's CD layout told by its digest |
| imp.mode2 | a BIN/CUE whose data track is `MODE2/2352` (The Reckoning's) | done | 1 | engine/vfs/disc.hpp. Real (2026-11-23): `secli import` of The Reckoning's CUE and BIN (sha256 cd5d3917…2591, the sheet still naming `Q2MP1.BIN` after the download renamed both, read through the image with the sheet's name) took 131 files in 194 s on the debug build, the pack's digest matching, its 8 movies and its 10 disc tracks as lossless music; peak memory 1.2 GB. Test: vfs_disc_test `disc_reads_a_sheet_renamed_with_its_image` |
| imp.raw2448 | a raw image of 2448-byte sectors (2352 and 96 of subchannel) with its `.mds` sheet (Ground Zero's MDF) | done | 1 | engine/vfs/disc.hpp: a `.mdf` alone, 2448-byte sectors told by the second sector's sync; tests/vfs_disc_test.cpp. Its `.mds` sheet: imp.mds |
| imp.mds | an `.mdf`'s `.mds` sheet read for its audio tracks (Ground Zero's disc music) | missing | 2 | the data track is read without it (imp.raw2448) |
| imp.7z | a 7z holding the image (Ground Zero's, a 607 MB archive of an MDF) taken in | done | 1 | import.hpp § Release archives: a 7z's one disc image unpacked into `<data>/.unpacking/` (read whole, 2 GiB limit), removed with its folder after. Real (2026-11-23): `secli import` of Ground Zero's CD 7z (sha256 53be82ea…0a59) took 93 files in 72 s on the debug build, the pack's digest matching, its 7 movies and the players' weapon patch with it; peak memory 2.6 GB on the debug build with its sanitizer (the archive and the image both held) |
| imp.movies | the Quake II packs' movies copied into `<folder>/video/` | done | 1 | copied with the pack (its `video/`, from its own folder or the disc's `Data/max/<pack>/`) |
| imp.vwep | Ground Zero's patch: the player models' weapons for its guns (`players/*/w_*.md2`, `a_grenades.md2`) | done | 2 | a `patch/<base folder>/` folder's content folders copied into the base folder |
| imp.tracks | each pack's CD audio tracks into its own `music/`, numbered as the digital release (`ogg_remap_tracks` is done) | done | 2 | a pack's own disc: its audio tracks kept in `<data>/<id>/<pack>/music/`, played before the game's when the pack plays (client/take_in.hpp) |
| imp.detect | found without being asked (Niclas, 2026-11-23): a pack in a store's library, on a mounted volume, as a disc image or archive in the folders the games page searches, or beside a base game being taken in, is offered and taken in | done | 1 | import.hpp `find_offers` over `store_folders` (stores' libraries, mounted volumes): a pack's folder, or a known pack's size on a disc, offered in the console at the start (client/take_in.hpp `offer_words`) and by `secli import`; beside a base game taken in: taken with it. Image and archive files: imp.detect_files |
| imp.detect_files | a pack's disc image or release archive lying in those folders (a `.cue`, `.mdf`, `.zip`, `.7z`) offered too | done | 2 | import.hpp `find_offers`: a disc image (`.iso`, `.cue`, `.bin`, `.img`, `.mdf`) or `.zip` of 1 MiB or more, 64 at most, listed by seeks (`vfs::list_zip_file`, `vfs::open_disc`) and offered as itself when it holds a first pack of a known pack's size or in a pack's folder; a `.7z` is not looked in (its listing needs the whole archive) and is taken by `secli import <file>`. Test: classic_content_import_test `a_pack_on_a_disc_image_or_in_a_zip_in_the_store_folders_is_offered_as_itself` |
| imp.games_page | each pack its own row on the games page (Quake: Scourge of Armagon, ...) starting the base game with the pack's folder | done | 1 | SELECT GAME: a row a pack under its game (client/installs.hpp `game_rows`, `packs_of`), opening the install with the pack's folder; the console's `games` names them |
| imp.needs_base | a pack taken in without its base game: said in the game's words (it needs the full game) | done | 2 | client/take_in.hpp `take_in_words`: the packs kept, "add Quake II too", the game playing left as it is; tests/shooters_edge_take_in_test.cpp (take_in_names_the_mission_packs_and_waits_for_their_game) |

## Scourge of Armagon (Quake, folder `hipnotic`)

| id | what it is (our words) | status | rank | ours / note |
| --- | --- | --- | --- | --- |
| hip.program | the pack's `progs.dat` read and run: the base game's header checksum (5927), no builtin the base game's lacks, 52 fields of its own (checked from the file) | done |  | loads and runs hip1m1 (`se-client --game=<folder>/hipnotic --map=hip1m1 --progs=pack`, 2026-11-23); re-run on the real pack 2026-11-23 (verification sweep): hip1m1 loads and plays |
| hip.campaign | NEW GAME from `start` through the five episodes to `hipend` (18 maps) | verify | 1 | a playthrough lab on the pack |
| hip.menu.episodes | the pack's episodes and levels in the menu | done |  | menu/episodes.cpp (C395); held by tests/shooters_edge_menu_test.cpp (episodes_for on the pack's maps) |
| hip.hud.weapons | the status bar's laser cannon, Mjolnir and proximity gun, plain, lit and flashing, from the pack's `gfx.wad` (`INV_LASER`, `INV_MJOLNIR`, `INV_PROX_GREN`, `INV_GREN_PROX`, `INV_PROX` and their `INV2_` and `INVA1_`..`INVA5_` sets) | done | 1 | hud/classic_art.cpp loads the pack's set by name (`pack_art`), hud/layout.cpp draws it off the items word (`armagon_bar`); tests/shooters_edge_hud_test.cpp; the se-client lab on hip1m1 (`--fields`) |
| hip.hud.items | the wetsuit's and the empathy shields' icons (`SB_WSUIT`, `SB_ESHLD`) | done | 1 | hud/classic_art.cpp loads the pack's set by name (`pack_art`), hud/layout.cpp draws it off the items word (`armagon_bar`); tests/shooters_edge_hud_test.cpp; the se-client lab on hip1m1 (`--fields`); the keys move to the main bar (`sbar_keys`) |
| hip.cfg | the pack's `quake.rc` and `default.cfg` run at start | done |  | its `default.cfg` (in its pack) runs as what it adds to the base game's: 9 the laser cannon (impulse 225), 0 Mjolnir (impulse 226), the zoom aliases; the window's layout stands (client/install_configs.cpp, tests/shooters_edge_install_configs_test.cpp); its `quake.rc` adds only the demo list (hip.demos) |
| hip.demos | the attract loop plays the pack's four demos | done |  | `menu_attract auto` takes the list from the pack's own `quake.rc` (hipdemo1..4); the se-attract lab with `--game=hipnotic` plays hipdemo1 on hip1m5 (2026-11-23) |
| hip.conback | the pack's console background | done |  | the pack's own `gfx/conback.lmp` (the scratched one) drawn by the se-attract lab with `--game=hipnotic` (2026-11-23) |
| hip.endtext | `end2.bin`, the text screen shown on quitting | not taken |  | the DOS build alone showed one; the Windows and Linux builds, which base Quake here follows, quit with nothing shown, so the packs do the same (decided 2026-11-23; printing it to the terminal on quit is the way back if wanted) |
| hip.dm | `hipdm1` played by bots on the pack's program | verify | 2 |  |
| hip.coop | co-op on the pack's program, a companion beside the player | done | 2 | `labs se-q1-campaign --game=<dir>/id1 --mod=hipnotic --episodes=hip1m1 --party=2`: two bots in co-op on the pack's program, 24 of 58 monsters down in 240 s, no fault (2026-11-23) |
| hip.bot | the campaign bot plays the pack to 100% | missing | 2 | plugins/shooterbots' campaign module learns its monsters, weapons, items and traps |
| hip.remix | the pack's weapons and items as roles for remix | done |  | profiles/remix.hpp: the laser cannon as energy, Mjolnir as blast, the proximity gun as lobber, the wetsuit as protect; the empathy shields and the horn have no role and drop (tests/shooters_edge_remix_test.cpp) |
| hip.ce | classic enhance's caches with the pack layered over the base packs (its textures, models, sounds) | verify | 2 |  |

### What the maps place that the base game's maps never do

Each row below is a classname the pack's maps place that no map of the base game places, with how
many of the pack's maps place it and how many times. All of them are spawned by the pack's own
program on our VM; a row turns `done` when a lab or a playthrough shows it as a player sees it.

**The things lab** (2026-11-23): `labs se-campaign --game=<dir>/id1 --mod=hipnotic --map=<map>
--things=1` on all 18 maps, each on its own at skill 1. Every thing the map's text placed is
watched by its map class name (`QcServer::spawned_as`) while the map runs, while each weapon is
picked up and fired, while each name the map targets is used and while each trigger is touched by
the player (games/shooters_edge/labs/campaign_things.hpp). The pack's program faulted on none of
them. It is a scripted look, not a playthrough: it says the pack's things work on our server, not
that a map can be got through (hip.campaign, hip.bot). The items' rows add `--pickups=1`.

#### Monsters

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| hip.ent.monster_armagon | `monster_armagon` | Armagon, the final boss: a walker with rockets and lasers | 1 / 1 | done | 1 | the things lab: walks, turns, animates, sounds and throws its effects on hipend |
| hip.ent.monster_gremlin | `monster_gremlin` | the gremlin: leaps, steals the player's weapon and fires it, multiplies by feeding on corpses | 9 / 74 | done | 1 | the things lab: all 62 animate, 30 woken walk and sound on 9 maps; the theft and the feeding are the program's own, not checked apart |
| hip.ent.monster_scourge | `monster_scourge` | the centroid: a scorpion-like walker firing nails from its tail | 9 / 46 | done | 1 | the things lab: all 27 animate, 11 woken walk and sound on 9 maps |

#### Weapons and items

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| hip.ent.item_artifact_empathy_shields | `item_artifact_empathy_shields` | empathy shields: half the damage taken is dealt back to the attacker | 12 / 12 | done | 2 | se-campaign `--pickups`: taken (its model emptied) on all 8 maps; its effect is the program's |
| hip.ent.item_artifact_wetsuit | `item_artifact_wetsuit` | the wetsuit: breathe and move freely under water for a while | 6 / 7 | done | 2 | se-campaign `--pickups`: taken on all 6 maps; its effect is the program's |
| hip.ent.item_hornofconjuring | `item_hornofconjuring` | the horn of conjuring: calls up a monster that fights on the player's side | 6 / 6 | done | 2 | se-campaign `--pickups`: taken on all 6 maps; the monster it calls is the program's |
| hip.ent.weapon_laser_gun | `weapon_laser_gun` | the laser cannon: laser bolts that bounce off walls, on cells | 17 / 19 | done | 1 | the things lab: picked up and fired on 6 maps (12 to 50 bolts in 3 s, their sounds, 4 to 29 cells spent); the HUD row hip.hud.weapons draws it |
| hip.ent.weapon_mjolnir | `weapon_mjolnir` | Mjolnir: a hammer that strikes lightning across the floor, on cells | 15 / 17 | done | 1 | the things lab: picked up and fired on 4 maps (5 to 10 strikes, 15 to 30 cells spent); LIFT left: the lightning drawn as Quake's beam (C465) |
| hip.ent.weapon_proximity_gun | `weapon_proximity_gun` | the proximity gun: grenades that stick and wait for someone to come close, on rockets | 17 / 21 | done | 1 | the things lab: picked up and fired on 5 maps (4 to 17 grenades, 4 or 5 rockets spent); LIFT left: one proximity mine with Ground Zero's |

#### Rotating brushes

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| hip.ent.func_movewall | `func_movewall` | an invisible box that moves with a rotating group to block and push | 15 / 292 | done | 1 | the things lab: 287 of 291 move with their groups on 15 maps |
| hip.ent.func_rotate_door | `func_rotate_door` | a door that swings open on a hinge | 13 / 47 | done | 1 | the things lab: all 47 swing (turned and sounded) on 13 maps; the window draws brush models turned by their angles (client/world_models.cpp `place_brushes`, brush/app/bsp.wgsl) |
| hip.ent.func_rotate_entity | `func_rotate_entity` | a brush model that turns continuously (fans, gears) | 6 / 13 | done | 1 | the things lab: all 13 turn on 6 maps; drawn turned as func_rotate_door |
| hip.ent.func_rotate_train | `func_rotate_train` | a train that turns as it follows its path | 5 / 12 | done | 1 | the things lab: all 11 run their paths, 10 turning, on 5 maps; drawn turned as func_rotate_door |
| hip.ent.info_rotate | `info_rotate` | the point a rotating group turns about | 16 / 70 | done | 1 | the things lab: all 69 taken up by their groups at the start (removed), on 16 maps |
| hip.ent.path_follow | `path_follow` | a path corner a follower walks | 1 / 12 | verify | 2 | runs in the pack's program; the things lab: idle on its one map (nothing seen following it) |
| hip.ent.path_follow2 | `path_follow2` | the second kind of path corner a follower walks | 1 / 12 | verify | 2 | runs in the pack's program; the things lab: idle on its one map (nothing seen following it) |
| hip.ent.path_rotate | `path_rotate` | a path corner that also sets a rotating train's turn | 6 / 46 | done | 1 | the things lab: the rotate trains run along them (their 11 move and turn) |
| hip.ent.rotate_object | `rotate_object` | a brush model turned by its group's centre | 16 / 77 | done | 1 | the things lab: all 76 turn with their groups on 16 maps; drawn turned as func_rotate_door |

#### Movers and walls

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| hip.ent.func_bobbingwater | `func_bobbingwater` | a water brush that bobs up and down | 2 / 5 | done | 2 | the things lab: all 5 bob on 2 maps |
| hip.ent.func_breakawaywall | `func_breakawaywall` | a wall removed when triggered | 2 / 6 | done | 1 | the things lab: all 5 removed when used |
| hip.ent.func_clock | `func_clock` | a clock whose hands turn with the level's time | 1 / 3 | done | 3 | the things lab: its 3 hands turn |
| hip.ent.func_rubble | `func_rubble` | throws rubble when triggered | 3 / 9 | done | 2 | the things lab: used, it throws its rubble (14 pieces made) |
| hip.ent.func_rubble1 | `func_rubble1` | throws the first kind of rubble | 4 / 10 | done | 2 | the things lab: used, they throw their rubble (75 pieces made) |
| hip.ent.func_rubble2 | `func_rubble2` | throws the second kind of rubble | 3 / 10 | done | 2 | the things lab: used, they throw their rubble (52 pieces made) |
| hip.ent.func_rubble3 | `func_rubble3` | throws the third kind of rubble | 2 / 24 | done | 2 | the things lab: used, they throw their rubble (43 pieces made) |
| hip.ent.func_togglewall | `func_togglewall` | a wall switched solid and back by a trigger | 3 / 4 | done | 2 | the things lab: 3 of 4 switched and sounded when used |
| hip.ent.func_train2 | `func_train2` | a train with its own speed and wait at each corner | 11 / 68 | done | 1 | the things lab: all 68 run and sound on 11 maps |

#### Effects and sounds

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| hip.ent.ambient_fan_blowing | `ambient_fan_blowing` | a fan's looping sound | 6 / 28 | done | 2 | the things lab: all 28 loops started at their places |
| hip.ent.ambient_flouro_buzz | `ambient_flouro_buzz` | a fluorescent light's buzz | 2 / 3 | done | 2 | the things lab: all 3 loops started |
| hip.ent.ambient_humming | `ambient_humming` | a machine's hum | 6 / 21 | done | 2 | the things lab: all 21 loops started |
| hip.ent.ambient_light_buzz | `ambient_light_buzz` | a light's buzz | 4 / 7 | done | 2 | the things lab: all 7 loops started |
| hip.ent.ambient_riftpower | `ambient_riftpower` | the rift's power hum | 1 / 1 | done | 3 | the things lab: its loop started |
| hip.ent.ambient_running_water | `ambient_running_water` | running water | 4 / 13 | done | 2 | the things lab: all 13 loops started |
| hip.ent.ambient_rushing | `ambient_rushing` | rushing air or water | 1 / 2 | done | 3 | the things lab: both loops started |
| hip.ent.effect_finale | `effect_finale` | the last level's end sequence | 1 / 7 | done | 1 | the things lab: used on hipend, it makes its end sequence's things (48) |
| hip.ent.effect_teleport | `effect_teleport` | a teleport's sparkle at a point | 1 / 2 | done | 2 | the things lab: both sound when used |
| hip.ent.func_earthquake | `func_earthquake` | shakes everyone standing while it is on | 7 / 21 | done | 1 | the things lab: used on 7 maps, it shakes the player (his speed changed in 3 uses); LIFT left: one quake shake with Dissolution's and Quake II's |
| hip.ent.func_exploder | `func_exploder` | an explosion when triggered | 7 / 94 | done | 1 | the things lab: all 94 appear, animate, sound and go when used, on 7 maps |
| hip.ent.func_multi_exploder | `func_multi_exploder` | a run of explosions over an area | 2 / 3 | done | 2 | the things lab: all 3 throw their explosions (9 made) and go |
| hip.ent.func_particlefield | `func_particlefield` | a field of sparkles while on (a force field's look) | 3 / 12 | verify | 2 | runs in the pack's program; the things lab: used, it made 4 things but no effect was seen near it |
| hip.ent.info_startendtext | `info_startendtext` | where the end text starts | 1 / 1 | verify | 2 | runs in the pack's program; the things lab: used, it made 1 thing; with hip.endtext |
| hip.ent.play_sound_triggered | `play_sound_triggered` | plays a sound when triggered | 5 / 16 | done | 2 | the things lab: all 16 sound when used |
| hip.ent.random_thunder_triggered | `random_thunder_triggered` | thunder at random while on | 4 / 6 | done | 3 | the things lab: all 6 sound when used |

#### Triggers and traps

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| hip.ent.func_counter | `func_counter` | counts its triggers and fires by the count | 11 / 25 | verify | 1 | runs in the pack's program; the things lab: used without its count, 7 of 25 go; its count not walked |
| hip.ent.func_oncount | `func_oncount` | fires when a counter reaches its number | 8 / 62 | verify | 1 | runs in the pack's program; the things lab: used alone it does nothing; its counter not walked |
| hip.ent.func_spawn | `func_spawn` | spawns a monster or an item when triggered | 9 / 304 | done | 1 | the things lab: used, they make their monsters and items (9 uses made things), on 9 maps |
| hip.ent.func_spawn_small | `func_spawn_small` | spawns a small thing when triggered | 2 / 3 | verify | 2 | runs in the pack's program; the things lab: 1 of 2 goes when used; nothing seen made |
| hip.ent.info_notnull | `info_notnull` | a named point with no class of its own (a target) | 5 / 34 | done | 2 | the things lab: a named point the program's things aim at; nothing of its own to do |
| hip.ent.trap_gods_wrath | `trap_gods_wrath` | lightning on everyone in its area when triggered | 1 / 2 | done | 2 | the things lab: strikes (its sound) when used |
| hip.ent.trap_gravity_well | `trap_gravity_well` | pulls whoever is near toward it | 2 / 3 | verify | 2 | runs in the pack's program; the things lab: idle when used; whether it pulls is not seen |
| hip.ent.trap_lightning_switched | `trap_lightning_switched` | a lightning arc switched on and off | 2 / 19 | done | 2 | the things lab: 15 of 19 strike when switched, making their bolts |
| hip.ent.trap_lightning_triggered | `trap_lightning_triggered` | a lightning arc fired when triggered | 5 / 17 | done | 2 | the things lab: all 17 strike when used; LIFT left: Quake's beam drawing |
| hip.ent.trap_shooter | `trap_shooter` | a shooter of spikes or lasers on a timer | 1 / 2 | verify | 2 | runs in the pack's program; ours names it; the things lab: idle on its map |
| hip.ent.trap_spike_mine | `trap_spike_mine` | a floating mine that drifts toward a player and bursts into spikes | 4 / 29 | done | 2 | the things lab: all 14 turn, animate and sound on 4 maps |
| hip.ent.trap_tesla_coil | `trap_tesla_coil` | a coil that zaps whoever comes close | 1 / 2 | verify | 2 | runs in the pack's program; the things lab: used, made 1 thing; whether it zaps is not seen |
| hip.ent.trigger_damagethreshold | `trigger_damagethreshold` | fires only after taking enough damage | 3 / 3 | verify | 2 | runs in the pack's program; the things lab: 2 of 3 go when used; not shot |
| hip.ent.trigger_decoy_use | `trigger_decoy_use` | draws monsters to a decoy | 1 / 7 | verify | 2 | runs in the pack's program; LIFT: a decoy monsters chase, with Ground Zero's doppleganger; the things lab: 2 of 7 go when used; no decoy seen |
| hip.ent.trigger_setgravity | `trigger_setgravity` | sets the gravity of whoever is inside | 2 / 7 | done | 1 | the things lab: touched, it changes the player's gravity (4 touches) |
| hip.ent.trigger_usekey | `trigger_usekey` | a trigger that needs a key | 1 / 2 | done | 1 | the things lab: touched without the key, it says so (its sound) |
| hip.ent.trigger_waterfall | `trigger_waterfall` | pushes whoever is inside like falling water | 1 / 1 | verify | 2 | runs in the pack's program; the things lab: touched, the push on the player not seen |

## Dissolution of Eternity (Quake, folder `rogue`)

| id | what it is (our words) | status | rank | ours / note |
| --- | --- | --- | --- | --- |
| rog1.program | the pack's `progs.dat`: the base game's checksum, no new builtin, 63 fields of its own (`items2` among them) | done |  | loads and runs r1m1 (`se-client --game=<folder>/rogue --map=r1m1 --progs=pack`, 2026-11-23); re-run on the real pack 2026-11-23 (verification sweep): r1m1 loads and plays |
| rog1.campaign | NEW GAME from `start` through both episodes to the end (23 maps with `ctf1` and the six ammo box maps) | verify | 1 | a playthrough lab on the pack |
| rog1.ammo_maps | the six `b_` maps (lava nails, multi rockets and plasma boxes, big and small) loaded as brush models | verify | 1 | the base game's `b_` boxes load the same way |
| rog1.items2 | the program's `items2` reaching the player's HUD (the new weapons and powerups) | done | 1 | net/qw_frame.cpp's `items_word` (`items2` from bit 23, in place of the runes), client/view.cpp's and client/timedemo.cpp's status; the se-client lab on r1m1; the word itself held by tests/shooters_edge_qw_server_test.cpp (the_items_stat_carries_items2_for_a_program_with_it_else_the_runes) |
| rog1.menu.episodes | the pack's episodes and levels in the menu | done |  | menu/episodes.cpp (C395); held by tests/shooters_edge_menu_test.cpp (episodes_for on the pack's maps) |
| rog1.menu.modes | MULTIPLAYER ▸ NEW GAME's teamplay row with the pack: Tag, Capture the Flag, One Flag CTF, Three Team CTF | missing | 2 | facts in porting/shooters-edge/q1-qw-console-facts.md |
| rog1.hud.weapons | the lava nailguns, the multi launchers and the plasma gun on the status bar (`R_LAVA`, `R_SUPERLAVA`, `R_GREN`, `R_MULTIROCK`, `R_PLASMA`, the bars `R_INVBAR1`, `R_INVBAR2`) | done | 1 | hud/classic_art.cpp (`pack_art`), hud/layout.cpp (`eternity_bar`); tests/shooters_edge_hud_test.cpp; the se-client lab on r1m1 (`--fields`) |
| rog1.hud.ammo | their ammunition (`R_AMMOLAVA`, `R_AMMOMULTI`, `R_AMMOPLASMA`) | done | 1 | hud/classic_art.cpp (`pack_art`), hud/layout.cpp (`eternity_bar`); tests/shooters_edge_hud_test.cpp; the se-client lab on r1m1 (`--fields`); the main bar's icon off bits 26-28 |
| rog1.hud.items | the power shield and the anti-grav belt (`R_SHIELD1`..`5`, `R_AGRAV1`..`5`) | done | 1 | hud/classic_art.cpp (`pack_art`), hud/layout.cpp (`eternity_bar`); tests/shooters_edge_hud_test.cpp; the se-client lab on r1m1 (`--fields`); only `R_SHIELD1` and `R_AGRAV1` are drawn, as the pack drew them |
| rog1.hud.team | the team colour's border in team games (`R_TEAMBORD`) | done | 2 | hud/layout.cpp (`sbar_team`, `sbar_team_frags`): teamplay 4 to 6 in a game of more than one; the colours by the palette; tests/shooters_edge_hud_test.cpp |
| rog1.runes | deathmatch's runes | verify | 2 |  |
| rog1.grapple | the grappling hook in team games, its chain drawn | verify | 2 | the beam model is drawn (C465) |
| rog1.motd | the pack's message of the day on joining | done | 3 | the pack's own program shows it as a centre print, only with teamplay 3 to 6 (porting/shooters-edge/q1-rogue-team-modes.md § 1): `labs se-client --game=<dir>/rogue --map=ctf1 --progs=pack --server="deathmatch 1;teamplay 4" --say=...` notes the welcome four times in its first six seconds, the team it was put on, the teams' score line and the 50 green armour kit (2026-11-24) |
| rog1.cfg | the pack's configs run at start | done |  | the pack ships no `default.cfg` or `quake.rc` (its pack read 2026-11-23), so the base game's start runs, as it does |
| rog1.demos | the attract loop plays the pack's three demos | done |  | the pack's own demo1..3 win over the base's by the search path; the se-attract lab with `--game=rogue` plays its demo1 on r1m5 (2026-11-23) |
| rog1.conback | the pack's console background | done |  | the pack's own `gfx/conback.lmp` (its logo at the right) drawn by the se-attract lab with `--game=rogue` (2026-11-23) |
| rog1.endtext | `end1.bin`, `end2.bin`, the text screens on quitting | not taken |  | as hip.endtext |
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
| xat.music | the pack's CD tracks found by the digital release's numbers | done |  | plugins/classic/content/soundtrack.cpp; held end to end by tests/shooters_edge_take_in_test.cpp (the window's music finds the remapped file; any case or a path) |
| xat.flashes | the pack's muzzle flashes, the player's and the monsters' | done |  | server/q2_flash.cpp; held by tests/shooters_edge_q2_flash_test.cpp (the_mission_packs_flashes_play_their_own_shots) |
| xat.effects | the ion ripper's, blue hyperblaster's, spinning light's, plasma's and trap's entity effects | done |  | client/world_fx_q2.cpp (Quake II client ledger `ef.*`); the marks drawn held by tests/shooters_edge_q2_world_fx_test.cpp |
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

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| xat.ent.ammo_magslug | `ammo_magslug` | mag slugs for the phalanx | 25 / 88 | missing | 1 | not in our rules |
| xat.ent.ammo_trap | `ammo_trap` | the trap: laid on the floor, pulls monsters in and leaves a power cube to eat | 14 / 24 | missing | 1 | not in our rules |
| xat.ent.item_quadfire | `item_quadfire` | dual fire: the weapons fire twice as fast for a while | 17 / 27 | missing | 1 | not in our rules; LIFT: the powerup timer and its HUD icon as the quad's |
| xat.ent.key_green_key | `key_green_key` | the green key | 2 / 2 | missing | 1 | not in our rules; the keys' code done for the base game (server/q2_items.hpp) |
| xat.ent.weapon_boomer | `weapon_boomer` | the ion ripper: glowing bolts that ricochet off walls, on cells | 24 / 33 | missing | 1 | not in our rules |
| xat.ent.weapon_phalanx | `weapon_phalanx` | the phalanx particle cannon: two magnesium slugs that burst | 24 / 35 | missing | 1 | not in our rules |

#### Monsters

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
| gz.music | the pack's CD tracks found by the digital release's numbers | done |  | plugins/classic/content/soundtrack.cpp; held end to end by tests/shooters_edge_take_in_test.cpp (the window's music finds the remapped file; any case or a path) |
| gz.flashes | the pack's muzzle flashes, the player's and the monsters' | done |  | server/q2_flash.cpp; held by tests/shooters_edge_q2_flash_test.cpp (the_mission_packs_flashes_play_their_own_shots) |
| gz.effects | the tracker's and the other entity effects, the disguise's figure, infra-red goggles' view | done |  | client/world_fx_q2.cpp, client/q2_look.hpp; the marks drawn held by tests/shooters_edge_q2_world_fx_test.cpp |
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
| gz.remix | the pack's weapons and items as roles for remix | done |  | profiles/remix.hpp: the ETF rifle as rapid, the plasma beam (both names) as beam, the prox launcher as lobber, the disruptor as launcher, their ammunition beside them, double damage as damage; the chainfist, the tesla, the A-M bomb, the spheres, the decoy and the goggles drop (tests/shooters_edge_remix_test.cpp) |

### What the maps place that the base game's maps never do

Each row below is a classname the pack's maps place that no map of the base game places (counts
as above). Our Quake II rules spawn none of them: each is built in C++ beside the base game's.

#### Weapons and items

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

| id | classname | what it is (our words) | maps / placed | status | rank | ours / note |
| --- | --- | --- | --- | --- | --- | --- |
| gz.ent.dm_tag_token | `dm_tag_token` | the tag game's token (`gamerules 2`) | 4 / 4 | missing | 2 | not in our rules; LIFT: one tag mode with Dissolution's |

#### Map things

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
