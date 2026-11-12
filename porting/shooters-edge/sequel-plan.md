# The sequel's port: the plan (working document)

**A WORKING DOCUMENT**, deleted with `porting/` when porting ends. It is the plan for bringing the
classic game's sequel (Quake II, id Software, 1997) into Shooter's Edge the way the first game
(QuakeWorld) was brought in, with the first game's port as the template and this one as the
template for the third (Quake III Arena). The roadmap row is `docs/goals/M18v.md` (in the engine's
own words, no game names); the facts are the clean-room notes (`sequel.md` and its pages); this
page says in what order things land, what is shared and what is the sequel's own.

Asked for by Niclas on 2026-11-11 (the go the classic-enhance scope rule waited for): load the
sequel's packs and enter a map first, then mods, our own single player and multiplayer (clean
room), reusing everything the first game's port wrote, refactored into shared layers with thin
glue for each game, so a pack of one game dropped into another's folder works (the sequel's rocket
launcher in the first game, in remix mode).

## The one design rule: readers by KIND, glue by GAME

Every file is read by **what it is**, never by **which game holds it**. A reader of a file family
lives once, in the engine or a plugin:

| Family | Reader (shared) | First game | Sequel |
| --- | --- | --- | --- |
| Packs, zips, folders, the search path | `engine/vfs` | `pak`, `pk3` | `pak` (packs win over loose files: M18v5) |
| Maps | `plugins/brush` (one `Map` view) | BSP 29, BSP2 | BSP 38 (read since M18t; brushes, areas) |
| Wall textures | `plugins/brush` (`MapPicture`) | inside the map | `textures/*.wal` (`brush/wal.hpp`, M18v1) |
| Pictures | `engine/assets` (pcx, tga, png), the wad reader | `gfx.wad`, `.lmp` | `pics/*.pcx` |
| The palette | `classicenhance::palette_of` | `gfx/palette.lmp` | the tail of `pics/colormap.pcx` (M18v1) |
| Vertex-animated models | `plugins/morph` (one `Model`, every frame a morph target) | MDL, SPR | MD2, SP2 (M18v3) |
| Sounds | `engine/audio` (`read_wav`) | `sound/*.wav` | `sound/*.wav`, players' `*` sounds (M18v4) |
| Configs | `engine/console` | cfg | cfg (same language) |

A **game's adapter** (`games/shooters_edge/client/game_adapter.hpp`) is a table of calls; the calls
every game shares are `client/adapter_parts.hpp` (lifted out of the first game's adapter in M18v1),
and a game's own file (`classic_adapter.cpp`, `sequel_adapter.cpp`) holds only what is truly its
own (its HUD and menu pictures, its menu pages, its held weapons, its demos). A game's DATA is its
two profile rows (`plugins/classicenhance/profile.hpp`: where its files are and their kinds;
`games/shooters_edge/profiles/game_profile.hpp`: its programs, items, arsenal, words, episodes).

Consequences, each held by a test as it lands:

- a map of the 38 family copied into the first game's folder draws, its wall textures read from
  whichever packs are open (held in M18v1 by `shooters_edge_game_adapter_test`);
- `open_model` picks the reader by the file's kind (`.mdl`, `.md2`, `.spr`, `.sp2`, `.bsp`, `.iqm`),
  so the sequel's held rocket launcher opens in the first game's folder once MD2 reads (M18v3);
- classic-enhance's stages read the content row, so the upscaler, the height and normal maps, the
  rebake, the sound enhancer and the models' glTF are written once and run on either game (M18v6);
- ASSET ISOLATION stays the default: a loaded game draws only its own files; another game's are
  used only in remix mode (M18u), which asks two adapters at once.

## The goal is remix: every part interchangeable

Niclas, 2026-11-11 15:08: "Remember the goal. Remix." Each part of a game is a choice of its own,
from any classic game the player owns: the sequel played with the first game's physics, the
third's maps, the first game's weapons, and so on. So every step above builds its part as a SLOT
with a shared shape, never as the sequel's code wired to the sequel's other parts:

| Part (slot) | The shared shape every game fills | What makes it swappable |
| --- | --- | --- |
| Maps | `brush::Map` (any family) and its pictures | already one view; the wall textures read in any folder (M18v1) |
| Collision | one trace a move and a rule ask (a box through the world: clip hulls or brushes) | M18v2: the 29 family's hulls and the 38 family's brushes behind one trace call, so any move runs on any map |
| Movement | `sv_playermove` values (`arena`, `original`, `sequel`) over that trace | the sequel's move as a value, the first game's moves on the 38 family's maps (M18v2) |
| Weapons and armour | the arsenal rows (`profiles/arsenal.hpp`) | the sequel's set as rows (M18v7), `sv_weapons`, `sv_armour` (M18t6c) |
| Items | the item rows (`profiles/items.hpp`) mapped onto slots | a map's item names mapped to another set's slots (M18t6b) |
| Models, skins, sounds | `morph::Model`, the sound by name | readers by kind (M18v3, M18v4) |
| HUD and menus | the HUD kit and `MenuTable` | each game's pictures through its adapter (M18v4) |
| Rules | the rules runner seam | a runner per program form (M18v7, M18v10) |
| Bots | the game-neutral bot (M18v8) | reads the slots, never a game |

A part is swappable only when nothing past its slot names a game: each step's PR shows its part
working with another game's parts in a lab (the sequel's map under the first game's move, the
first game's map with the sequel's textures copied in), even before the remix switch (M18u)
offers it to the player.

## What the first game's port gives for free

The renderer (`plugins/brush/app`: classic and modern looks, lights and shadows, water, lava,
skies, fog, weather, decals, the cull), the effects, the HUD kit, the console and cvars, the
search path, classic-enhance, the server core, the lobby, the dedicated server, the demos'
playback machinery, the video export, voice, the bots, the menus' kit, the save system, the
runner seam for rules (`server/rules_runner.hpp`, already holding the sequel's runner,
`server/sequel_server.hpp`), the brush trace (`plugins/brush/trace.hpp`) and the sequel's move
(`plugins/shooter/sequel_move.hpp`).

## The steps (the row's M18v1..M18v12)

1. **M18v1, the files open and a map is entered** (this thread's first PR): the content row and
   the game row, the shared adapter calls and the sequel's adapter, wall textures, the PCX palette,
   lab `se-sequel` drawing every demo map from its start in both looks.
2. **M18v2, the window plays it**: LOAD GAME and `-basedir` recognise the sequel's folder (every
   content row tried; the kept game remembers which), the listen server runs the sequel's runner
   (the host picks the runner by the program form: the native library passed over, our runner in
   its place), a map of the 38 family loads behind the window (brushes, not clip hulls), and the
   player walks the demo maps with the sequel's move. A Mac run command in the PR.
   LANDED (stacked on M18v1): the runner is picked by the map's family for now (a map of brushes
   plays on the sequel's runner whatever the program), since only that runner traces brushes;
   M18v2c (one trace) lets the program form pick it. The spin found on the way: the classic
   listen adopts the server's view, so the spawn's delta angles must be spent once adopted.
   ORDER (Niclas 2026-11-11 18:46, "Should we not start there? Clean room copy of the game?"):
   next is step 7, our own clean-room game (deathmatch rules, then the campaign and its monsters),
   ahead of 2b-6. Packs for every thread: /mnt/project-files/paks/quake2/{demo,full}/ (README there).
2b. **M18v2b, coloured light in the modern look** (Niclas, 2026-11-11 15:13): the sequel's maps
   are lit in colour (RGB lightmaps, `_color` on lights, faces with the light surface flag
   emitting by their texture's colour and value). The modern look must carry that colour (the
   real-time lights, the rebake, bloom) and the brightness rule must be settled (a clean-room
   question: how the game scales its textures and lightmaps). Before and after in `se-sequel`.
3. **M18v3, models and their animation**: MD2 and SP2 into `plugins/morph`; the items, monsters
   and objects standing in the map; the player models and their skins (`players/<model>/`, with
   the fallbacks); the held weapons (`models/weapons/v_*/tris.md2`) behind `held_weapons`. Niclas,
   2026-11-11 15:28: animation must work (it has been a problem in the first game): every model
   animated at the server's 10 Hz frames with the client's blend from the last frame
   (`client/model_lerp.hpp` reused), and a lab that films items, players (run, fire, pain, death),
   monsters and held weapons frame by frame and fails a frame that jumps or freezes.
4. **M18v4, pictures and sounds**: the status bar (the layout language: needs a clean-room note),
   the console font, the menus as the sequel's own `MenuTable` pages, the sky box (`env/`, six
   faces), the players' own sounds.
5. **M18v5, files and mods**: the sequel's search order (packs over loose files) as a content-row
   field; its mod folders; `default.cfg`.
6. **M18v6, classic-enhance on the sequel**: every stage on the content row (the WAL textures
   upscaled with their maps, the RGB lightmaps rebaked, the sounds, MD2 to glTF); the same `ce_`
   switches and menu.
7. **M18v7, its rules**: items, weapons, damage, movers, area portals, deathmatch (M18t4 goes
   on), played by bots with a golden digest; then the WHOLE single-player game (Niclas, 15:28: "a
   lot of work... and all mobs"): every unit and map of the full game, every monster (senses,
   attacks, pain, death, their animations), the level changes, the help computer, saves, played
   start to finish by a scripted player like the first game's campaign (M18s). Needs clean-room
   notes for the full game's monsters and the unit system beyond `sequel-monsters.md`.
8. **M18v8, the bots lifted** (Niclas, 2026-11-11 14:48 and 14:49): the bot as a whole (its mind,
   goals, fighting, footwork, item routes, team play, talk, map plan and navigation) moved behind a
   game-neutral seam, so most of it runs on the sequel and on any remix: map knowledge from either
   family (a trace callback and the walkable graph built from it, items by kind from the item
   rows, weapons from the arsenal rows, the move model as a knob set). Shooter's Edge's own bots
   become glue. **Much of the bot work is not on `main` yet**: this step starts only after it
   lands (the build-on-main and finish-first rules), and is planned with that thread.
9. **M18v9, recordings and the network**: the `.dm2` client recording and the multi-view server
   recording (MVD2, as its modern servers write: Niclas, 2026-11-11 15:07, wants MVDs supported)
   and protocol 34, clean-room notes first (none is specified yet). Each is a new FORMAT READER
   under the demo player that exists (`client/demo.hpp`, told by its first bytes, into the same
   demo state): the highlights, rewinding, speeds, multi-view and video export are reused, never
   written again. Then joining its servers and its clients joining ours (as the first game's
   interop row did).
10. **M18v10, mods and servers** (Niclas, 2026-11-11 15:09: "just like QuakeWorld works now in
    Shooter's Edge"): our client joins existing servers running existing mods; real clients join
    ours; our servers (dedicated and listen, the engine's server core) host the sequel's existing
    mods. Its mods are native game libraries: Niclas chose (decision card, 2026-11-11 15:11) to
    load the mod's library where the platform matches (Linux and Windows x86), the engine
    answering its calls (`sequel.md` § The game library); a platform that cannot load one only
    joins. The game row's native programs then gain a runner instead of being passed over. Our
    own rules stay an `engine/mod` C++ package any mod can replace.
11. **M18v11, remix**: classic remix mode (M18u) with the sequel's weapons, armour, items, player
    models and maps in the first game and the other way round; the arsenal's second set (M18t6c).
12. **M18v12, the template**: what this port learned written as the checklist for a third classic
    game in `docs/shooters-edge.md` § Profiles.

## Data for the labs

The demo pack is fetched at run time and never committed: the 3.14 demo's `baseq2/pak0.pak`
(sha256 `cae25718...b48d09`, `sequel.md` § The demo pack). A copy is in a public install repository
Niclas pointed at (2026-11-11: `drags/docker-quake2`, folder `baseq2`, with the point releases'
`pak1.pak` and `pak2.pak`); clone it outside this tree and pass its folder to `--game`. Pictures of
it go on private pages only. Lab pictures of the sequel's work use its own maps and models (the
project rule for classic work).

## Clean-room questions still open

For a reader subagent (`clean-room` skill), each when its step comes: the status bar's layout
language and the inventory (M18v4); the menus' pages (M18v4); the `.dm2` format and protocol 34
(M18v9); the step move's ledge rules and the full game's other monsters (M18v7).
