# Shooter's Edge

The plan for the engine's second game: **Shooter's Edge**, a fast multiplayer arena shooter in
modern C++ on this engine, compatible with the mods, maps, servers and clients of the classic
multiplayer shooter whose formats and protocol it speaks. It starts once Fall Line is on the
engine (`docs/goals.md` M18 follows M10–M13 and M17k). Until a row lands, this page is the plan;
change it in the same PR that changes the plan.

The game lives in `games/shooters_edge/`. Outside the `porting/` working area nothing names the
game it is compatible with or its implementations: the formats are named by their file
extensions and versions, the language its mods are written in is QC, and the wire format is the
classic protocol (version 28). `make agents-check` holds it.

## What it is

- **Functionality, never code.** The game does what the classic game does (the movement a player's
  hands know, the QC a mod runs, the packets a server sends), written fresh in this engine's
  C++ from the file formats, the protocol and the language's reference, and from measured
  behaviour. No existing implementation's source is copied, translated or paraphrased
  (§ Licensing and the clean room).
- **Compatible where it counts.** Existing content runs unchanged: pak and pk3 archives, a game
  directory per mod, BSP maps, the model, sprite, texture and sound formats, QC bytecode (the multiplayer and single-player `progs`), config files, and recorded demos. Our client joins the servers
  people run today and their clients join ours (§ The network).
- **Better where it can be.** Everything the engine already does well applies: a modern GPU
  frame over Dawn, the synth's mixer, deterministic simulation with labs and goldens, the bots'
  decision records, the `ge` CLI. The modern features players expect from today's clients are
  in the plan (§ Modern features), not bolted on later.
- **Reusable first.** Anything a first-person shooter, or any game on the same map format, would
  use goes in `engine/` or a plugin; only what makes Shooter's Edge itself stays in
  `games/shooters_edge/` (§ Where the code goes).
- **A test of the `ge` CLI.** The port is also the proof that `ge` is complete enough for an
  agent to build a second, very different game with it. Every command the port finds missing is
  added to `ge` when it is found (§ The `ge` tooling it needs).

## Compatibility contract

What existing content needs from the game, and therefore what "compatible" means. A row is held
by a lab or a test over free content (§ Content and test data).

| Content | Formats | What compatible means |
| --- | --- | --- |
| Archives and search paths | `pak` (a directory of files in one blob), `pk3` (a zip, stored or deflated, with its own directory tree), a `.pk3dir` directory standing in for an unpacked pk3 while a mod is developed; loose files over them | The base directory, then a game directory per mod (`-game`, the server's `*gamedir`), archives in numeric then name order, later ones overriding earlier; the same file names a mod expects |
| Maps | BSP version 29, BSP2 (the larger-limits variant), the `.lit` coloured lightmaps beside them, BSPX lumps (extra data in the same file) | Every lump read: planes, nodes, leaves, the visibility (PVS, run-length coded), faces, lightmaps with up to four styles a face, textures (with animated and random sequences, liquids, sky), clip hulls (point, player, large), brush models, the entity lump |
| Models | MDL (vertex-animated, frame groups, skin groups, 8-bit packed vertices and normals from a fixed table), SPR (sprites with orientation types) | Every frame and skin a mod names; the player model's colours (top and bottom) remapped at draw time; frame interpolation as a modern client does it |
| Images | The 256-colour palette, the colour map (light levels to palette), LMP pictures, WAD2 (the 2D graphics: the status bar, the font) | Drawn as the classic game draws them, and also as true colour (§ The picture) |
| Sound | WAV (8 and 16 bit PCM, mono, any rate, the loop cue) | Played at their own rates, looped where the cue says, spatialized, with ambient sounds by leaf |
| Game logic | QC bytecode: the multiplayer `progs` and the single-player one | Every opcode, the entity field layout the progs declares, the globals, the string table, the builtins by number, the server's entry points (`StartFrame`, `PlayerPreThink`, `ClientConnect`…), the engine-side fields the server reads (`origin`, `velocity`, `movetype`, `solid`, `effects`, `modelindex`…) |
| Extended logic | The extension builtins and fields modern servers and clients share, asked for by name (`checkextension`) | The common set first (§ Modern features), each one a row of a table the VM answers from |
| Config and console | `.cfg` scripts, `exec`, `alias`, `bind`, cvars with server- and user-info, `wait`; the advanced scripting today's clients added (§ Configs) | A player's existing config runs, however advanced; a server's `server.cfg` runs; a cvar the game does not know is kept and written back, never an error |
| Demos | Client demos (one client's view), MVD (every player, written by the server) | Played back, seeked, and written; an MVD from another server plays here |
| The network | The classic protocol (version 28) and its common extensions | § The network |

## Configs

Players carry configs that grew for years in today's clients, and a competitive player will not
rewrite theirs. `engine/console` therefore runs the console language at its full modern extent,
specified in our own words in `porting/shooters-edge/configs.md` and held by a corpus of config
fixtures we write (`tests/golden/console_*.txt`):

- **The classic core**: `exec`, `alias`, `bind` and `unbind` (with the `+`/`-` pairs a held key
  sends), `set`, `seta` (archived), `setinfo` and `serverinfo`, `wait`, `echo`, `toggle`, the
  command buffer's order, quoting and `;` splitting, `//` comments.
- **Variables in commands**: `$cvar` expansion in any command line, with escaping; alias
  arguments (`$1`, `$2`, `$*`).
- **Conditionals and arithmetic**: `if` / `else` blocks over numbers and strings, `inc` and
  `dec`, `cycle` through values, calculation into a cvar (`set_calc`-style operators: arithmetic,
  comparisons, string length, substrings, search), temporary aliases.
- **Bind modes and modifiers**: separate bind tables for the game, the console, menus and demo
  playback; modifier keys in a bind; key names for every key and mouse button and the wheel.
- **Team messaging**: message macros expanded at send time (`$health`, `$armor`, `$ammo`, the
  weapon, `$location` from a map's location file), message triggers that run an alias when a
  matching line is printed, and the teamplay filters.
- **Config files as data**: `cfg_save` writing every archived cvar, bind and alias in a stable
  order, `cfg_load`, per-mod configs in the game directory, `autoexec.cfg`; HUD layouts as
  cvars, so a HUD config is a config.
- **Safety**: a server can stuff only what a client allows (the cheat and server-locked flags),
  a runaway alias loop stops at a depth with a message naming the alias, and a script's errors
  name the file and line.

## Playing it

The `shooters_edge` window (`games/shooters_edge/client/`, M18h's first slice) is the game on
one machine: the baseline mod (compiled at start from the QC sources built into the game) runs
as a LISTEN SERVER in the same process, and the player is its client, his commands handed to
the server's frame with no network between. It reads the maps out of your own copy of the
classic game's packs: point `-basedir` at the folder that holds `id1/pak0.pak` (and `pak1.pak`
for the full game's maps). The packs are never copied or committed (§ Content and test data).

**The short way, on any machine** (`make setup` once installs what the build needs; README.md §
Run a game):

```sh
make run:shooters_edge                                         # the built-in arena, no packs
make run:shooters_edge ARGS="-basedir ~/Games/classic +map dm6"  # your packs' maps
```

**Or load the game from the menu**: `make run:shooters_edge`, then ESCAPE ▸ OPTIONS ▸ CLASSIC
ENHANCED ▸ LOAD GAME and pick the folder your game is installed in (the one holding
`id1/pak0.pak`; picking `id1` itself works too). Only an install of the classic game is taken: a
folder without `id1` and its first pack is refused, and LOAD GAME's line says what it lacks. The
game is opened in place of the arena at once (its start map), its packs are enhanced in the
background (`ce_auto 1`, the default; the enhancing page shows every stage, and the run goes on
with the menu closed), and when the run ends having written anything the world is made again
from the cache, behind the one drawn. The folder
is kept (`game_folder.txt` in the platform's per-user folder) and opened at the next start when
`-basedir` names none and the folder the game runs in holds no game. `basedir <folder>` in the
console does the same as LOAD GAME (`client/game_folder.hpp`). Then `map dm3` plays that map.

**Dropped into an install, or beside several** (`client/install_search.hpp`). Started inside a
classic install (the folder the game runs in, or the folder its executable or app was placed in,
is an install or lies inside one: its base folder, its multiplayer folder, the bundled client's
folder), the game plays that install at once, ahead of the one kept. Placed in a folder ABOVE
installs, it finds each folder right under it that is one: the folders named like a classic
install first, in any case (the two games' own names, with or without a space or a 2, their
multiplayer client's names, the bundles' names, the sequel's short name), then any other folder
that holds a game. Each one's release is told by its packs' sizes (the first game's shareware or
registered packs, the sequel's demo or full game; any other size is your own copy); the game
loaded last is played when it is one of them, else the first full game. `games` in the console
lists them and `games <n>` loads one.

**An install's configs, in its own order** (`client/install_configs.hpp`). The bundled client's
configs come with the install: `default.cfg` on disk, then, when the bundled client's folder holds
`configs/config.cfg`, `autoexec.cfg` and that config, then `autoexec.cfg`, then the window's own
saved settings (so a change made here sticks), then the command line's `+set` pairs and its other
`+` commands. `exec` finds a file in the window's settings folder, the game folder, the bundled
client's folder and the base folder, then in the archives, which include the bundled client's
folder's own (its place names and fonts). Settings the window does not have are passed over and
counted on one line. The sequel's install runs its bundled client's order: `config.cfg`
(`q2config.cfg` on Windows, each tried after the other; a mod folder's before the base folder's),
then the base folder's `autoexec.cfg` and the mod's, `postexec.cfg`, the window's own settings, the
command line, and `postinit.cfg` last. Maps' coloured light is also read from `maps/lits/<map>.lit`.

**Place names, made in the game** (`client/place_names.hpp`). Each map's place names are read as
it starts (`tp_loadlocs`): the file saved under the game folder first, else the game's own.
Stand in a place and `addloc <name>`; `removeloc` forgets the nearest, `clearlocs` all of them,
`saveloc <name>` (or `saveloc $mapname`) writes `<game folder>/locs/<name>.loc` in the shared
format, `loadloc <name>` reads one, and `loc` says where you are. The long names
(`locations_add` and so on) work too, and so does the sequel's spelling: `loc add <name>`,
`loc del`, `loc set <name>` (renames the nearest), `loc list` and `loc save`.

**The title and the loads.** With no `+map` the window opens at once on the title
(`client/title.hpp`: a night with the full moon low in a total eclipse, blood red, cloud carried
across it) with the main menu up, the start map loading behind it; ESCAPE out of the menu plays it
once it is in. Every map loads behind the window (`client/map_load.hpp`): its file and its world's
textures and models read on threads, its pipelines compiled while frames go on, a spinner in the
bottom right corner naming the map and the seconds so far. A measured run (`--frames`, a
timedemo) waits for its map before its first frame.

**On a Mac (Apple silicon; Metal through Dawn):**

```sh
brew install cmake ninja                 # once
make build PRESET=release                # or only the window: cmake --preset release &&
                                         #   cmake --build --preset release --target shooters_edge
build/release/games/shooters_edge/shooters_edge -basedir ~/Games/classic +map dm6
```

**On Linux (Vulkan through Dawn; `sudo tools/setup-ubuntu.sh` installs what the build needs):**

```sh
make build PRESET=release
build/release/games/shooters_edge/shooters_edge -basedir ~/games/classic +map dm6
```

- **`-basedir <dir>`**: the folder holding the game folder; `~` is your home folder, and a path
  with spaces goes in quotes (`-basedir "/Volumes/Old Games/classic"`). When the install has the
  multiplayer client's folder (its player skins, its multiplayer program), that folder is
  played over `id1` unless `-game` names another. **`-game <folder>`**
  plays a mod: its folder is mounted over `id1`, so the mod's files win and everything it does
  not ship comes from the base game (saves go to the mod's folder). **`-progs <file>`** runs a mod's progs in place of the
  baseline mod (§ Running your own mod), and **`-models <dir>`** names the held weapons' folder
  (by default the tree's `games/shooters_edge/content/models/view`). Each folder's packs are read in number order (`pak0.pak`, `pak1.pak`,
  ...; any case), then its other packs (a mod's `misc.pak`), zips (`*.pk3`) and unpacked zips (`*.pk3dir`), and a loose file in the
  folder (`maps/mymap.bsp`) overrides them all (`client/files.hpp`, over `engine/vfs/search.hpp`).
- **`+<command>`**: console lines run at start. `+map dm6` loads that map; `+map arena` the
  built-in test arena (no packs needed, only the sky drawn). With no `+map`, the map `start`
  when the packs have it, else the arena. `--size=WxH` sets the window's size, `--frames=N`
  quits after N frames.
- **The keys**: W A S D move, SPACE jumps, the left button fires, 1 to 8 pick a weapon (7 the
  rocket launcher), the wheel the next one. The mouse looks (raw counts, captured) from the
  moment the window gets the focus, by a click on it or on the app's icon in the Dock or the
  taskbar, with no second click; only the console lets it go by default, and closing the console
  takes it back. `in_mouse_release` names what frees it (bits: 1 the console, 2 a menu, whose
  pointer then hovers and clicks; 4 a demo's play bar; default 5), so a menu is driven by the keys
  unless it holds 2. Closing the window quits.
- **The main menu** (ESCAPE; ESCAPE again goes back, and out of it at its root): the classic
  game's pages (§ The main menu), every row the game can do working and the rest dimmed.
- **The console** (the key under ESCAPE): `map <name>` changes the map, `sv <line>` runs a line on
  the server's console (`sv timelimit 10`, `sv fraglimit 20`, `sv sv_gravity 600`),
  `sensitivity <degrees a mouse count>` (0.066 by default), `m_invert 1` (both cvars, saved
  with the player's settings), `menu_look auto|classic|modern`, `togglemenu`, `r_look classic`
  (the pack's own picture; `modern`, the default, is our sky, filtered textures, real-time
  lights, water, lava, slime, torch flames and weather) with the picture's other cvars
  (`r_fx_lights`, `r_fx_glow`, `r_bloom`, `r_lava_glow`, `r_slime_glow`; the bloom's from the next
  map), every classic-enhance switch (`ce_water 0`, `ce_flames 0`, `ce_shadows 0`, ...;
  `ce_textures` and `ce_bloom` from the next map), the three dearest switches' levels
  (`ce_shadows_quality`, `ce_water_quality`, `ce_skies_quality`: `low`, `medium` or `high`) and
  `r_quality` over all three (`custom`, the default, leaves them as set; `low`, `medium`, `high`,
  `top`, `auto` inside `r_budget` ms, or 0..1: the quality ladder, `app/ladder.hpp`), the
  weather's (`sv_weather 1` brings the rain) and `quit`.
- **The campaign** (M18s4): ESCAPE ▸ SINGLE PLAYER ▸ NEW GAME (or `newgame` in the console)
  plays the packs' own single-player program from the map `start`, on the campaign host
  (`server/campaign.hpp`): the maps in a chain, the next one read when the program changes level,
  the parms and runes carried, the game paused while the menu or the console is up. Lit when
  the packs carry a `progs.dat` of the single-player form (or `-progs` names one). `map <name>`
  then starts a new game of it on that map while the server's last `deathmatch` is 0; a
  MULTIPLAYER ▸ NEW GAME sets its rules and goes back to the deathmatch program. The level's
  end is the program's (`client/campaign_view.hpp`, drawn by `menu/level_screens.hpp`): the
  intermission (the pack's `complete` and `inter` pictures, the time, the secrets and the kills
  in the big figures), an episode's text typed out at eight characters a second, and the
  shareware episode's end opening the help pages; TAB held shows the solo scorebar in the
  status bar's place (monsters, secrets, the time, the map's title) and, with `scr_levelstats 1`
  (the default), the level's figures live as the intermission draws them. In a match TAB held
  shows the scoreboard (`menu/scoreboard.hpp`, read off the server by `client/scores.hpp`): the
  pack's `ranking` picture, the players by frags with their ping, loss, minutes, colour box and
  name, spectators last, a team column and the teams' board first with teamplay; the teams'
  board shows by itself while the player is dead. The HUD shows the keys, the runes, the cloak and the suit, each
  read by the program's own item constants. Each frame the server hands the player his own data
  (`server/client_data.hpp`): a blow he took kicks the view away from where it came from and
  flashes the screen (the blood red, what the armour took a duller amber); the weapon's punch
  kicks the view (never the aim) and springs back as the server decays it; and with
  LOOKSPRING on (`lookspring 1`) the pitch drifts, while he walks with the mouse still, to the
  slope ahead (`sv_idealpitchscale`: down a stair the view looks down). The program's
  temporary entities (gunshots and spikes sparking off the walls, explosions, teleports, lava,
  the beams' ends) and its blood bursts are drawn by the game's effects. `save <name>` in the
  console keeps the game as it is in `<game folder>/saves/<name>.save` and `load <name>` brings
  it back (`server/campaign_save.hpp`): the whole running game on `engine/save`, the map started
  again and the save read over it, so a loaded game plays on exactly as the one never saved did.
  Only a single-player game saves, and not at the level's end or with the player dead.
  `save <name>.sav` writes the classic text save instead, `<game folder>/<name>.sav`, where the
  classic game keeps its own (`server/classic_save.hpp`), and `load <name>` tries ours first,
  then `<name>.sav`, so a game saved by another engine loads too (it plays on from the program's
  state, not bit for bit). The menu's LOAD and SAVE pages are twelve slots, `s0` to `s11`, each
  showing its save's line. On a Mac:

  ```sh
  build/release/games/shooters_edge/shooters_edge -basedir ~/Games/classic
  # then ESCAPE, SINGLE PLAYER, NEW GAME; later the console: save s1 ... load s1
  ```
- **A rocket jump**: 7 for the launcher, look straight down, then SPACE and the left button
  together. The blast costs about half your health and throws you about four times as high as
  a jump (held by `tests/shooters_edge_listen_test.cpp` and the `se-client` lab).

What the first slice draws: the map in its classic look (its textures, lightmaps, warped liquids
and sky) over the engine's sky, from the first-person rig (the view height off the mod's view
offset, stairs smoothed, a landing's dip); the game's effects (`client/world_fx.hpp`: a rocket's
trail and glow, an explosion's sparks, smoke and light on the walls, the red flash of a hit and
the tint under water, slime or lava); the things standing in the map (`client/world_models.hpp`,
M18i3: every model, sprite and brush item on the server's model list loaded from the packs once a
map, then drawn each frame where the server's entities and its static ones stand: the weapons
lying about, armour, health and ammo boxes, the torches and their flames, the powers, the other
players in their own top and bottom colours; a group of frames plays at its intervals, an item
flagged to spin turns, each lit by the lightmap on the floor under it, shaded from above, its
fullbright texels glowing; with `ce_models` on and the pack enhanced, each skin is the
cache's upscaled copy, raised to twice the pack's size and then toward the cache's own scale
while the models' one atlas holds them all, read when the map loads); the weapon in hand (`client/held.hpp`: the game's free
models, switched with the number keys, kicked and lit by its muzzle flash as the mod fires it,
lit by the map's baked light under the player, every shot firing the engine's muzzle flame and
sparks, no smoke (`app/muzzle.hpp`; `ce_muzzles 0` leaves the light alone); `r_drawviewmodel`,
`r_viewmodel_fov`, `cl_gunx`, `cl_guny`, `cl_gunz`, `cl_gunsway`, `r_viewmodel_center` (1 by
default: centred under the crosshair; 0 to the right) and `r_viewmodel_hands` (0: the weapon
without its hands) in the console); the HUD (`hud_style
classic|modern|compact|arena|tactical|minimal`, the classic status bar by default; `hud_sbar 0`
its pieces without the panels, `hud_vitals left|right` the side health and armour stand on in every other look, `hud_guns
ammo|pairs|row`, `hud_guns_all`, `hud_ammo_all`; the weapon in hand raised over the classic bars
while they show; drawn in the packs' own pictures when
a classic game is loaded, the free art only for Shooter's Edge's own content; `viewsize
110` drops the inventory bar and `viewsize 120` every bar); the console; the main menu (below). The map's own moving brush
models (doors, plats, buttons; M18i6) are drawn where the server holds each one's entity, and one
the game removed (a secret wall the mod does not keep) is not drawn: `brush/app/bsp_draw.hpp`'s
`set_movers`, fed each frame by `client/world_models.hpp`'s `place_brushes`; their shadows in the
modern look stay where the map built them. Not yet: sounds and the network (the rest of M18h,
M18i).

The baseline mod's POWERS (`qc/items.qc`): the quad (`item_artifact_super_damage`, four times
the damage dealt) and the shield (`item_artifact_invulnerability`, no damage taken), 30 s each,
back 60 s after they are taken. A power held glows round its holder (`effects`: blue the quad's,
red the shield's; the window lights the walls round him and tints his own screen). The torches
and flames (`light_torch_small_walltorch`, `light_flame_*`) are static things the clients draw. In
the modern look with `ce_flames` on, the torch's model is drawn without its painted flame and the
engine's burning gas is the flame.

**What a pickup says.** The baseline mod names an item in the loaded game's own words: the game's
profile has a table of names by class name (`profiles/items.hpp`'s `names`; the classic game's
`kClassicNames`: "shells", "nailgun", "Double-barrelled Shotgun", "Red Armor", "Pentagram of
Protection"), which the window gives the server as local info (`localinfo weapon_nailgun
"nailgun"`) when the mod plays a game's packs, and the mod's `name_of` (each variant's `glue.qc`)
reads it under the item's class name. So "You got the rockets", "You got the Red Armor", and a
backpack lists what was in it ("You get the Rocket Launcher, 20 shells, 5 rockets", the weapon only
when it is new to its taker). On the built-in arena, with no packs, the mod speaks its own words
("You got ammunition", "You got the Spiker", "You got armor").

**What the server says.** Everything else a player reads from the server comes in the loaded
game's words too: a second table in the profile (`profiles/words.hpp`'s `kClassicServerWords`),
each a template in plain ASCII (`server/words.hpp`: `%1` a name, `%g1` its digits in the gold
figures, `^red text^`, `%[ %]` the gold brackets, `%>4g1` right-aligned, `%n` a newline), put in
the server's local info beside the item names by the window and by the dedicated server when the
game folder holds packs. The mod fills one with the `words` builtin (#640, `GE_QC_WORDS`):
- **Deaths**: one line to everyone at the medium level, chosen by the weapon, whether the victim
  was gibbed (health below -40) and whether the killer had the quad ("bob rides ann's rocket",
  "bob was gibbed by ann's rocket", "ann rips bob a new one"), a teammate's kill (-1 to the killer:
  "ann checks his glasses"), one's own ("bob discovers blast radius") and the world's ("bob sleeps
  with the fishes", "bob burst into flames"); where a death has variants one is drawn from the
  game's stream. Before the match (the lobby's warmup and countdown, `ge_phase` in the local info)
  nothing is told or scored, and a player who leaves is told only in the match ("bob left the game
  with 3 frags").
- **The clock**: each whole minute left ("[19] minutes remaining" in gold, and in a two-team game
  "Team [red] leads by 3 frags" or "The game is currently a tie"), then 30, 15 and 10 to 1 seconds;
  the end is "The match is over" and, in a team game, the teams' scores and shares.
- **The powers**: "Quad Damage is wearing off" and "Protection is almost burned out" 3 s before.
- **The lobby** (`server/announce.hpp`): "ann is ready - no matchtag set" (red text after the
  name), "All players ready", "Timer started", "bob stops the countdown", "The match has begun!",
  and each second of the countdown the centre print with the seconds in gold and the match's
  settings a line each (the window's lobby panel then leaves out its own big count). Its start
  plays `items/protect2.wav`, its last five seconds `buttons/switch04.wav`.
The built-in arena, and Shooter's Edge's own content, keep the mod's and the lobby's own words.
`make lab LAB=se-words` draws both side by side in a console font.

### Benchmarking a demo

`timedemo <demo>` in the console, or `-timedemo <demo>` on the command line (which quits with
the report and exits non-zero if the demo stopped early), plays a recorded game **as fast as the
machine draws it**: every frame drawn, one in 33 ms shown and the rest drawn offscreen (a
window's present waits for the display on some platforms whatever its mode: on macOS a window's
image comes back once a refresh), no frame cap, one demo frame a drawn frame whatever
the demo's own clock says, at the window's settings (its size, `r_look` and the picture cvars, `hud_style`). The
listen server waits meanwhile; ESCAPE stops the run early. At the end the console and the
terminal print the frames timed and their seconds, the **average** frame rate, the **max** and
**min** (one over the shortest and longest frame), the **1% low** and **0.1% low** (the slowest
1% and 0.1% of frames as a rate over their own time), the median and 99th-percentile frame, and
the median **GPU span** of a frame (where the GPU has timestamp queries). What each thing the look
draws costs is the lab's `--price` (below), never a pass's own timestamps: a tiled GPU overlaps
the passes and each one's reading stretches over its neighbours' work. `+vid_fullscreen 1` (or
the menu's VIDEO ▸ FULLSCREEN) runs it over the whole screen at the desktop's own mode, on the
display the window is on or on `vid_display n`'s (`vid_displays` lists them). The `-` flags are
read wherever they stand, so a `+` console line ends at the next flag, and a flag the client does
not know is refused wherever it stands (with the console line it followed named) rather than read
as one of the line's words; `-benchmark` says it is now `-timedemo`. A negative number (`-1`) is
a word:

```sh
build/release/games/shooters_edge/shooters_edge -basedir ~/Games/classic -timedemo demo1
build/release/games/shooters_edge/shooters_edge -basedir ~/Games/classic +vid_display 2 \
    +vid_fullscreen 1 +r_look modern -timedemo demo1
build/release/games/shooters_edge/shooters_edge -basedir ~/Games/classic --size=2560x1600 \
    -timedemo ~/Downloads/match.dem
```

- **Which demo**: a path (absolute, or starting `./` or `../`) is read from disk alone; else the name through the search path (`demo1.dem` in `pak0.pak`, or a loose file
  in the game folder), with `.dem` added when the name has no extension, then any file in the
  packs whose name before its extension matches; else the name as a path on disk.
- **The formats**, told apart by their first bytes (`client/demo.hpp`), never by the name: the
  single-player demo (protocol 15, `.dem`: the shareware pack's `demo1` to `demo3`), the
  multiplayer client's recording (protocol 28, with the extensions that change how a message
  reads) and the server's **multiview recording** of every player (`.mvd`, below). A gzip file
  (`.mvd.gz`, as recordings are usually downloaded) is unpacked on the way in.
- **What is timed**: every frame but the first few after a level starts (the map's load and its
  pipelines' first use; `kSkipAfterLevel`). A level change inside the demo loads that map from
  the packs. What is drawn is what the window draws today: the map from the recorded eye and
  the HUD off the recorded stats (the demo's effects are not read yet); the entities' models join when the window draws them (M18i).
- **Headless**: `make lab LAB=se-timedemo` runs the same loop into an offscreen target, on a
  demo written in code round the fixture halls (`--format=client` or `--format=multiview` for the
  other formats), or on a real one with `ARGS="--basedir=<dir> --demo=demo1"`, and charts every
  frame's time and GPU span (`--look=classic|modern` picks the look). It paces the frames as the
  window does, one in flight, and leaves its three pictures' frames untimed; until round 8 of the
  timedemo it waited for every frame before starting the next, which added the GPU's round trip
  (about 0.6 ms on the M3 Max) to every frame. `cpu by zone` lists each timing zone's median a
  frame: the CPU's part of the world's frame is about 0.6 ms at 720p, most of it Dawn's submit
  (the frame's commands made into Metal's) and the BSP's prepare.
- **The price** (`ARGS="... --look=modern --price=1"`): the demo played again with each drawn
  `ce_` switch off in turn and once without the HUD, each run between two runs with everything on;
  a switch's saving is read against the mean of the all-on runs either side (so a machine warming
  or getting busier over the minutes reads as no saving), and the largest gap between neighbouring
  all-on runs is the noise. The lab charts each run's frames as a box and lists what each switch
  saved off the median frame, the most first.
- **The shadow maps** (the modern look): the shadowed few (48) are the nearest lights in the
  eye's potentially visible set whose reach comes into the view first (the cone round the look
  that holds a 32:9 picture), then the nearest of the others in that set, then the nearest of the
  rest; by nearness alone the map's big lights behind walls took half the slots on an open map (a
  dm4 doorway: 14 of the 30 lights in view lit unshadowed, their shadows popping in a few steps
  on). A light with no slot shines through what stands before it: with 32 slots, start's corridor
  (57 lights in view) left the lamps before the NORMAL hall's slipgate unshadowed, and the frame's
  shadow on the wall behind it (in the map's own lightmap too) came only once the player walked in.
  48 cost about 5% of a frame on llvmpipe over 32 (demo1, the modern look: 358 -> 375 ms median). A light's cube is drawn when
  it joins the few and kept while it stays; at most two new cubes a frame (`Options::fresh_cubes`), the rest of a
  room's torches unshadowed for the frames until their turn, so walking into a room costs no
  frame more than two cubes; a dynamic light (a flash, a rocket) lights without a shadow.
- **The light grid** (the modern look): the frame's up to 160 lights are binned on the CPU into
  4 m world cells round the eye (32 x 16 x 32), each cell listing the lights whose reach touches
  it, in order; a pixel loops over its cell's lights only, not all of them. A light left out of a
  cell is one the pixel would have skipped by its reach, and the rest are summed in the same
  order, so the picture is byte for byte the same (`se-client` on e1m1 and the fixture, on the
  M3 Max). 2 m cells saved another 0.1 ms of GPU and cost more than that in binning.
- **The lean pipeline** (the modern look): the map's draw is compiled twice from one shader, and
  a frame lit in real time with no weather takes the copy with only that path (`bsp.wgsl`'s
  `kLitOnly`), so its shader carries none of the lightmaps', the rebake's or the weather's code;
  a light wholly in its cube's shadow is skipped before its highlight. The same pixels again.

### Watching a demo

`playdemo <demo>` in the console (`-playdemo <demo>` on the command line, which quits when the
demo ends) plays a recorded game **at its own pace**: vsync on, nothing timed, each drawn frame
moving the demo's clock on by the frame's wall time (at most a tenth of a second, so a stall
slows the demo instead of skipping it). `cl_demospeed <x>` sets the speed (1 by default, up to
20; 0 pauses). ESCAPE pauses it and opens the main menu with the paused demo drawn behind it;
ESCAPE out of the menu plays on (a demo paused by hand stays paused), and whatever the menu or the
console starts there (a map, a save, a server, another demo, another game folder) stops it. The
demo is found and drawn as the benchmark finds and draws it:
the map from the recorded eye, the HUD, and every entity the demo holds (the other players in
their colours, the items, the missiles, the level's statics), as `-demovideo` draws them.

**The demo's game folder.** A multiplayer recording names the game folder its server ran
(`fortress` for a team mod). When the window was opened on the base folder (no `-game`) and that
folder is beside it under `-basedir`, the demo plays from it, the base under it, so the mod's own
models, skins and maps are found; a `-game` you pass always wins. A folder that is not there is
named in the console with what to do.

**Heard and read.** The window's own game, a match on its listen server or the campaign, is heard
through the same players (`client/listen_sound.hpp`): each sound the server starts for the player
(a shot, a door, a lift, a pickup) at its entity, moving with it, the map's loops from its load,
and the ambience by leaf, for a listener at his eye. The built-in arena, with no classic game's
folder open, is heard through the game's own recordings: `content/classic.assetmap`'s `sound` rows
mount each under the classic name the baseline mod plays (`client/own_sounds.hpp`), never over a
classic game's own folder. A played demo is heard on the window's audio
device: its sounds (the pack's
recordings, moving with what makes them, the level's loops and ambience) and the players' voices,
the same players `-demovideo` renders (`client/demo_heard.hpp`), at `volume` (0..1, 0.7; the
menu's SOUND VOLUME). The lines the server printed to the player the view rides (the deaths, the
chat and team chat, the pickups) go to the HUD's feed and the console as plain words (the charset's
coloured letters and the colour marks clients write into chat taken out), and a centre message to
the HUD's centre for two seconds. A seek plays nothing of what it skipped.

```sh
build/release/games/shooters_edge/shooters_edge -basedir ~/Games/classic \
    -playdemo ~/Downloads/match.mvd.gz +demo_track 2 +cl_demospeed 2
```

(The `+` words go last: everything from the first `+` on is the console's.)

**Like a video.** While a demo plays the mouse is free and a play bar shows along the bottom
(above the HUD) for a few seconds after the pointer moves or a key is pressed, and always while
paused (`demo_bar 0|1|2`: never, so, always). The demo is scanned once when it starts for its
HIGHLIGHTS, and the tracked player's are marked on the bar in their kind's colour; the pointer over
a marker names it.

| Key | Console | What |
| --- | --- | --- |
| a click or drag on the bar | `demo_seek <time>` (`83`, `1:23`) | go there |
| Space, K, the bar's button | `demo_pause` | pause or play |
| Right, L | `demo_skip 15` | forward `demo_skip_forward` seconds (15) |
| Left, J | `demo_skip -5` | back `demo_skip_back` seconds (5) |
| Up, Down | `cl_demospeed <x>` | faster or slower: 0.25, 0.5, 1, 2, 4, 8 (any 0..20 by hand) |
| Page Down, N / Page Up, P | `demo_next`, `demo_prev` | the next or last highlight (a few seconds before it) |
| Home, End | | the start, the end |
| | `demo_highlights [all]` | the tracked player's highlights (or everyone's) in the console |

A HIGHLIGHT is a moment of the tracked player's (`client/demo_highlights.hpp`): a MULTI KILL (two
or more kills within 0.2 s: one rocket, one blast), a LONG ROCKET (a rocket kill whose rocket flew
1000 units or more), an AIR KILL or AIR GIB (a rocket or grenade kill of a player in the air), an
ANNIHILATION (the victim had 150 or more health and armour and lost it all within 2 s), a TELEFRAG
and a STREAK (three kills or more within 10 s). The kills are read off what every recording
carries (`client/demo_scan.hpp`): a player's dead bit or his health going to 0 is a death, the
other player whose frags went up then is the killer, the rockets are followed from where they
appear to where they go. No obituary text is read. `make lab LAB=se-highlights
ARGS="--file=<demo>"` lists a demo's highlights and kills and pictures every player's bar.

A seek back reads the demo again from its start without drawing (a 10 minute match in a fraction
of a second); the times on the bar are the demo's TIMELINE, its frames' steps added up with a long
pause in the recording counted as a quarter second, the clock `-demovideo` runs on.

**A multiview recording** holds every player, so the view rides one of them: the lowest slot
playing by default, or the one `demo_track <slot>` names (`demo_track -1` goes back to the
default). The view takes that player's origin and his own view angles, his stats for the HUD and
the messages the server sent to him alone. Records written for no client (the recorder's own
data) are skipped. The demo's clock is the sum of the records' milliseconds. `make lab
LAB=se-demo ARGS="--file=<demo>"` reads any demo without a GPU and draws every player's path
from above and his health over the clock, with a table of who played (`--track=<slot>` picks
the view).

**Smooth at any rate.** A played demo is drawn between its last two frames: the playback reads a
frame once its clock passes the one before, and each picture places the players, the entities and
the eye between the two by the time that passed, angles turned the shortest way round
(`client/demo_lerp.hpp`). A recording made at 77 frames a second then moves evenly at 60, 120 or
144 pictures a second and in a 30 fps video, instead of repeating one frame and jumping two. A
teleport, a respawn, a gib and a model change are not drawn across, and a view the server turned
jumps. `demo_lerp 0` draws each frame as read (`timedemo` always does: one frame a picture).
`make lab LAB=se-demo-smooth ARGS="--file=<demo> --track=<slot>"` measures it: how far the eye
moves each picture with and without, charted.

**Models drawn between their frames** (`client/model_lerp.hpp`, every path: your own game, a
played demo, a joined server). A classic program steps a model's frame ten times a second; when an
entity's frame changes, the window draws it from the frame it left to the new one over the time its
frames last (the gap between its last two changes, 20 to 150 ms; at first 0.1 s), the GPU blending
the two frames' vertices, and a looping group (a torch's flame) flows between its own frames. In
your own game the places are drawn too: every model one server frame behind, between the last two
states the window saw (the server's 72 or 77 frames a second fall unevenly on 50, 60 or 144
pictures), and a walking monster, which the program moves a stride at a time, drawn over each
stride's time rather than jumping. A first sight, a new model, a warp, a move over 128 units and a
gap over a quarter second are not drawn across. The held weapon's frames flow too: a nailgun's shot
frame from the last, and a played fire clip back to rest over a frame's time; in a demo or on a
joined server a classic set's weapon plays the weapon frame the server sends, blended between its
changes. Your own eye rides the same clock as the models: drawn one server frame behind, between
the server's last two states. A played demo's walking monster glides over its strides too
(`demo_lerp.hpp`'s stride rule). `r_lerpmodels 0` and `r_lerpmove 0` draw each state as held. `make lab LAB=se-model-lerp ARGS="--game=<base>/<folder>"`
films a monster walking up to the player both ways side by side (a GIF) and measures the judder
and the pose jumps.

### A demo as a video

`-demovideo <demo> <file>` turns a recorded game of any format into a video and quits; no window
opens (`client/demo_video.hpp`). The demo is played at its own pace on a fixed frame clock of
`--fps=N` pictures a second (video.cfg's 60 by default; 30, 24, 25 or any rate over 0, fractions too), each
picture drawn offscreen on the GPU by the window's own world frame: the map
from the recorded eye, the entities, doors and other players where the demo has them, the level's
statics and the HUD off the recorded stats. Its sound is the demo's own: every sound message
plays the pack's recording (`sound/<name>`) placed where the demo says, the level's static sounds
loop, the map's ambience follows the eye, and the players' voices in a voice format play over it
all. A sound starts on the sample its frame's time falls on, so what is heard and seen are within
a frame of each other however slowly the machine draws. The file is written by `engine/capture`:
`.mp4` (H.264 or HEVC from the platform's encoder), `.webm` (AV1 and Opus) or `.mkv` (any;
`--codec=none` uncompressed); `--codec` auto, h264, hevc or av1; `--resolution` the picture,
as `WxH` (each side 16 to 8192, an odd side rounded down to even) or a name: `480p`, `720p`,
`1080p` (video.cfg's default, below), `1440p`, `4k` (`--size` is the same flag);
`--seconds` the most video. The `+` lines run first, so `+r_look classic` and `+demo_track 2`
shape it.

**The words and the HUD.** The picture's upper left says what the video shows
(`client/demo_caption.hpp`): the map and its title in gold, the clock on the play bar's timeline,
whose eyes it is and their frags (a single-player level: the kills and secrets out of its totals),
and the match: the teams' frags summed, best first, or a free-for-all's leader. The words are in the
packs' own console font when they carry one (`conchars` in `gfx.wad`, or classic-enhance's copy in
the pack's cache, `pictures/enhanced/conchars.png` first, which wins, so an enhanced font is what a
video shows), else the engine's charset, a whole number of screen pixels to a font pixel (8 px a
line at 360 lines, 24 at 1080). The status bar is drawn in the packs' pictures when they have them
(enhanced when `ce_pictures` is on and the cache has them), as in the window.

**The messages.** Under the words go the notify lines the recorded player read, as the classic
client showed them: the pickups, the deaths, the server's words and the chat (`say` and
`say_team`; a team's lines are in its own players' recordings only), the newest last, four at
most, a line for 4 s and a chat line (in gold) for 6 s. A print the game sends in pieces is shown
once its line ends. A centre print, and an episode's closing text, is drawn in the middle for 2 s
or longer for a long text. `playdemo` in the window shows the same in the HUD's own notify feed
and centre message.

**video.cfg.** A video is drawn in the newer look whatever the player plays with
(`client/video_cfg.hpp`). After the usual startup (`default.cfg`, `config.cfg`, `autoexec.cfg`,
the `+` lines), `-demovideo` resets `r_look` (modern), the look's other `r_` cvars and every
`ce_` switch to the engine's defaults, then reads the player's own `video.cfg` from the game
folder (`<basedir>/<game>/video.cfg`), or else the current folder, then runs the `+` lines again,
and the flags `--resolution`, `--fps` and `--codec` win over the cvars a video.cfg sets:

| cvar | default | what |
| --- | --- | --- |
| `demovideo_size` | `1080p` | the picture: `WxH` or 480p, 720p, 1080p, 1440p, 4k |
| `demovideo_fps` | `60` | pictures a second |
| `demovideo_codec` | `auto` | auto, h264, hevc, av1, none |
| `demovideo_text` | `1` | the words in the upper left |
| `demovideo_messages` | `1` | the chat, the notify lines and the centre prints |
| `demovideo_hud` | `1` | the status bar |

```
// ~/Games/classic/id1/video.cfg: 1440p at 60, more bloom, the map's own sky
demovideo_size 1440p
r_bloom 0.45
ce_skies 0
```

**A clip.** `--from=m:ss --to=m:ss` cuts the part between two times on the play bar's clock (the
times `playdemo`'s bar and `demo_seek` use), and `--highlight=N` cuts the tracked player's Nth
highlight, its window from a few seconds before the moment to a few after (`demo_highlights` in
the console lists them numbered while the demo plays; `+demo_track <slot>` picks another
player's). The frames before the clip are read and not drawn. A file named `.gif` is an animated
GIF (no sound, `--gif-width=N` wide, 480 by default; `--fps=15` keeps it small) and needs no video
encoder; `.webm`, `.mp4` and `.mkv` are videos with sound as above.

```sh
build/release/games/shooters_edge/shooters_edge -basedir ~/Games/classic \
    -demovideo demo1 ~/Movies/demo1.mp4
build/release/games/shooters_edge/shooters_edge -basedir ~/Games/classic \
    -demovideo ~/Downloads/match.mvdv ~/Movies/match.mp4 +demo_track 3
build/release/games/shooters_edge/shooters_edge -basedir ~/Games/classic \
    -demovideo ~/Downloads/match.mvd.gz ~/Movies/airgib.webm --highlight=3 +demo_track 2
build/release/games/shooters_edge/shooters_edge -basedir ~/Games/classic \
    -demovideo ~/Downloads/match.mvd.gz ~/Movies/airgib.gif --from=4:31 --to=4:39 --fps=15
```

What is not drawn yet: the demo's temporary entities (gunshots, blood, explosions) and the
weapon in hand. `make lab LAB=se-demo-video` records a match and turns it into videos headless
(below).

### Demos with voice: `.qwdv` and `.mvdv`

A recording with the players' voices in it is a **format of its own**, so a player that knows
only the plain formats never meets a message it cannot read: the voice header (8 bytes, `GEVR`,
the version, the base format, the codec), then the client recording's or the multiview
recording's records unchanged, with the protocol's voice message (svc 84, the x2 family's voice
bit) among their messages. Without the header and the voice messages it is a plain recording.

- **The server relays voice** when `ServerConfig::voice` is set: it offers the voice bit, and each
  voice message a client sends (clc 83: 20 ms Opus frames) goes, with the talker's slot, to the
  clients that agreed the bit and may hear it. By `voice_route` that is everyone, or (the
  default) only the players whose user info names the talker's `team`. A talker never gets his
  own back.
- **A client's recording (`.qwdv`)** holds what its player was sent: his team's voices, routed to
  him, and his own, spliced in as he spoke. A plain client recording leaves every voice message
  out (`net/classic_record.hpp`).
- **The server's multiview recording (`.mvdv`)** holds every voice it relayed, whoever heard it:
  all the teams.
- **The window joined to a server** (`connect`, `net/classic_remote.hpp`) asks for voice with
  `cl_voip 1` and hears what the server routes to the player through the same players, its own
  voice left out; while `+voip` is held (V) the microphone is sent. The live recording it reads
  (`ClientRecorder::live`) keeps the voices with no header; a file it records is unchanged.
- **The window's own match** relays voice too (`sv_voip 1` while `cl_voip` is on): its player
  hears the joined players and is heard while `+voip` is held (`listen_mic`). In a free-for-all
  (nobody on a team) the team route reaches everyone.
- **Playing one back** (`playdemo`, `-demovideo`, the labs): the reader finds the header by its
  bytes; each talker's frames play on a stream of their own, a gap of up to three frames
  concealed by the codec (`client/demo_voice.hpp`).

`make lab LAB=se-demo-video` plays a match on the fixture halls with two teams of two talking
in turn, records red 1's `.qwdv` and the server's `.mvdv`, charts who is heard in each and turns
both into videos; `ARGS="--basedir=<dir> --map=dm3 --voices=<dir>"` plays it on a map of the
game folder's, each client speaking `<dir>/<name>.wav` (red1, red2, blue1, blue2), and
`ARGS="--basedir=<dir> --demo=demo1"` turns that demo into a video instead.

### The main menu

`games/shooters_edge/menu/` (M18k2) is the menu ESCAPE opens over the running game (the listen
server's frames go on under it). Its pages are the classic game's:

| Page | Rows |
| --- | --- |
| main | SINGLE PLAYER, MULTIPLAYER, OPTIONS, HELP, QUIT |
| SINGLE PLAYER | NEW GAME (`newgame`: the packs' campaign from `start`), LOAD and SAVE (twelve slots `s0`..`s11`, each its save's line: `load s<i>`, `save s<i>`) |
| MULTIPLAYER | JOIN A GAME, NEW GAME (the rules, then START), SETUP (name, shirt and pants colours) |
| NEW GAME | MAX PLAYERS, GAME TYPE (`deathmatch`), TEAMPLAY, SKILL, FRAG LIMIT, TIME LIMIT, MAP, START |
| SETUP | NAME, SHIRT COLOUR (`topcolor`), PANTS COLOUR (`bottomcolor`): the player's user info, sent to the server as it changes |
| OPTIONS | CUSTOMIZE CONTROLS, GO TO CONSOLE, RESET TO DEFAULTS, SCREEN SIZE, BRIGHTNESS (`gamma`), MOUSE SPEED (`sensitivity`), MUSIC VOLUME, SOUND VOLUME, INVERT MOUSE (`m_invert`), LOOKSPRING, LOOKSTRAFE, VIDEO OPTIONS, CLASSIC ENHANCED |
| CUSTOMIZE CONTROLS | ATTACK, MOVE FORWARD, MOVE BACK, STEP LEFT, STEP RIGHT, JUMP, WALK / RUN (`+speed`), SIDESTEP (`+strafe`), TURN LEFT, TURN RIGHT, NEXT WEAPON: each the keys bound to it; ENTER, then the key (BACKSPACE clears, ESCAPE leaves it, a third key replaces both) |
| VIDEO | WORLD LOOK (`r_look`), HUD (`hud_style`), MENU LOOK (`menu_look`), FULLSCREEN, HELD WEAPON |
| HELD WEAPON | SHOW WEAPON (`r_drawviewmodel`), HANDS AND ARMS, CENTRED, SWAY (`cl_gunsway`), MUZZLE FIRE (`ce_muzzles`) |
| CLASSIC ENHANCED | LOAD GAME (the desktop's folder chooser: your install), ENHANCE (the loaded game's enhancing, stage by stage), TEXTURES AND MODELS, LIGHT AND SHADOW, WATER, LAVA AND SKY, FIRE AND WIND, WEATHER |
| TEXTURES AND MODELS | `ce_textures`, `ce_relief`, `ce_parallax`, `ce_models`, `ce_smooth`, `ce_pictures` |
| LIGHT AND SHADOW | `ce_rebake`, `ce_shadows`, `r_fx_lights`, `r_fx_glow`, `ce_bloom`, `r_bloom` |
| WATER, LAVA AND SKY | `ce_water`, `ce_lava`, `r_lava_glow`, `ce_slime`, `r_slime_glow`, `ce_teleports`, `ce_warps`, `ce_skies`, `ce_clouds` |
| FIRE AND WIND | `ce_flames`, `ce_bonfires`, `ce_wind` |
| WEATHER | `sv_weather`, `sv_rain` (snow, sleet, rain), `sv_precip`, `sv_wind`, `r_lightning`, `r_snowcover`, `r_wet`, `ce_fog` |
| HELP | the keys (in the classic look, the pack's help pages) |
| QUIT | YES (`quit`), NO |

- **Every row is the console's.** A setting reads its cvar when the menu opens and writes it with
  a console line when it changes, so the console, a config and the menu are one setting; NEW
  GAME's rules are the listen server's cvars, run as `sv <cvar> <value>` by START before its
  `map`. An action is a console line.
- **What the game cannot do yet is dimmed**, shown and skipped by the focus. A row names the cvar
  or command it needs (`kEntries` in `menu/main_menu.cpp`) and lights up by itself the day the
  console has it (`connect`, `volume`, `maxclients` ...). Dimmed today: JOIN A GAME and MAX
  PLAYERS (network play), MUSIC VOLUME and SOUND VOLUME (the window plays no sound yet).
- **CLASSIC ENHANCED is the modern picture's page**: only what a player will want to change, a
  page a subject; every other `ce_`, `r_` and `sv_` setting stays the console's (`cvarlist ce_`).
  A switch the window does not draw yet (`ce_decals`, `ce_reflections`, `ce_flicker` ...) is left
  off the pages until it does. `ce_textures`, `ce_models`, `ce_rebake`, `ce_bloom` and `r_bloom`
  are read when the world is made, so the window makes it again when the menu closes.
- **The settings are kept**: every archived cvar and bind goes to `<game folder>/settings/config.cfg`
  when the menu closes and when the game quits, and is read back at the next start.
- **Two looks** (`menu_look`): with `auto` (the default) the menu is drawn in the pack's own
  pictures whenever the packs on the search path carry them (`gfx/mainmenu.lmp` and the rest,
  coloured by `gfx/palette.lmp`, the glyphs from `gfx.wad`): the plaque, the titles, the strips
  with the spinning cursor, the glyph rows, the help pages and the quit box on the classic
  320 x 200 screen at a whole-number scale. Without them, or with `menu_look modern`, it is the
  engine's menu view over the darkened game. Nothing of the pack is copied or committed.
- **CLASSIC ENHANCED ▸ LOAD GAME** shows the desktop's folder chooser; the folder picked is
  loaded as § Playing it says, and the menu shows its enhancing. **ENHANCE** (dimmed until a game
  is loaded) shows classic-enhance's enhancing page in the menu's place, starting a run of the
  loaded game when none is running (each stage skipped when it is current): the panel stage by
  stage beside CANCEL, PLAY (which closes the menu) and BACK. A run that cannot start says why in
  the panel.
- `togglemenu`, `menu_main`, `menu_options` and `menu_quit` open it from the console or a bind.
  The `se-menu` lab pictures every page in both looks, and the campaign's screens.

## Running your own mod

The mod server (`games/shooters_edge/server`, M18p) runs a multiplayer `progs` as a compiler
wrote it. The tree ships one in source: the **baseline mod** (`games/shooters_edge/qc/`, M18d),
a complete deathmatch (weapons, ammunition, armour, health, deaths and respawns, frags, doors,
plats, buttons and triggers) written in QC for modders to copy and change. Its README is the
modder's guide; `ge qc build games/shooters_edge/qc/arena/progs.src` compiles it to `progs.dat`
with the tree's own QC compiler (`plugins/progs/qc_compile.hpp`, the language in
`porting/shooters-edge/qc-language.md`), and the `se-baseline` lab plays it against bots. The
same sources build a second variant, `qc/campaign/` (M18s7): the classic maps' single-player
game on the single-player program form, run by the campaign host and played on a pack's maps
by `se-campaign --progs=games/shooters_edge/qc/campaign/progs.src`. What both variants share is
`qc/common/`; each variant's `engine.qc` and `glue.qc` hold what its form does differently.

- **Point the game at it** with a path: `--progs=<file>` on the command line, or the `sv_progs`
  cvar in the server's console lines (`sv_progs mods/arena/progs.dat`, or `set sv_progs ...`).
  The path given on the command line wins, then the cvar, then the built-in test mod. Today the
  `se-mod` lab is the front end: `make lab LAB=se-mod ARGS="--progs=<file> --bots=4
  --timelimit=1"` plays a match against bots on the test arena and draws it. The game window
  takes it as `-progs <file>` (§ Playing it); the dedicated server (M18f) takes the same flag.
- **Or ship it in the mod's folder**: with no `-progs`, the window looks through the game's
  files (the mod's folder over the base game's, § Playing it) for the game profile's
  multiplayer program files in order, `qwprogs.dat` and then `progs.dat`
  (`games/shooters_edge/profiles/game_profile.hpp`, looked for by the game's adapter, § The game
  adapter), and runs the first of the multiplayer form; the console's `mods` lists the mod
  folders there are; so `-game mymod` runs `mymod/qwprogs.dat`. A file of the single-player form is passed
  over (the base game's own `progs.dat` is its campaign, found the same way for SINGLE
  PLAYER), and with none the baseline mod runs; the status line says which.
- **Or ship it compiled** (M18p): a mod folder whose packs hold a `qwprogs.qvm` runs that
  module on the dedicated server (`shooters_edge_server --root=<game> --mod=<folder>
  --map=<map>`) ahead of any `qwprogs.dat`, on the module server
  (`server/qvm_server.hpp`, the system calls in `server/qvm_calls.cpp`, the adapter to the loop
  `sim/qvm_program.hpp`). The module starts the map itself (it reads the map's entity text token
  by token and spawns everything), answers a client's commands before the server's overridable
  ones, reads and writes the console, and reads its own config files through the mod's search
  path; its file writes go only to the mod's folder. Classic clients join it unmodified. The
  `se-qvm` lab is the front end: `make lab LAB=se-qvm ARGS="--root=<game> --mod=<folder>
  --map=dm4 --seconds=10"` lists the exports and system calls it made, its entities by class,
  its console lines and any fault, and draws the entities from above. The bot calls answer
  "no bot" for now.
- **Line numbers in faults**: put the line table beside the file as `<file>.lines` (a line
  number per statement, the sidecar `ge qc` reads: `parse_lines` in `plugins/progs/file.hpp`). Without it a fault names the function and the
  statement.
- **What the server offers**: every standard builtin (1–82), and the extensions in
  `server/catalogue.hpp`, each answered by `checkextension` (the maths, `tokenize`, `tracebox`,
  `randomvec`, `registercvar`, `min`/`max`/`bound`, `cvar_string`, the string extensions and
  `strzone`, `calltimeofday`, pausing, visible weapons, the console's `set`, client commands,
  `items2`, the extra move types, `etos`, `findfloat`, `findchain`, `findchainfloat`, `copyentity`, `vectorvectors`, the inverse
  trigonometry and `tan`, `strtolower`/`strtoupper`, `strreplace`/`strireplace`). A builtin the server does not offer faults with its number;
  an unknown map key is skipped with a message (`x is not a field`).
- **When it does not load**: the message says why (a refused record, a missing engine-side
  global or field, a layout of another version) and what to do next. A mod built for the
  single-player game, or for a server with a different field layout, is refused at load (the single-player layout gets its own host in M18s).
- **The free maps** a mod is played on load with `server/bsp_map.hpp` (M18b): a map file read
  into the `MapSpec` (its hulls, its entity lump) and its PVS and PHS answering the server's
  `vis` hook. Until the content's search paths land (M18a) a host hands it the file's bytes; the
  scripted match still runs on the test arena, whose entity text (`kArenaEntities`) has the
  classic deathmatch spawn points.

## C++ mods

Beside the classic game's mods, Shooter's Edge is the first game to offer the engine's own mod
format (M52a, [`docs/mods.md`](mods.md)): a mod written in C++, built by `ge mod build` into a
package, and loaded through `engine/mod`. Its half of the ABI is
`games/shooters_edge/sim/mod_api.hpp` (`SE_MOD_API` 1):

- **The hooks are the game-program seam's**: match start, start frame, a player's pre- and
  post-think, connect, put in server, disconnect. The game's own rules run each first and the
  mod's hook after (`sim/mod_host.hpp`'s composite program), so a mod sees and may change what the
  game just did. A mod's own functions are numbered by `function(k)` and stored in an entity's
  think, touch or blocked; `call_function` runs the game's too.
- **The services** read and change entities by number and fields by `SE_FIELD_*` id, spawn,
  remove, size an entity and start a sound. Every argument is checked; a bad one is refused and
  counted, and the match goes on.
- **Determinism**: the mod's state block, its random stream (seeded from the match,
  `MatchOptions::mod_seed`) and its counts are `ModRun`, copied by a snapshot and folded into
  every frame's hash. A match without a mod keeps every hash it had.
- **The sample** is `games/shooters_edge/mods/rocket_bounce/` (rockets half as fast again, a
  grenade's third bounce lights its fuse), built by the tree; `make lab LAB=mod` plays the match
  with it beside the match without it. `ge mod new my-mod --game=shooters_edge` starts a new one.

## Profiles: the shell and the classic games

Shooter's Edge itself is a **shell**: the window, the menu, the console, the HUDs, the network,
the bots and a small free arena of its own. The fun is in loading the classic games the player
owns into it. Each classic game is a **profile**, and a match is three independent choices,
so the games can be mixed (one game's maps with another's movement and a third's models; the
weapons and armour are a fourth choice, § Weapons and armour):

| Choice | What it picks | Today | Later |
| --- | --- | --- | --- |
| **Content** (the profile) | the packs and game directories on the search path, the map, model and sound formats, the palette and the menus' pictures, the content rules (which episodes the packs open) | the classic game (BSP 29, MDL) | its sequel (BSP 38, its models), the third (BSP 46) |
| **Program** | the rules of the game: a QC program of a known layout, or a C++ mod (`engine/mod`) | the multiplayer layout; the single-player layout (M18s) | the sequel's rules as a C++ mod, written clean-room |
| **Movement** | the player move: `sv_playermove arena`, `original`, or a profile's own | both (M18s) | the sequel's move |

```
 shell (games/shooters_edge)  window · menu · console · HUDs · net · bots · free arena
   │
   ├─ profile  (games/shooters_edge/profiles/<id>)  data: packs · dirs · formats · rules
   │     classic game (29) ─────────── sequel (38) ───────── third (46)
   │
   ├─ host     (plugins/shooter)  the loop; the host's KIND follows the program:
   │     arena host (matches: per-command move, spectators, multicast)
   │     campaign host (spawn parms, serverflags, level changes, skill, saves, autoaim)
   │
   ├─ program  (plugins/progs, engine/mod)  a LAYOUT TABLE per program form
   │     multiplayer QC (54730) · single-player QC (5927) · C++ mod (the sequel's rules)
   │
   └─ move     (plugins/shooter)  sv_playermove: arena · original · (a profile's own)
```

- **A profile is data, not code**: a row naming its game directories (the classic game's base
  directory, the sequel's), the formats it reads (each already a reader in `plugins/brush`,
  `plugins/morph`), its default program and movement, and its **content rule** (the classic
  game: the full game's second pack opens episodes 2–4, `campaign.md`'s `registered`). Adding the
  sequel is adding its row and the readers and program it needs; nothing in the shell or the
  loop branches on a game's name.
- **A program form is a layout table**, not a struct: the loop mirrors its `Entity` into the
  program's memory through the table, so the single-player layout (M18s) and any later one are
  rows of tables, and a C++ program (`engine/mod`) needs none.
- **The host's kind follows the program, not the game**: a campaign program gets the campaign
  host's services (parms carried across maps, the level change with its intermission, skill,
  saves, autoaim), a match program the arena host's. Both run on one loop.
- **The movement is a setting** for any game: a multiplayer match plays with the original move
  when `sv_playermove original` is set, and the campaign with the arena move. The setting is
  sent with the server's settings, so a predicting client predicts with the same rules
  (`porting/shooters-edge/frame-move.md` has both halves and who jumps in each mix). Built:
  `plugins/shooter/framemove.hpp` and `ServerSettings::playermove`; the mod server's
  `sv_playermove` (`auto` follows the program's form), `edgefriction` and `sv_nostep`.

### The game adapter: one set of calls, one implementation per game

The profile rows hold what is only names (folders, packs, file names, extensions). What a game
does with them differs from game to game, so each classic game has an **adapter**
(`games/shooters_edge/client/game_adapter.hpp`): one table of calls the window, the console,
the menus and the labs ask a game's files through, implemented once per game in a file of its
own. The first game's is `client/classic_adapter.cpp`; a later game's is a second file with the
same calls and nothing of the first game's in it. Below the adapters stay the readers a family of
files shares: `engine/vfs` (folders, packs, zips, loose files, a mod over the base) and
`plugins/brush` (every map family into one view).

| Call | The classic game | Its sequel | The third |
| --- | --- | --- | --- |
| **mods** (`list_mods`) | folders over the base with numbered packs, zips or loose files | the same folders and the same pack format | folders of zips, read in name order, with a description file each |
| **programs** (`find_program`) | one QC program for the server (`qwprogs.dat`, then `progs.dat`) | a native library of the server's rules | up to three bytecode programs inside its zips, one a side: the server's rules, the client's half (its HUD and prediction), the menus; or native libraries |
| **maps** (`list_maps`, `map_info`) | BSP 29 or BSP2, the title the world's message | BSP 38 | BSP 46 |
| **textures** (`map_textures`) | the map carries its pictures (`plugins/brush/pictures.hpp`); a PNG or TGA by the texture's name stands in, alpha and all (`textures/<map>/<name>`, then `textures/<name>`) | its own picture files beside the map, by the texture's name | shaders over picture files |
| **models** (`open_model`) | `.mdl` into the shared vertex-animated form (`plugins/morph`), `.spr` sprites, `.bsp` items; a PNG or TGA beside a model stands in for a skin, alpha and all (`progs/player.mdl_0.png`) | its own model files (with `.pcx` skins in a folder a player model) read into the same form | its own model files (a body of head, upper and lower parts, `.skin` files) read into the same form |
| **pictures** (`pictures`) | the graphics archive's lumps and `gfx/*.lmp` in the palette's colours; a PNG or TGA of a picture's name under `gfx/` stands in, alpha and all, at its size or a whole multiple of it | its own picture files (`.pcx`) | its own picture files and shaders |
| **sounds** (`open_sound`) | WAV files under `sound/` | WAV files under `sound/` | WAV files and other formats under `sound/` |
| **config files** (`find_config`) | console text at the top of its packs and its mod folder (`exec mymod.cfg`) | the same | the same, inside its zips |
| **place names** (`open_locations`) | `locs/<map>.loc` (`plugins/shooter/locs.hpp`'s format) | none of its own: named off the map's items | none of its own: named off the map's items |
| **menu** and **modes** (`menu`, `modes`) | the classic pages (`menu::classic_menu()`): single player and multiplayer | its own pages: single player and multiplayer | its own pages: multiplayer only, no campaign |
| **held weapons** (`held_weapons`) | `progs/v_*.mdl`, authored at the eye, the firing frames a `fire` clip | its own weapon models in a folder a weapon | its own weapon models |
| **demos** (`list_demos`, `find_demo`, `open_demo`) | the single-player, client and multiview recordings (`client/demo.hpp`) | its own recording format | its own |

**Every program file names its form and its side** (`ProgramFile` in `profiles/game_profile.hpp`:
QC bytecode, a native game library, a sandboxed bytecode, the engine's C++ mod; server, client
or menus). A form this build has no runner for is named when found and passed over
(`runs_form`), never read as another; today only QC runs. The engine's C++ mods (§ C++ mods) are
hooks over whichever program runs, so they stay a choice of their own.

A stand-in picture's alpha cuts the face where it is under a half, as a fence texture's holes do
(`docs/rendering.md` § See-through texels on a BSP map); the TGA reader is
`engine/assets/tga.hpp`, the PNG reader `engine/assets/image.hpp`. A model's skin picture does
the same on the model: a texel under half opaque is a hole (`kMarkHole`, `docs/rendering.md` §
Morph meshes), and a skin picture carries no tint rows, so it is drawn in its own colours (lab
`se-skins`). Every model comes through `open_model`: the items (armour, health, ammunition), the
weapons lying about, the players and their skins. A later game's model families are new readers
into the same shared forms, so the window that draws them does not change.

The HUD's and the menus' pictures come through `pictures`: a picture file of a picture's name
wins over classic-enhance's upscaled copy, and the set keeps one density (the first picture's
scale; a copy at a whole step under it is scaled up nearest, any other picture is the pack's
scaled up), so a player can swap the status bar or the glyph sheet alone (lab `se-pictures`).

A config file the console's `exec` names is looked for in the player's settings folder, then the
folder the game runs in, then the game's files through `find_config` (`read_configs_from`, the
console's fallback reader), so a mod's own cfg in its pack runs and the player's copy of a name
wins. The startup files (`default.cfg`, `config.cfg`, `autoexec.cfg`) run before the game's
files open, so the pack's own startup files never replace the game's defaults or the player's
binds. A map's place names (the teamplay callouts, the bots' talk) come through
`open_locations` into `ge::shooter::Locs`; with none, the bots name places off the map's items.

**A classic game is played with its own things alone.** With a classic game's folder open, the
window draws that game's own held weapons through `held_weapons` (`client/held.hpp`'s
`load_held_set`), placed at the eye where its files author them (`view::Hand::at_eye`, the
classic row with no offset; lab `se-held`); when its files lack one, no weapon is drawn and the
status line names the missing file. The HUD's pictures are the game's own the same way
(`hud_art_of`: no choice of the free art while a classic game is loaded), and the main menu is
the game's own table of pages (`menu::MenuTable`, the adapter's `menu`). Shooter's Edge's own weapons, HUD art and other assets are
for its own content (its own maps and mod), never a stand-in for a classic game's, so changing
them never changes a classic game. Playing one classic game with another's parts is classic
remix, a feature of classic-enhance (M18u), shown only when two full games are loaded.

### The rules runner: the seam a second program form plugs into (steps 1 to 4 landed)

The adapter answers what a game's FILES hold. What its RULES do is the other half: the listen
server, the dedicated server, the campaign, the bots and the recorder all hold the QC server
(`server/qc_server.hpp`) itself today. A second program form (the sequel's native rules, a
sandboxed bytecode, the engine's C++ mod as the whole game) needs them to hold a **runner**
instead, of which the QC server is the first implementation. Steps 1 to 4 below have landed
(`server/rules_world.hpp`, `server/rules_runner.hpp`). Step 5, the sequel's runner (M18t4), is
under way. Its maps' brushes and the trace its move collides through landed first
(`plugins/brush/trace.hpp`, `docs/levels.md` § The classic BSP families). Then came its player
move (`plugins/shooter/sequel_move.hpp`): packed integer state, a third of the pitch when walking,
the step slide that keeps the farther of two slides, ducking, ladders, currents and the landing
pause, over any world's trace (`SequelBrushWorld` for a map's brushes). A player at rest stands an
eighth above a floor: resting on the plane counts as touching it, which the trace calls solid, so
the packing nudges the origin up. Then the runner itself (`server/sequel_server.hpp`, form
`"sequel"`): a map started from its brushes (`MapSpec::brushes`, the version 38 map read whole),
its entities spawned as data, players placed at deathmatch spots or the start and moved by every
command as it arrives, the rules' frame every 100 ms of host time, `kill` and the respawn by
attack, and the world filled in the shape every runner fills. Items, weapons, damage, movers and
monsters come next; the move becomes `sv_playermove sequel` when the host can pick the runner.

**What holds the QC server today** (counted 2026-11-02, outside `server/`): 31 files, in three
groups. The host and the wire (`client/listen*`, `client/lobby_room*`, `net/classic_*`) start
maps, run frames, connect and drop players and read the model and sound lists, the static
entities, the clients' info, the server info and the message sink. The bots (`bots/`) read each
entity's origin, bounds, view angle and health off the loop's entities (`sv.edicts`, about 60 places)
and a few fields straight out of the VM (`q.vm`, `q.layout`: frags, the weapon frame). The
scoreboard and the recorder (`client/scores.cpp`, `net/classic_record.cpp`) read the frags and
each entity's model, frame, skin, colours and effects through the program's layout.

**The runner is a table of calls, like the adapter**, chosen by the program form `find_program`
found (`runs_form` grows a runner per form):

| Call | What it does | The QC server's answer |
| --- | --- | --- |
| `start` | a map loaded under the settings, its spawn functions run | `load_map` |
| `connect`, `drop` | a player or spectator joins or leaves a slot | `connect`, `disconnect` |
| `frame` | one server frame over the frame's packets, in slot order | `qc_frame` |
| `command` | a client's command or a console line | `commands.hpp` |
| `carry` | the players' state kept across a level change | `save_change_parms` |
| `world` | the shared view below, filled at the end of each frame | built from `sv`, `q.vm` |

**The shared view (`RulesWorld`) is what every reader moves onto.** One struct the runner fills
and the host, the bots, the wire and the recorder only read: the map's name; the model, sound and
visible-weapon lists; the static entities and sounds; the light styles; the server info; each
client's info, frags, stats and slot; and one row an entity (origin, angles, bounds, velocity,
model index, frame, skin, colours, effects, alpha, health, free or not), in the entity's index
order, so a later game's entities map onto the same rows. The rows live in arrays sized once at
`start` and rewritten in place each frame: no allocation and no virtual call on the frame's path,
and the copy is measured with `se-loop` before and after. The message sink stays the wire's: the
runner writes the same messages, so the client and the demos do not change.

**It lands in steps, each with the digests unchanged** (the QC server stays the only runner until
the last):

1. **Landed.** `RulesWorld` and the QC server filling it (`fill_world`): the lists at the map's
   start and after a save's load, the clients and every entity's row at the end of each frame, of
   `connect` and `disconnect`, and of each command or console line (`commands.hpp`), so a reader
   between frames sees what the last call left. Every lab's digest is the same.
2. **Landed.** The wire's entity and player states, the player's stats, the frags, the map's
   title and the recorders read the view; nothing in `net/` reads a program field for them.
3. **Landed.** The bots read the view: each body's place, bounds, view, flags and water from its
   row, a bot's stats and every frag from its client's numbers. They still trace the loop's world
   (`trace_move`), which is the map's collision and no rules'. Every lab's digest is the same.
4. **Landed.** The listen host, the lobby room, the campaign, the wire's endpoint and the
   dedicated server hold a `RulesRunner` (`server/rules_runner.hpp`: the table of calls, the
   runner's state, and `RulesCommon`, the part every runner keeps alike: the loop, the lists, the
   clients' info, the settings, the message sink, the map change asked for and the world). Only
   the code that makes a runner names the QC server (`qc_runner`). The bots take the common part.
   What still reads the program behind a runner goes through `qc_of` in one place each: the
   listen's field and global by name, an entity's class, the campaign's statistics, the saves'
   VM state and the lobby's team field; each is a candidate for the world when a second runner
   needs it. Every lab's digest is the same.
5. The sequel's rules as a second runner (M18t4), with its own program form; nothing in steps 1
   to 4 names it. **Started**: `server/sequel_server.hpp` runs the map, the spawns, the move and
   the frame (golden `tests/golden/shooters_edge_sequel_server.txt`).

**What stays QC-only**: the QC VM's own tools (`ge qc`, the profile, the fault traces), the
extension catalogue and the C++ mods' hooks over a QC program (§ C++ mods), which are hooks on the
QC runner, not on every runner. A runner a later game brings reports its own faults through the
same `out::` lines.

### Weapons and armour: an arsenal of their own

Weapons and armour are a **fourth choice**, beside content, program and movement, and they
can be swapped one at a time. The classic campaign can be played with the sequel's weapons, its
armour, or both, once the sequel's profile lands. The design makes that possible:

- **An arsenal is data.** Each weapon is a row: its pickup and view models, sounds and HUD icon
  by the content's names; its ammunition kind; its fire (hitscan, a projectile and its speed,
  spread, splash); its damage, refire and switch order. Each armour is a row: what it absorbs,
  its amount and its pickup. A profile ships one weapon set and one armour set, so the classic
  game's set and the sequel's set are rows in two profiles. Nothing in our programs names a
  weapon by a constant.
- **Our programs read the arsenal, never their own copies.** (Landed: the rows are
  `games/shooters_edge/profiles/arsenal.hpp`, handed to the program as the server settings
  `arsenal_weapon1..8` and `arsenal_armour1..3`; a slot with no row keeps the program's own.) The weapons and armour in
  `qc/common/` (the arena and campaign variants share them) fire, pick up and absorb through the
  arsenal's rows. The settings `sv_weapons <profile>` and `sv_armour <profile>` pick which set a
  map's items give. They default to the content's own sets, and each can be set without the
  other.
- **A map's items are mapped, not rewritten.** A map names its items by the content's own class
  names. The arsenal maps each name to a slot (the shotgun slot, the green armour slot), and the
  chosen set fills the slot. So the classic game's maps place the sequel's weapons where the
  classic ones stood. An item with no slot in the chosen set falls back to the content's own.
- **What a set needs is loaded with it.** The models and sounds of a borrowed set come from its
  own profile's packs, which must be on the search path. The check at load (M18t5) refuses a mix
  whose arsenal cannot resolve, and names the missing pack.
- **The classic programs keep their own.** A pack's own program builds its weapons in its own
  code, so swapping weapons is a feature of OUR programs (and of a C++ mod that implements the
  arsenal). The exact classic campaign stays exact. Our campaign variant plays the classic maps
  with any arsenal.

### The re-release: words by key

The full game's later release ships a program and maps that print text KEYS (a `$`, then
lower-case letters, digits and underscores: `$map_skill_normal`, `$qc_got_item`) instead of
words, with values passed as further arguments to the print builtins. Its words are a UTF-8 text
file of `key = "text"` rows (`localization/loc_english.txt`) in the release's own archive beside
the game folder (a zip with its own extension), where `{}` or `{N}` mark where argument N goes.
The client reads the file once (`client/files.hpp`'s `localized_text`; a loose copy on the search
path wins) and hands it to the server (`QcServerOptions::localized`), which puts in the words
(`plugins/progs/localized.hpp`) in bprint, sprint, centerprint, dprint and WriteString and in the
level's name, so no client sees a key. The program's own builtins (the prints that take a key and
arguments, the finale's end, a local sound, debug drawing, the player's weapon-switch flags, path
walking, bots' moves) are installed by number in its first form and bound by name in its later one
(`server/catalogue.hpp` § The re-release's program); all but the prints do nothing and return 0,
the classic behaviour each falls back to. Its achievement message (52, one string) is stepped over.

### Single player and multiplayer as variants of our mod

The baseline mod (M18d) becomes two variants built from one source tree, and the classic game's
own programs are the same pair:

| | Arena (multiplayer) | Campaign (single player) |
| --- | --- | --- |
| The classic game's own | the player's multiplayer `progs` | the player's single-player `progs` in its first pack: the exact campaign, episode 1 on the shareware pack, all four with the full game |
| Ours (free) | `qc/arena/progs.src`: today's deathmatch | `qc/campaign/progs.src`: monsters, the level flow, runes, skill, coop on the same common code |
| Shared | `qc/common/`: weapons, items, combat, doors, plats, buttons, triggers | |

Choosing single or multiplayer is choosing a program: the menu's SINGLE PLAYER ▸ NEW GAME loads
the profile's campaign program (the pack's own when present, else ours), MULTIPLAYER its arena
program. Our variants are built by `ge qc build` from their own `progs.src`; a modder copies
either.

### Co-op and its companions

A campaign in coop is the campaign host (`server/campaign.hpp`) with more than one player and
`coop 1`: every player starts at the map's coop starts, is carried from map to map in his own
spawn parms, takes keys the others still find, and comes back on a coop start with a fresh
inventory when he dies, the map going on, on a coop start no one stands on. Teammates are spared:
one player's blows never harm another unless `sv_friendlyfire 1`, and a spared teammate's blow
does not push him either unless `sv_friendlypush 1` (then a rocket at a friend boosts him without
harm); his own blast still harms himself. Monsters choose among the players. On the start map the
episodes open one at a time, alone or in coop: the first always, each next once the one before
is done, and a done one shuts (`sv_episodes_done`, a rune's bit each, recorded by the campaign
host when an episode is left with its rune and archived by the window from game to game; the
host keeps the bars of an episode not yet open shut, `server/campaign_episodes.hpp`).

A COMPANION (`bots/companion.hpp`) is a fighter that plays beside the player rather than for
him, an enabler: it keeps at his shoulder, steps out of his way, kills only what has come for him
or itself (a monster asleep on its spot is left for him to find), holds its fire while he stands
in the line, presses the button he stops by, catches up when left behind (past 440 units), and
never lets a blast off near a player: grenades and rockets only for a foe more than 260 units from
everyone, itself among them, another weapon taken up otherwise. The exit is HIS: the nodes in an
exit's box are closed to the companion's routes and goals and any step into it is turned back, and
when he has stood a moment within 640 units of an exit it goes to stand beside it, faces him and
jumps now and then to show him the way out. While he stands still it fidgets as a person does
(`plugins/shooter/idle.hpp`: spins, looks at an item in sight, glances at him, hops round him,
jumps, sways), stopping a human moment after he moves on. It reads
only what its client is sent, like every bot here, and its personality's traits set its moves and
its patience.

THE WAY THROUGH (`bots/way_through.hpp`) is what a leader bot plays a campaign map by, as a
person does the first time through, off nothing but the map's own entities and his nav graph
(`bots/map_nav.hpp`): the exit when a route leads there past every shut door that only something
else opens; else the nearest button, key or trigger that opens such a door whose opening reaches
places he has not seen; else the nearest place he has not been; and when nothing at all is left
where he stands (a pool let him out on a ledge whose doors open from the other side) he swims
back. A bridge a button slides out (a door that starts open and is a plate) is shut to him until
it is out. He fights what he meets on the way, passing the monsters no weapon he holds keeps down.
`se-coop-episode` plays our campaign in coop on the player's own pack from `e1m1` with a companion
beside him, map after map, and draws each map with the tracks, the exits and where monsters fell.

In the window, `sv_coop 1` makes the campaign a coop game (`client/coop_mode.hpp`): set in a cfg
or on the command line the game starts as one on the start map, set in the console one starts at
once, with `sv_coopbots` companions (1 by default, at most seven: eight players); `skill` is the
monsters' for everyone. `coopgame [bots] [novice|average|expert] [map]` starts one by hand, and
`addbot` and `removebot` change the party during it (`client/coop_party.hpp`). `addbot` in a
single-player game seats a companion too, at any time: the game turns cooperative there and then
(`server/campaign.hpp`'s `campaign_set_coop`: a death respawns at a coop start instead of
restarting the map, and the coop weapon rule holds) and turns back to single player when the last
companion leaves; the game saves again once it has gone. With `sv_port`
set, others join the coop game over the classic protocol as they would a server
(`net/classic_coop.hpp`): the window's player and the companions are the endpoint's local seats, a
client who joins enters the campaign and is carried from map to map with his parms as the player
is (every client signs on again on each next map), and the ninth player is refused as the server
being full.

## Licensing and the clean room

**The engine.** gameengine2 is under the PolyForm Noncommercial License 1.0.0 (`LICENSE`): open to
read, change and share for noncommercial use, with commercial use reserved to its copyright
holder. That is exactly why no GPL code may enter it: the GPL forbids adding restrictions such as
"noncommercial only", so a derived work of GPL code could not carry this licence. A clean-room
rewrite is not a derived work: copyright protects code, not functionality, file formats or
protocols, and reimplementing them for compatibility is lawful.

**The existing implementations.** The original game's source, the community clients and servers
and their mods are released under the GNU GPL. Copying, translating or closely paraphrasing any
of it would make this tree a derived work bound by the GPL, which this tree's licence cannot be. The plan avoids that by building
from specifications, not source:

1. **The specification is written in our own words.** Every format, message and behaviour the
   game needs is described in a working note under `porting/shooters-edge/` before it is built:
   from the public descriptions of the file formats and the protocol, the QC language's
   reference, and measurements of running programs. A note states facts (a lump's offset, a
   message's bytes, a builtin's number and what it returns, a measured acceleration curve);
   it never quotes source.
2. **The builder reads the note, not the source.** An agent writing engine or game code works
   from the note and from the black-box measurements (step 3). It does not open GPL source while
   building that subsystem. Research that needs to read source to answer a question is done by a
   separate reader agent, which writes the answer as facts in the note with no code; the
   building agent reads only the note (the `clean-room` skill holds the procedure).
3. **Behaviour is matched by measurement.** Where the feel must match (the player's movement,
   the timing of a weapon, what a server sends after a given input), a lab runs an existing
   server or client as a separate program, never linked and never committed (fetched and built
   in the lab's own cache), feeds it scripted inputs, and records what comes out: positions,
   velocities, packets. Those recordings are facts about a program's output, and our code is
   held to them by a lab with a tolerance (`se-parity`). Running a GPL program as a tool is not
   distributing it.
4. **Nothing GPL enters the tree.** No source, no patch, no compiled `progs` built from
   GPL QC, no shader or table copied out of one. Tables a format defines (the packed vertex
   normals, the default palette's role) are data the format specifies; where a table's values
   come only from GPL code, the note derives or measures them instead, and says how.

**Mods.** A mod a player loads keeps its own licence, GPL included: the VM executes its bytecode as
data, which makes neither work a derivative of the other. The game never bundles a GPL mod.

**Content.** The classic game's own data is not redistributable. The game never ships or
fetches it; a player points the game at their own copy. Tests and labs run on
free content (§ Content and test data).

## Content and test data

- **Free game data**: a BSD-3-Clause replacement for the classic game's data (maps, models,
  textures, sounds; named in `porting/shooters-edge/survey.md`), fetched at a pinned release into the labs' cache and
  checked by hash, never committed. Its maps and models are the labs' reference scenes.
- **Free QC**: the labs need a multiplayer `progs` that is not GPL. Our own QC compiler
  (`ge qc build`, M18d), written from the language reference, builds our own deathmatch mod
  (`games/shooters_edge/qc/`) in the tree, and the `se-baseline` lab and its test run it. The
  VM's unit tests still run bytecode written by a tiny assembler, so each opcode is pinned
  without a compiler between.
- **Fixtures of our own**: the tests write their own tiny archives, maps (a box, two rooms and a
  door, a ramp, water) and models with writers in the test support code, so a unit test needs
  no download.
- **Our own sounds and models**: `games/shooters_edge/content/` holds the game's registry of every sound and model it needs under our own names (`sounds.tsv`, `models.tsv`), `classic.assetmap` from the classic content's names to ours (read by the map cloner and the game), and free CC0 replacements credited in `CREDITS.md` (M18n, the `free-assets` skill). A player's own data still wins when the player points the game at it.
- **A player's own data**: the client reads the base game directory and any mod from a directory the player
  chooses; nothing in a test depends on it.

## Where the code goes

Decided with the `plugins` skill's questions; dependencies run game → plugin → engine.

| Home | What | Why there |
| --- | --- | --- |
| `engine/vfs` (new) | The virtual file system: archives (a directory, a pack, a zip), search paths in order, file lookup without allocation on the hot path, reading a file whole or as a stream | Any game with mods or downloadable content wants it; Fall Line's assets could live in one |
| `engine/core/inflate` (moved from `engine/lab`) | Deflate decoding for zip archives | The labs' inflater becomes shipped code; it gets a golden and a fuzz pass |
| `engine/audio` | Sampled voices beside the synth's: PCM buffers at their own rates, resampled to 48 kHz, looped, attenuated by distance and panned by the listener; a channel limit with stealing | Any game with recorded sounds; the synth stays the only voice Fall Line uses |
| `engine/platform` | Relative (captured) mouse with raw deltas, every key and mouse button named, the wheel, text input for a console, gamepad | Any first-person game; text input any game with a console |
| `engine/console` (new) | Cvars (typed, flags: archived, server info, user info, cheat, latched), commands, aliases with arguments, binds in modes, `exec` of scripts, the command buffer and `wait`, `$cvar` expansion, conditionals and arithmetic, triggers, `cfg_save`, completion (§ Configs) | A console is generic: Fall Line's developer page can use it, and a mod's config needs it |
| `engine/ui` | Bitmap fonts from a sheet, 2D pictures from palette images, the scaled HUD, a drop-down console's text view | The 2D layer M10 builds, given the pieces a classic HUD draws |
| `engine/render` | A static-geometry pass with lightmap atlases and animated light styles, vertex-animated (morph) meshes with frame interpolation, sprites, a palette-and-colour-map look as a post option, dynamic point lights, scrolling and warped materials, sky layers and boxes, portal-free PVS culling as a visibility input to the existing instance culling | Each is a mechanism; the BSP specifics that feed it are the `brush` plugin's |
| `engine/net` | The replication layer (M17f) used by the native protocol; a channel on the classic protocol needs only the sockets, the simulated network and the capture | The native protocol is the engine's own; the compatible one is the game's |
| `engine/play` | The play contract grown for a first-person game: an angle axis kind on the protocol's grid, a step whose length the input sets (a usercmd's milliseconds), several players a run | Any first-person game an agent plays through `ge` |
| `plugins/brush` (new) | The brush world: BSP29 and BSP2 loading, the PVS and PHS, clip hulls and the trace through them, brush models, the entity lump's parser, the lightmap and texture data with animation sequences; `app/`: feeding the world pass | Any game on this map format (single-player, multiplayer, a mod's own game) |
| `plugins/shooter` (new) | The first-person shooter's movement model (ground and air acceleration, friction, stepping, ducking, water, jumping, the strafe and bunny-hop behaviour players rely on), the classic game loop (the server's frame on the run clock, entities by movetype, thinking, pushers, touches, the area tree, a player's packet and commands) behind a game-program seam any program plugs into, the client's commands and prediction, hitscan and projectile helpers over a trace callback, the view's bob and roll, the bot's aim and movement hands on M16 | Any FPS on the engine, whatever its map format |
| `plugins/progs` (built, M18c, M18d) | The QC VM: the progs loader and checker, the interpreter, entity and global memory, strings, the builtin table a game fills, errors with a QC stack trace, the profiler, the extension registry, the tests' assembler and the `qc` tool `ge qc` hands off to; the QC compiler (M18d) | Any game that runs QC bytecode (a single-player game, a multiplayer one, a mod's standalone game) |
| `games/shooters_edge/` | The rules of a match as a game program on the loop's seam (`sim/`: the native program, the test arena and the scripted match, M18o2; the adapter that lets a loaded progs drive the seam, `qc_program.hpp`, M18c), the server (`server/`: the mod server, M18p: the builtins by number, the extensions, the hooks, the clients, the test mod and a match with bots; the dedicated server, M18f), the client (prediction, interpolation, the HUD, menus, the console's commands), the protocol (native and compatible), demos, its labs, its play contract, the dedicated server executable | What makes Shooter's Edge Shooter's Edge |

**The one exception to C++ only.** QC is executed, never written into the engine: the VM
runs existing mods' bytecode for compatibility, and the engine, the plugins and the game stay
C++. The tree's own QC is limited to the game's baseline mod (`games/shooters_edge/qc/`) and
its test mod.
AGENTS.md and the non-goals in `docs/goals.md` state this exception.

## What the engine has, and what it lacks

| Need | The engine today | Gap, and the row that closes it |
| --- | --- | --- |
| Fixed-step simulation, deterministic | `loop/run_clock` at 120 Hz; `core/libm`; goldens; labs as determinism tests | The server's frame runs on the run clock at its own rate (77 Hz, the classic tick); each player's move runs on its usercmd's milliseconds, as the protocol defines it. QC floats are 32-bit and its math builtins call `core/libm`, so a server replays to the bit from its input log (M18c, M18f) |
| Files and archives | Embedded blobs; the labs' inflater | `engine/vfs`, inflate shipped (M18a) |
| A map format with visibility and collision | A heightfield world (`plugins/mountain`) | `plugins/brush` (reading: M32b; clip hulls, PHS: M18b), `plugins/shooter/bsp_world.hpp` (the hulls as the server's brush models) |
| Scripting for mods | `plugins/progs` (M18c), the stated exception: the VM runs assembled bytecode to the bit and drives the loop's seam through `games/shooters_edge/sim/qc_program.hpp` (`qc` lab) | The builtin catalogue, the map's entity text and the allocate rules (M18p); the compiler (M18d) |
| Player movement and weapons | `engine/move` (M28): the mover for any genre, the classic arena row among its presets, strafe jumping charted (`docs/movement.md`); `plugins/shooter` (M18o1): the classic player move in floats over clip hulls, held to its recordings on every scenario (`pmove` lab); (M18e) the engine's mover and swimmer on the classic numbers (`row.hpp`) over a brush world's hulls (`hull_world.hpp`, its water, slime and lava laid as `engine/water` volumes), held to the same recordings within their own tolerance on 28 of the 35 scenarios (`pmove` lab, `docs/movement.md` § The classic row) | The weapons: `plugins/shooter` (M18e) |
| A server with many clients | `engine/net` links, channels, sessions, NAT, the simulated network, captures; `plugins/shooter` (M18o2): the classic loop (the server's frame on the run clock, entities by movetype, thinking, pushers, touches, the area tree, a player's packet and commands) behind a game-program seam, and `games/shooters_edge/sim`'s native program playing a scripted match that replays to the bit (`se-loop` lab) | The bytecode program on that seam (M18p); the server's clients and bots (M18f); the compatible protocol (M18g); the native one on M17f's replication (M18h) |
| Client prediction and interpolation | Planned in M17f; `plugins/shooter` (M18o2): the client's commands (rate, lengths, key shares, the protocol's grids) and its prediction from the protocol's quantized state, with error smoothing and other players run forward | M17f lands the generic layer; the game's interpolation of other entities and its client over `plugins/shooter` (M18h) |
| Demos | The input log (`play/inputs`), recordings (`ge record`) | Client demos and MVD read and written, seek by snapshot (M18h) |
| Rendering | Dawn, the pass table, instances with Hi-Z culling, shadows, TAA-U, bloom, post, skinning; lavapipe in the container | The static-geometry pass with lightmaps and light styles, morph meshes, sprites, sky layers, warps, the classic look, dynamic lights, PVS culling (M18i) |
| Sound | The synth, the meter, WAV writing | Sampled voices, spatial sound, ambient sounds by leaf (M18j) |
| Input | Keyboard, pointer, focus | Captured mouse with raw deltas, every key, the wheel, binds, text input (M18k) |
| 2D and text | Planned in M10 | Bitmap fonts, palette pictures, the console's view, the HUD and menus (M18k, after M10) |
| A console and settings | Fall Line's settings | `engine/console` (M18k) |
| Bots | `engine/ai`: decision records, hands, paths, sight | A deathmatch bot as a client of the server that plays through the simulation like a player: navigation over the brush world (a graph generated from the map on `ai/path`), aim hands, item timing, its decisions traced (M18r; § Playing together) |
| Hosting and the lobby | `engine/net/session` (M17d), NAT traversal and the rendezvous (M17e, M17j) | HOST and the lobby in the game's menus, a listen or dedicated server started from inside the game, the server browser (M18q; § Playing together) |
| Labs | Sim, scenario, picture, drive and bench labs; digests; `ge lab diff`, `ge graph` | Each row's labs (§ Labs) |

## The network

The two protocols the owner chose: both.

**The compatible protocol** (`games/shooters_edge/net/classic_*`) speaks the classic protocol,
version 28, and the extensions today's servers and clients negotiate, over `engine/net`'s UDP sockets,
simulated network and pcap capture (but not its packet format, channels or handshake, which are
the engine's own):

- The connectionless messages: status queries (what a server browser asks), the challenge, the
  connect with its user info, and the heartbeats a server may send to public master servers
  (off unless the server's owner configures a master).
- The net channel: sequence and acknowledgement with the reliable bit, the client's port number
  for NAT rebinding, the reliable message resent until acknowledged.
- Server to client: the reliable messages (print, sound, the stats, the serverdata, the model
  and sound lists, spawning static entities, lightstyles, the intermission), and every frame the
  player states and the entities as deltas against the last frame the client acknowledged.
- Client to server: the last three usercmds in every packet (angles, the movement axes,
  buttons, impulse, milliseconds), each delta coded against the one before, so a lost packet
  costs no input; string commands on the reliable stream.
- The extensions in a first tier: float coordinates (maps bigger than the 16-bit range),
  transparency, more than 64 visible entities, the larger model and sound lists, and the MVD
  stream's own extension flags. The negotiation (the extension bits in the challenge and the
  connect) is a table, so the next extension is a row.
- Downloads of maps, models, sounds and player skins a client lacks, from the server, in chunks.
- Player skins: each player's user info carries a `skin` name beside the top and bottom
  colours, and the server passes it on to every client with the player's other user info.
  A client draws a player with `skins/<name>.png` from the game directory when there is one (a
  high-resolution skin on the classic skin's layout, its colour rows found by scaling the classic
  marks), else `skins/<name>.pcx` (an 8-bit picture in the palette, the size of the player model's
  skin), recoloured by the top and bottom colours like the base skin. A missing or damaged file
  falls back to the base skin, and the client
  asks the server for a missing skin when downloads are allowed.

**Built so far (M18g, the server half; `docs/goals/M18g.md`)**: the codec, the net channel, the
server endpoint over `QcServer`, lag compensation for hitscan, a scripted client and a measured
session (`classic_trial`):

- **The codec.**
  - The quantization and the CRC are held bit-exact by vectors.
  - The move checksum uses the protocol's key table, kept as a wire constant for interop
    (`classic_checksum_table.cpp`). Every move is verified by default.
  - Every svc and clc message is a table row or a hand-written form, decoded and described for
    the tests and the lab.
- **The first tier.** The extension bits in the challenge are answered either in the connect or
  through `pext`. A base-protocol client gets every base form.
- **The mod's raw bytes.** What a mod writes with the Write builtins reaches a client only when
  it reads as whole messages. A malformed message would cost that client the rest of its packet,
  so it is dropped and counted instead.
- **Downloads.** Skins (and any served file) download during the signon. Under the chunked
  extension they go in 1024-byte chunks: the first inside the packet, the rest out of band.
  Otherwise they use the classic block a `nextdl`.
- **The rewind.** `traceline` and `tracebox` call `QcServer::lag_rewind` around the trace when
  the shooter is a client and `antilag` is on. The other players move to where the shooter saw
  them, then are restored.

`se-net` runs the session under every preset. `se-interop` is a stub until the dedicated
server's classic flag and existing programs are at hand.

**The native protocol** is the engine's own (`docs/networking.md`), used between our client and
our server when both ends are ours: the M17 handshake and AEAD (M17i), the snapshot ring and
deltas with interest (M17f), prediction of the local player and of projectiles, lag
compensation by rewind, voice (M17h), the session layer's lobby and chat (M17d), and NAT
traversal (M17e). Gameplay is identical under both: the server's frame and QC do not know
which protocol a client speaks, and one server serves both kinds of client at once.

**Held by labs on the simulated network**: a compatible client and our server, our client and
a compatible server, under every condition preset; and, in `se-interop`, our client against an
existing server program and an existing client against ours, run as black boxes on loopback
(§ Licensing and the clean room, step 3).

## The picture

Built on the existing frame (`docs/rendering.md`), measured in the container on lavapipe like
every pass, and judged by captioned screenshots:

- **The world** (`plugins/brush/app/bsp_draw.hpp`): one static vertex buffer for the map's
  faces, sorted by kind and drawn a kind a call. **The cull** (`bsp_cull.hpp`, `r_cull`): the
  potentially visible set of the eye's leaf on the CPU (a brush model's faces always in it), then
  on the GPU each face's box against the frustum and the depth pyramid in two phases (last
  frame's pyramid early, this frame's late), compacted in the faces' own order by a scan so a
  frame's pixels never hang on thread order, and drawn with indirect calls. `se-world` holds it
  to the same pixels as drawing everything. **Light styles**: every style a face stores is a
  layer of the lightmap atlas, and the 64 styles' levels are a uniform the frame fills from their
  letter patterns ('a' dark, 'm' as baked, ten letters a second; `set_light_style`), so animated
  light costs no re-upload. Coloured light from a `.lit` file when the game's files hold one
  (`maps/<name>.lit` beside the map, else `lits/<name>.lit`; `client/files.hpp`'s `map_lit`), over
  the grey light the map carries; one made for another build of the map is refused and the map keeps
  its own.
- **Materials**: animated texture sequences ("+0" .. "+9", five frames a second, each texture
  starting at its own frame), warped liquids, the two-layer scrolling sky or the map's own sky
  box (the worldspawn's `sky` names six pictures under `env/` or `gfx/env/`, `rt bk lf ft up dn`,
  as .tga, .png, .jpg or .pcx; `client/files.hpp`'s `sky_box_files`, `brush/app/sky_box.hpp`)
  drawn at infinity on the sky faces, all six or none (`r_skyname` names one for every map),
  fullbright palette colours as emissive.
- **The map's own fog** (`brush/map_fog.hpp`, M18i13): the worldspawn's `fog` (`_fog`, `airfog`:
  density, red, green, blue, then alpha and bias; fewer numbers fill the rest) and `skyfog`; the
  `fog` command takes the same words and holds until the next map (with none, it says what it
  is); `r_skyfog` is the sky's share where the map does not set one (0.5). Drawn in both looks as
  the engine's flat fog (`render/view.hpp`'s `FlatFog`, read through `frame.wgsl`'s `local_fog`
  by every material): a surface z units out is alpha x (1 - e^-(t^2)) fog, t = max(density / 64 x
  z - bias, 0), by its distance from the eye (not its depth along the view, so turning on the
  spot changes nothing); the sky takes its fixed share; a glow added over the scene fades to
  black.
- **Models and sprites**: MDL frames as morph targets in one buffer, interpolated on the GPU;
  the player's colours remapped in the shader; sprites as instanced quads by orientation type.
- **Effects from the game's files** (`client/world_fx.hpp`, M18l): a file `particles/<any>.fx`
  in the game's files (a mod's over its base game's) defines effects as text
  (`engine/particles/effect.hpp`, lengths in map units); one named `explosion`, `gunshot`, `spike`,
  `blood`, `teleport`, `lava`, `rocket_trail` or `grenade_trail` is thrown in place of the classic
  effect of that name (its light as strong as a classic light that far reaching). A file that
  does not read is named on the console with its line and the fix. The `se-timedemo` lab's
  `--particles=sample` plays the match with a mod's own blast and rocket trail.
- **Effects, classic** (`games/shooters_edge/app/effects.hpp`): the classic client's effects in
  its own numbers, drawn as unlit specks of the pack's own palette colours (the engine's shining
  particles, read from `gfx/palette.lmp`) that run along the palette's ramps as they age: an
  explosion's 1024 specks off a 32-unit box at up to 256 units a second (half speeding out on the
  bright ramp, half slowing on the second), a rocket's speck every 3 units on the fire ramp rising
  as it greys and a grenade's from the ramp's greys, 20 dark grey specks a pellet and 10 a nail,
  20 dark red ones a wound, a teleport's grid of 896, lava's 1024; all falling at 40 units a
  second squared. The pack's own models stand in for the rest (`client/world_models.hpp`'s
  `kEffectModels`, loaded by the window whether the program precached them or not): the
  explosion's sprite (its 6 frames at 10 a second) and the lightning bolt's model laid every 30
  units from the player's origin to the end, each segment turned to a fresh roll every frame.
  The classic look also draws each weapon's own flash frames, not the engine's muzzle fire.
- **Effects, enhanced** (`games/shooters_edge/app/weapon_fx.hpp`, the modern look, each behind
  its `ce_` switch: `ce_muzzles`, `ce_explosions`, `ce_trails`, `ce_hits`, `ce_beams`): a
  LAYERED fireball of the engine's (`render/explosion.hpp`: the main ball, then three smaller
  billows born round it a few hundredths of a second later, a dome when it goes off against a
  surface), the classic's ember ball kept as glowing embers that cool from yellow to a dull red,
  white-hot sparks, dark chunks and smoke that lingers, its light white-hot then orange; a rocket
  burns a short flame out of its motor (the muzzle fire's march stretched back along its flight)
  and lays fire into puffs of smoke that spread as they thin (the particles' `grow`), a grenade
  pale smoke; a pellet's or a nail's hit tears debris off the face it hit and makes no smoke (big
  chunks, smaller chips and a spray of dirt in the wall's own colour, flung out of the hole, that
  fall and hop on the floor under the hit before they lie still; how much is the surface's,
  `render/impact.hpp`
  by the surface class of the map triangle the round's line crosses there, its normal off that
  triangle); the arc gun's beam is the engine's bolt (`render/beam.hpp`) in ONE look every player sees
  (no style to pick, so nobody plays with a worse one): as thin as the classic's bolt, a hair-thin
  white-hot core in a narrow blue sheath, its brightness keyed to the eye so it reads the same by
  day and in a dark room at night, rooted on the gun's own muzzle, and the brightest light in the
  room along it and where it lands (`bolt_lights`, boosted over `r_fx_lights`' share). While it fires, the same bolt crackles
  across the gun's two prongs (`prong_arc_look`, between the ends `find_arc` reads off the pack's
  own arc), in place of the file's flash. Under `ce_decals` the weapons leave
  their MARKS (`client/world_fx.hpp`): engine/detail's decals pressed onto the map's own faces, a
  scorch where a blast went off against a surface (its embers glowing for a second and a half), a
  bullet hole where each pellet hit a face (a classic program's one message for a shot spread over
  the shot's cone), the bolt's burns where it lands, and WOUNDS on a body: a hole with blood
  splattered round it, pinned to the triangle of the figure's skin the round's line crossed, riding
  its frames until it comes back to life; a pool of 128, drawn as
  the beams' pass's burns (soot and ember). The classic drew none, so its look shows none. Smoke is lit as the room round it is
  (`render::BlastLight` off the lightmap under it and the open sky over it), not by the sun
  through the roof. A match's temporary entities (the gunshots, spikes, beams and blood the
  baseline mod multicasts) reach these effects as a campaign's do. The `se-weapon-fx` lab fires
  every weapon at a wall in both looks, from the eye and from beside the line of fire; with
  `--game=<install>/id1 --map=e1m1` the pack's own maps and weapons, for private pictures.
- **Dynamic
  lights** (`set_dynamic_lights`): the classic adds them to the lightmaps' light, falling off in
  a straight line to their radius; the modern draws them as point lights. **The screen's blend**
  (`render/post.hpp`'s `set_post_blend`): the liquid round the eye, the powers held and a hurt's
  fading red folded into one colour and share the post pass mixes in after its grade.
- **Two looks, one renderer** (`games/shooters_edge/app/look.hpp`): the classic look (crisp
  texels, the lightmaps and their styles, the map's own sky, no bloom, and the post pass's grade
  pulling every colour to the nearest of the palette's 256) and the modern look (filtered,
  real-time lights with shadows, our sky and liquids, bloom). In the window
  (`client/world_extras.hpp`) the modern look also draws the water, the lava and the slime as
  real liquids, the torches' burning gas and the weather through the open sky, and the map's
  textures come from classic-enhance's cache when the pack was enhanced, each behind its `ce_`
  switch; `make lab LAB=se-client ARGS="--game=<folder> --map=<name>"` draws a pack's map in
  both looks, for private pictures only. The `se-world` lab pictures every named view in both. The player sets the picture with
  cvars: `r_look` (classic or modern), and for the modern look `r_fx_lights` (the light the
  effects throw, default 0.2 of the classic's), `r_fx_glow` (a spark's glow, default 0.15) and
  `r_bloom` (default 0.3), `r_lava_glow` (the lava's light on the walls round it, default 0.2)
  and `r_slime_glow` (the slime's faint green light, default 1); the modern's exposure and bloom
  wash a room out at the classic's strengths, so its defaults turn them down. The classic keeps
  its own, with the pools' light baked in its lightmaps. The lab takes the same cvars:
  `make lab LAB=se-world ARGS='--cvars="r_lava_glow 0"'`.
- **Headless**: a CPU raster view of any map from above (the leaves, the PVS of a point, the
  hulls) for the labs, like Fall Line's map views, with no GPU at all.

## Playing together

The owner asked for three things beyond a server and a client: a lobby on the network, servers
hosted from inside the game, and bots that use the simulation code, so improving the simulation
improves them.

**The server (M18f).** One host (`games/shooters_edge/server/host`) runs a server on the
classic loop: its slots are empty, a remote client's (its packets handed to the frame in arrival
order) or a bot's, filled by a command source (`plugins/shooter/cmd_source.hpp`) that reads the
server and returns a usercmd; the host puts it on the protocol's grid and packs it into a packet
like a client's, so a bot's commands run exactly as a person's. Every frame is logged (the slot
events, the packets, the hash after it), and the log replays the run to the bit with no bot at
all, naming the first frame that leaves it. The host on the engine's server core
(`server/dedicated.hpp`), its slots filled by a seeded stand-in that wanders the arena and fires
at whoever is ahead, is what the `se-server` lab runs and replays. The headless
`shooters_edge_server` binary is the engine's dedicated process (`engine/server`, docs/servers.md)
over the PLAYABLE server (`net/classic_game.hpp`): the baseline mod's deathmatch served over the
classic protocol on the game's port, so a classic client connects and plays; it reads
`server.cfg` in layers and its flags (the mod's rules among them: `+set fraglimit 20`, or a line
in `server.cfg`; a map from the game folder under `--root`, loose or in a pack; at a match's end
the next map, `sv_maprotation`'s when set, every client signing on to it again), prints a status line every few seconds and the digest at
the end, and `--realtime` paces it to the wall clock. Its bots come with the engine's lobby
(M53c). The `se-dedicated` lab has classic clients join it under each network preset.

**Hosting from inside the game (M18q).** The menu's HOST page starts a server with its settings
on one page: the map and the rotation, the mode, the mod, the frag and time limits, the player
cap, the bots and their skill, a password, and public or LAN only. Two ways to run it, picked on
the page:

- A listen server: the server's frame runs in the player's own process beside the client, on
  the same run clock, and the host plays over a loopback link like any other client (no special
  case in the game program). The match ends when the host leaves.
- A dedicated server: the game spawns the same `shooters_edge_server` binary M18f builds as its
  own process, hands it the settings as a cfg, and joins it. It keeps running after the window
  that started it closes; the menu finds it again by its local address and can stop it.

Either way the server registers with the LAN discovery and, when public, with the rendezvous
(M17e, M17j), and answers the classic status queries, so one server browser lists ours and
existing ones.

**The warmup (M18q1).** Every match map opens in the lobby's warmup (`server/lobby.hpp`): the
players and bots are on the map and play it freely, but nothing counts: the frag and time limits
are held off, and when the match starts the map is restarted with every seat on it, so scores,
items and the clock begin new. Each seat is on the red or the blue team (a new one joins the
smaller; `team red|blue|auto|switch`), and says `ready` or takes it back with `break`; once every
seated player is ready (and at least `sv_minplayers` are seated) a countdown of `sv_countdown`
seconds runs, stopped by a break or a player joining unready; the leader (the listen server's own
player) may `start` it whoever is ready. When the match's map changes, everyone is back in warmup,
unready. The lobby is a screen of its own, full screen over the main menu's night
(`hud/lobby_page.hpp`), never over the game: both teams, each player's place (IN WARMUP or IN
LOBBY), his frags so far and READY, and the keys: F4 join the warmup, F1 ready or break, F2 the
other team, F5 and F6 a bot more or less (`addbot`, `removebot`), ENTER the leader's start. The
player opens on it; F4 (`warmup`) puts him on the map in the warmup, where the others fight and
frags are scored, and F4 again (`sitout`) takes him back. On the map only a banner is left (the
phase, how many are ready, F1 and F4) and the countdown's seconds. The match needs at least
`sv_minplayers` seated (2; a bot counts), and starting it restarts the map, which resets every
frag the warmup scored and puts every seat on the map. `sv_lobby 0` starts the match with the
map, as before. On the dedicated server `sv_countdown` sets the countdown's seconds (5) and
`sv_intermission` keeps the scores up that many seconds after the mod ends a match, before the
next map (0, none: the mod's own end alone). When the player's packs carry the classic menu's pictures, the lobby is drawn in
them (`menu/lobby_screen.hpp`): the screen and the banner in the menu's text boxes, the words in
the glyph sheet's two sets, the countdown in the big figures, as the ESC menu is; without them it
keeps the modern look.

**Hosting from the window (M53a).** The window's match is the dedicated server's game run in the
window's process (`client/listen.hpp`, `net/classic_game.hpp`): the same server core, the same
lobby and bots, the player a seat of it. With `sv_port <port>` set before `map <name>`, others join
it over UDP with a classic client (`connect <address>:<port>`) as they would `shooters_edge_server`;
`sv_maxclients` is its slots (32). `sv_port 0`, the default, keeps the match the window's alone.

**Who is on, and the easy jump for one player (M53f).** `users` lists every player on the
server by user id (`userid frags name`); any client may ask it, so a player can tell the
administrator his id. `status` is the administrator's table: slot, id, name, frags, ping, loss,
minutes on, address (`bot`, `local` for the window's player) and the easy jump (`-`, `asks`, the
share `pm_speedjump` allows, or `<share> granted`). `speedjump <id|name> on|off|<share 0..1>`
grants the easy jump to one player (a player whose disability makes strafing hard) while
`pm_speedjump` stays as it is for everyone else, `speedjump` alone lists who has it. The grant is
the server-only key `*speedjump <share>` in that player's user info: every client sees it and
everyone is told, a client cannot set it (one sent in a connect is removed), it stays across map
changes and goes when the player leaves; the player still turns it on with `cl_speedjump 1`. In
the window these are console commands for the match it hosts (`sv <line>` too); on a dedicated
server set `rcon_password` and send `rcon <password> <line>` from a joined window (`rcon secret
speedjump 3 on`) or any classic client. Without a password the remote console is refused. The
words and the remote console are `net/classic_admin.hpp`.

**The rocket jump helpers (M18e).** `+rjump` does a rocket jump for the player: one press asks
the server, and on the first command the rocket launcher can fire the server fires a real rocket
80 degrees down at his own yaw as he jumps (the launcher chosen for it; a press inside the refire,
or while the sequel raises the launcher, waits up to a second with his jump held back). `+rrjump`
is the reverse rocket jump: the shot turned half round at his own pitch (look down to throw
yourself forward). Nothing is added: the rocket, its ammo, the blast's damage and push are a
hand-made rocket jump's, and his screen never turns. An aid for a player whose hands cannot turn,
jump and fire at once. The request is impulse 164 (156 the reverse) sent with the jump, the
numbers a classic match mod uses for its own helpers, so the buttons work on such servers too.
The server decides: `sv_rjump` and `sv_rrjump` (0 or 1, in the server info, 0 on a dedicated
server, on in the window's own match) are rules a vote may change for the map; a player granted
the helpers (the server-only user info key `*rjump 1`, visible to all) may use both anyway. A
refused press is told in a game's words ("The rocket jump helper is off on this server.", "You
have no rocket launcher.", "You have no rockets."). One code for both games' rules
(`server/rjump_rules.hpp` over `plugins/shooter/rjump_assist.hpp`); a compiled module answers its
own impulses.

**The lobby (M18q).** Before the match, on M17d's sessions: the players and their pings, teams,
ready, chat to all or the team, a map vote, kick, and the host's start; at the end of the match
everyone returns to it. Joining is from the server browser, an invite (M17j's friends), or an
address, with NATs crossed by hole punching or the relay (M17e). A client speaking the
compatible protocol skips the lobby and joins the match, as it would on an existing server.

**The server browser (M18q).** MULTIPLAYER ▸ JOIN A GAME (`menu_servers`) lists the servers on the
LAN (the status query broadcast to ports 27500 to 27505, `sb_lan`), at the masters (`sb_masters`,
and the masters the loaded install's own `<folder>/sb/sources.txt` lists) and the player's own
(`sb_servers`): any server answering the classic status query, ours or another program's. Each row
is the name (`*` when it wants a password), the mode (the server's `mode` key, else coop, team,
ffa or single from its `coop`, `deathmatch` and `teamplay`), the map, players over slots, the ping
and the skill when the server says it; the foot holds the address, the game folder, what the match
is doing and the players by name and team. UP and DOWN choose, LEFT and RIGHT sort, R or F5
refreshes, ENTER runs `connect <address>`. Our server's info carries what a browser lists: `map`,
`deathmatch`, `teamplay` and `coop` when set, `skill` where monsters play it, `needpass` when a
password is set. `connect` takes a host name as well as an address.

**Demos from anywhere (M18k2).** DEMOS on the main menu's root (`menu_demos`) browses the
player's own folders (the game's folder first, then home, Downloads, Desktop, Documents and the
machine's roots; it opens on the folder the last demo was played from, `menu_demo_dir`) through
the engine's folder browser (`engine/vfs/browse.hpp`). It lists folders and the game's
recordings (every format the window plays, the voice and packed formats too), also packed the way they are
downloaded (`.gz`, `.zip`, `.7z`, `.xz`, `.zst`, `.tar.gz`): a packed file is listed only when a
recording's extension stands before its packing, so `match.mvd.gz` is and `maps.zip` is not.
ENTER checks the file before anything starts (`client/demo_check.hpp`: the bytes, not the name;
unpacked, then read to its level's map) and plays it as `playdemo` does, or shows in the footer
why it is no demo. BACKSPACE goes up a folder, LEFT and RIGHT through the places, R lists again.

**A classic game in its own look, picked from SELECT GAME (M18k2).** SELECT GAME is the main
menu's first row, so ENTER on a fresh start goes straight to it (`menu_games`;
`menu/games_page.hpp`). It shows each classic game the content profiles know by a picture of its
name (`menu/art/<id>.png`, embedded; the names are pictures, never words in the tree), the
install of it loaded last (`client/installs.hpp`: `<prefs>/installs.txt`, a line a game) and that
install's edition, read from its packs the moment it is picked (the profile's `full_game` file
found or not: Shareware or Registered, the sequel's Demo or Full game). ENTER on a game with a
kept install opens it; on one never found, or FIND ANOTHER INSTALL, the window's folder chooser,
and the folder is taken by whichever game's base folder it holds (`install_of`). Once a classic
game is open, the title's night gives way to its own recordings played behind the menu in turn
(`menu_attract`: auto, the game's own list, demo1 to demo3 for the classic game; "" keeps the night), ESCAPE closes the menu onto the
recording to watch it, and nothing modern is left on screen: the console is written in the
game's glyph sheet over its own console background (`menu/classic_console.hpp`, the engine's
`ui::sheet_font`), the HUD's words in the same sheet (`hud::hud_style(art)`), and DEMOS, JOIN A
GAME and SELECT GAME are drawn in the game's letters under carved titles (`menu/classic_pages.hpp`;
SELECT GAME's own row cut letter by letter from the strips like DEMOS'). Lab `se-attract` draws
the menu, SELECT GAME, the watched recording, the console, DEMOS and JOIN A GAME over a played
recording (`--basedir` a player's install).

**Bots on the simulation (M18r).** A bot is a client of the server like any player. Its only
output is a usercmd a frame (the look angles, the movement axes, the buttons, the impulse),
which goes through the same move and the same game program as a person's, so a bot can do
nothing a player cannot, and any change to the movement or the game program is a change the
bots play under at once. A bot never reads the client's prediction or edits the world. What it
knows, it learns through the server's own queries: the hull trace for sight, the sounds the
program starts for hearing, and the items and players the program exposes, the same facts a
player's client receives. Its parts are the engine's (`engine/ai`): navigation generated from
the map on `ai/path` (where a player can walk, jump, fall, swim and ride a plat), sight and
memory, aim and movement on `ai/hands` with skill profiles, the fights and items chosen on
`ai/choose`, and the strafe-jump and the rocket jump as moves it plans. Every decision is
recorded for `ge why`, and its play is measured against people's input logs with `ai/human`,
so a change to a bot is shown before and after: frags, deaths, items held and humanness.

**Bots as a team (M18r7).** With `teamplay 1` the bots of one team share a plan
(`games/shooters_edge/bots/team.hpp`), redone every half second, built on how a team game is
won: equipment and survival before frags, the best armour's area held, the quad timed and taken
before the other team, the quad carried to where the enemy was last called out, items left to
the teammate who needs them, backpacks taken to stack ammunition, and no shot fired across a
teammate. Each player's part is a role (hold, rush, carry or free), and the rest of his choices
stay his own fighter's. `run_team_match` (`net/classic_team.hpp`) plays eight of them as clients
of our server and records the match as a multiview demo; the `se-team` lab runs it and writes
the demo with `--save`, which the game plays with `-playdemo` and turns into a video with
`-demovideo`.

**What a bot knows, and what a team says (M18r8).** A bot knows the map by its skill
(`bots/knowledge.hpp`): the expert knows every item and times the key ones he sees taken, the
average player knows every item but times only the small ones, and the novice learns the map by
sight over the match. A team talks over `say_team` ("quad in 10, get quad!", "have quad!",
"enemy at RA", "need help", "took RA"); the server sends a team's words to its own players and
the multiview recording keeps them, so a demo shows the team talk in each player's view.

**A mind of his own, team messages and place names (M18r9).** No two bots share anything: each
reads the team lines his own client is sent, a person's as well as a bot's, and plans his own role
from what he saw and read (`bots/mind.hpp`). The words are a team game's usual message keys
(`bots/talk.hpp`): a status report (`ra:150 h:100 rl [quad]`), `took ra`, `safe [ra]`, `lost
[ra]`, `help [mh]`, `coming quad`, a point (`rl at [rl]`: "you take it"), `enemy at [quad-low]`,
`have quad`, `get quad`, `quad over` and timers (`quad in 20`, `ra in 10`), read back in whatever
order and spelling a person types them, colour codes and braces included. An expert reads every
line and times the key items exactly; an average player misses half the lines said once but
reads a repeated one, and his timers are a few seconds out; a novice reads one line in six, only a
line said three times with any care, and nothing in a fight. At the start each expert or average
player takes on the nearest of the launcher, the best armour and the quad that no teammate called
`coming` for, and calls it `safe` once he stands on it; the quad is called by whoever timed it,
again every few seconds while no teammate is believed near it. A player gives an item up to a
teammate near it when he has no use for it (armour no better than his, a weapon he holds, rockets
he is full on), and the best armour, the launcher and the lightning gun to a teammate with the
quad, pointing at it with his status; the quad's carrier sweeps the key areas.

**Ears, a map of the team, and an area kept (M18r10).** A bot hears what his client is sent, by
his skill (`plugins/shooter/hearing.hpp`, the game's samples in `bots/ears.hpp`): an expert takes in
every sound, knows every sample and judges the distance to a tenth; an average player takes in a
close or unique sound (a pickup, a power, a spawn) most of the time and a far shot seldom; a novice
hears only what is close and never what a sample means. A pickup heard is put on the item of that
pickup nearest where it seemed to come from, so the listener knows which armour went, times it as
if he had seen it, and knows where its taker is; a spawn (the baseline mod plays the fog's sound
where a deathmatch player comes in) is put on the spawn point. An enemy heard is chased for a
moment like one lost from sight. The mega health is timed only by an expert who took it, from the
moment his own health is back down to 100. An expert keeps where each teammate last was for ten
seconds; knowing where all of them are, he knows where they are not, and a key area none of them
is in is likely lost, to be taken back by a strong free player (an average player goes by a
teammate's `lost` line). An expert holding an area whose item is away and not due for a while,
with 20 or more rockets, steps out to fire at the teleporters' exits and the powers' spawns near it
(`bots/spam.hpp`). Each map's key items are rated by how scarce they are, by an expert only (a lone
200 armour, the 150 where there is no 200, the grenades where there is no launcher). A map's
buttons (`bots/buttons.hpp`) come off its own entities: a `func_button` with health over 0 is shot,
the rest touched, and the items within reach of the doors it opens are gone for through it by an
average or expert player; a novice now and then shoots at, or walks into, any button he sees.

**Temperaments (M18r11).** Beside his skill, a bot has a temperament (`plugins/shooter/temper.hpp`)
that decides which fights he takes, sizing each up as might against might: his body (health and
what his armour saves) times his arms (the weapon's factor, three for the quad), against what he
believes of the enemy's (`bots/temper.hpp`). That belief is built from what he hears: the weapon an
enemy fires, the armour, the mega health, a weapon or the quad he takes, his pain and his death;
it fades back to a fresh spawn's in 30 seconds, and a novice reads none of it. An AGGRESSIVE bot
takes every fight, goes after every sound as a novice (after arming himself as a better player),
stays in a fight down to 25 of body, never holds an area for his team and never gives an item up. A
BALANCED bot takes a fight his skill wins (an expert's edge is twice an average player's, so a fresh
enemy falls to his scattergun; a novice's is 0.7) and turns at 50. A COWARD fights only at two and
a half times the enemy's might, his edge counted (the big armour, the mega and the launcher), never
goes after a sound and turns at 70. A bot outmatched by an enemy he sees turns for any item he can
reach away from that enemy, or for his nearest teammate. An enemy in sight within 192 units is fought whatever the
odds: turning away only shows him a back. The labs take `--tempers` (`a`, `b`, `c`).

**Personalities (M18r11b).** A bot is a set of traits (`plugins/shooter/traits.hpp`), each from 1
to 10: its aim with each kind of weapon (the scattergun, the spikers, the grenades, the rockets, the
arc), its reaction, its timing of items, its knowledge of the map, camping, discipline, moves,
hearing, communication, teamplay, aggression and the dodge. Novice, average, expert and perfect are presets
(every trait 2, 5, 8 or 10), and the temperament sets aggression (9, 5 or 2) and an aggressive
player's teamplay at most 3, so the presets play exactly as the skills did. A trait read in steps
reads 1 to 3 as a novice, 4 to 6 as average and 7 and up as an expert: knowledge (where items and
buttons are and what they are worth), timing (which items are timed, from sight, sound and a
mate's line), communication (what is said and read, the team's map), teamplay (areas held, help,
the quad's rush, items left to a mate; 3 and below plays for himself), hearing (what a sound
means), camping (rockets at the spots an enemy comes in), moves (the hop and its hands),
discipline (3 and below runs at every sound) and aggression (the temperament). The aim and the
reaction are read smoothly, the hands taken up anew with each kind of weapon. The labs take
`--traits` (each player's changes over his preset, `aim_rl=9/timing=3`) and print every bot's
traits.

**Reactions (M18r11c).** A reaction is drawn for each sighting (`plugins/shooter/aim.hpp`
`draw_reaction`): another enemy taken up, or the same one back after 0.3 s out of sight. Half of
the draws lie evenly between the low and the median, the rest thin out to the high, since people's
reaction times are skewed to the slow side: a reaction trait of 8 draws 140 to 220 ms round 160, a
5 180 to 300 round 230, a 2 240 to 420 round 300 (10: 120 to 190 round 140). The view holds where it
was until the hands' own tracking lag (120 ms at 8, 150 at 5, 200 at 2) makes up the rest, and the
fighter does not fire before the reaction is over (`bot_reacting`). A sighting the view was already
on, within 5 degrees (pre-aimed at a doorway), takes half the draw: the only way under 100 ms. A
sound takes longer to act on than a sight: a chase after an enemy only heard starts the median
reaction plus 100 ms (a quick player, trait 8) to 200 ms (a slow one, trait 2) after the sound.
`se-team` prints each player's sightings, their mean reaction and the share pre-aimed.

**The dodge and the watch.** In a fight a bot does not run its route under the enemy's crosshair:
it steps across the line to him, left and right, turning back at drawn times (an expert's dodge
trait 8 every 0.2 to 0.6 s, seldom running straight; a 2 every 0.45 to 1.35 s, half the key, half
the time straight at him), and never to a side the player's box would leave the floor, meet a wall
or stand in a liquid on (`plugins/shooter/dodge.hpp`). Not on a jump the route proved, a lift, at a
door or in the water. Every bot's hit rate fell from 30 % to 27 % on e1m2 with it. Standing (on an
item, waiting for one), or holding its area, a bot with nobody in sight watches the ways in
(`bots/watch.hpp`): for each of the area's entries in the map plan, the first node of the way in it
sees, one after another for 1.5 to 4 s, the one nearest a sound first; the view goes there on the
aim's hands (`Bot::look`), so a man coming in is pre-aimed at. `se-team` prints the share of the
fight spent dodging, the dodge's turns a minute, the share of the match spent watching and each
weapon's share of the match.

**Hit rates (M18r11d).** A team game counts every player's shots and hits by weapon
(`games/shooters_edge/net/classic_hits.hpp`), the way a match's statistics do, and `se-team`
prints them per player and per skill (`team.hits.<skill>.<weapon>`). A shot is ammunition spent on
the weapon held (the scatterguns count pellets, 6 and 14 a shot; the arc counts cells; nothing in
a frame its player is dead in or comes back from, when the mod empties his ammunition); a hit is
health and armour an enemy lost to whatever last hurt him (`dmg_inflictor`: a player's hitscan
by the weapon he holds, or a missile counted once however many it hurt). A rocket's direct hit is
a blow of exactly the mod's direct damage, or one that killed a player the rocket was next to;
any other rocket that hurt an enemy hit by its blast. The bands the bots are held to, a division's
players in percent (Div1 / Div2 / Div3): the arc 38–45 / 28–35 / 15–25, a rocket's direct hit
30–35 / 22–28 / 10–18 and its blast 18–25 / 15–20 / 12–15, the double scattergun 35–45 / 25–35 /
15–25, grenades 8–15 / 5–10 / 2–5. A **weapon drill** (`se-team --drill=arc`, `TeamMatchOptions::drill`)
cuts every player's weapons to one, its ammunition topped up, so a match measures that weapon
alone: on a map whose every item a route reaches (dm6), two seeds of six minutes give each skill a
few hundred shots or more.

The trigger is a finger, not a filter: a fighter fires while his view is within the target's width
and a slack his weapon's aim trait sets (10° at trait 2, 7° at 5, 3° at 8, 2° at 10; the arc's beam
held down longer, 14°, 9°, 6° and 4°), measured on the view the command carries, which a swim or a
button may have turned. The double is fired within 350 units and chosen under 250, the launcher
within 1200, a rocket at a man within 400 aimed at his middle and at his feet farther off.

**Bots on an existing server (M18r14).** The bots are tested where it matters: a team of them
joins a live classic server running the common match mod and plays a 4on4 against people, and
every match leaves data that is read here, learnt from, and played again. The server is an
existing one, built beside the checkout and never in the tree; the bots are ours.

- **One process, one team.** `shooters_edge_bots --server=<host:port>` runs a whole team: each
  bot is its own classic client (`net/classic_client.hpp`) on its own UDP socket, one move a tick
  at the server's rate, paced by the wall clock as the window's remote client is. The map is
  loaded from the player's own packs by the name serverdata carries, and its nav built as
  `run_team_match` builds it. Two processes can play the two teams of a bots-only match; each
  knows nothing of the other but what its clients are sent.
- **A bot knows only its own client.** On our server a fighter reads the server's world; on
  someone else's there is none. So the fighter's world is rebuilt from what its client was sent
  (`net/classic_mirror.hpp`): its own player and stats, the other players' places and dead flags,
  the entities of the newest frame, the frame's sounds, the frags and the prints. Nothing else
  reaches it, and a fact a client is never sent (another player's health) has no field to read.
  Where the server culls by the map's visibility, an item that stops being sent counts as taken
  only when its spot is in the client's own visibility set. What a bot knows of its team is what
  its teammates said on `say_team`, as before (M18r9): four bots in one process share no memory.
- **The match mod's ways.** The team and colours it stuffs are taken, `ready` and `break` are
  said on the owner's word or once every person is ready, the countdown and the match's end are
  read off its prints, and a map change is followed by the reconnect and a new nav.
- **The bot client in the window.** `shooters_edge --bots=4 --server=<host:port> --team=<name>`
  runs the same team in the game's window, which draws the team's world as its bots believe it
  (`net/classic_team_view.hpp`): the map from above, each bot's route, goal and the enemies it
  believes in, the item timers, the enemies the four clients were sent together (which no single
  bot knows), each bot's plan and the team's lines. The console drives the bots: a line no command
  names goes to every bot, `bot<N> <line>` to one (`bot1 skill expert`, `ready`, `status`); the
  picture records as a video with `--bots-view=<file>`.
- **The narrator.** The process describes its own team's game, read off its bots' beliefs and the
  public scores and never written back into them: the score and its trend; each area by its
  place name (held, lost, contested, last seen when) and the key items' believed timers; each
  member's role, goal, health, armour, weapons, mode and last words. It prints every few seconds
  and on `status`, and keeps a timeline. It knows no more than the team does; what the team
  believes wrongly, it reports wrongly, and that is the point.
- **The match folder.** Each bot's client recording (every packet read and every command sent),
  its decisions, the team's lines and the narrator's timeline. A bot is a function of the packets
  it was sent and its seed, so a fresh fighter fed its recording gives back its commands bit for
  bit: `ge why` answers a live match's decisions here, and a changed bot run over the same
  recording shows where it would have decided otherwise.
- **The match's video.** `shooters_edge -demovideo <mvd> <file> --autotrack` follows the hottest
  player (`client/demo_autotrack.hpp`): his kit (armour, the best weapon, powerups, frags) and his
  kill streaks, cut to a challenger that beats him by a margin after a dwell, to the hottest alive
  two seconds after he dies, and at once to one who takes the quad or the shield or kills twice
  quickly. The plan reads the whole demo first, so a cut lands before the fight.
- **Read against the truth.** The server's own multiview recording is the truth. The `se-live`
  lab sets each bot's beliefs against it (where each enemy was against where he was believed,
  each key item's timer against its respawn, the team lines read and missed), and draws the kill
  map by area, item control over time, and hit rates per weapon and humanness for the bots and the
  people of the same match side by side; given a folder of matches, it sets them side by side so a
  change to a bot is read against the same people.

**Place names (`.loc` files).** `plugins/shooter/locs.hpp` reads a map's place names in the
community's `.loc` format (one point a line, `x y z name`, coordinates in eighths of a unit, the
`$loc_name_<key>` macros expanded) and names any point by its nearest. The files themselves are
the community's and carry no licence this repository can take, so none is committed: a game folder
with `locs/<map>.loc` in it is read (the `se-team` lab's `--locs` names a file), and a map without
one is named off its own items (`ra`, `rl`, `quad`, `mh`).

## Modern features

Today's community clients and servers added much since the original release. Grouped by what a
player of today expects, then what makes it better, then what can wait. The research behind the
list is in `porting/shooters-edge/survey.md`.

| Tier | Feature | Row |
| --- | --- | --- |
| Expected | Rendering at the display's rate independent of the network and physics rate | M18h |
| Expected | Smooth interpolation of other players and projectiles; prediction of the local player | M18h |
| Expected | A configurable HUD built from elements (each a position, a size, a style), a scoreboard, frags and teamplay readouts | M18k |
| Expected | MVD demos recorded by the server, demo playback with seeking, rewinding and speed control, a demo browser | M18h |
| Expected | Spectating and following a player; a server-side record of every match | M18h |
| Expected | Raw mouse input sampled at its own rate, independent of the frame | M18k |
| Expected | Lag compensation on the server for hitscan weapons (rewinding the other players) | M18g |
| Expected | Larger maps and limits (BSP2, float coordinates, more entities, more models and sounds) | M18b, M18g |
| Expected | A server browser over LAN discovery, our rendezvous, status queries and the public masters | M18q |
| Expected | Hosting a listen or a dedicated server from the menu; a lobby before the match | M18q |
| Expected | Bots that fill a server, at a chosen skill, playing by the same rules as people | M18r |
| Better | Coloured lightmaps, replacement textures, skyboxes, fog, transparency | M18i |
| Better | Real-time shadows from the engine's shadow passes on models | M18i |
| Better | Voice chat on the native protocol (M17h) | M18l |
| Better | Skeletal models (IQM) beside MDL: read into the engine's model, baked into the model pass's frames (glTF later) | M18l |
| Better | A mod's own particle effects by name (point and trail builtins), drawn from the game's `particles/*.fx` | M18l |
| Better | A data-driven particle system for mods (effects named in a script) | M18l |
| Better | The common QC extension builtins and fields (string handling, tracing, the extra entity fields), registered by name | M18c, M18l |
| Later | Client-side QC (a mod drawing its own HUD and predicting its own entities) | M18m |
| Later | Single-player campaigns and co-op on the single-player `progs` | M18s |
| Better | The easy jump: `cl_speedjump 1` hops and gains on forward and jump alone where the server's `pm_speedjump` allows it, and on any other server by the client strafing for the player, disclosed in the user info (`docs/movement.md`) | M18e |
| Better | The quick weapon: `cl_quickweapon 1` makes +attack fire the best weapon carried of `cl_quickweapon_prio` and put `cl_quickweapon_default` back; +attack2 the same off `cl_quickweapon_prio2`; `cl_quickweapon_preview` shows the weapon a press would fire see-through in hand, `cl_quickweapon_hide` hides the hand instead (§ The quick weapon) | M48i |
| Better | Weapon groups: `cl_weapongroups 1` makes each number key pick the best weapon of its group (`cl_weapongroup<n>`, the game's own by default), the next on a second press, with a HUD column that opens on the key and folds into the weapon in hand (§ Weapon groups) | M48j |
| Later | Splitscreen; touch controls for phones and tablets | M18m |

### The quick weapon

A fire key that fires the best weapon carried, then puts the usual one back, all on the client
(`plugins/shooter/quick_weapon.hpp` for any shooter, `client/quick_fire.hpp` the game's wiring),
so it works in the window's own match and on any server joined: it asks only for what a player
could, a weapon by its key (`impulse`) and the attack.

| Cvar | Default | What it does |
| --- | --- | --- |
| `cl_quickweapon` | 0 | 1: +attack is a quick fire off `cl_quickweapon_prio` |
| `cl_quickweapon_prio` | `7 8 5 6 3 2 4 1` | +attack's weapons, best first, by weapon key |
| `cl_quickweapon_prio2` | `8 3 2` | +attack2's (the hitscan guns); empty: +attack2 fires what is in hand |
| `cl_quickweapon_default` | `2 1` | the weapon put back: the first carried with ammunition |
| `cl_quickweapon_preview` | 1 | the weapon +attack would fire held see-through in hand, turning solid as it fires and back |
| `cl_quickweapon_hide` | 0 | with the preview off: no weapon drawn while +attack would switch |

+attack2 is bound to no key: `bind mouse2 +attack2` (MOUSE2 jumps by default). A press asks for
the weapon with the attack in every command until it is in hand, so a server that keeps an ask
only for the command it came in still switches and fires the moment the weapon in hand is free; a
tap holds the attack until the shot shows (the ammunition fell) or a second passes; the way back
is asked every 0.2 s for 1.5 s; a weapon key the player presses wins. The preview is the held
weapon drawn with `HeldDraw::opacity` below 1: a screen door that moves each frame, which the
frame's TAA-U averages to a see-through gun. Through a tap of +attack the hand keeps holding the
quick weapon: the ghost turns solid in 0.06 s, fires, stays solid 0.15 s more and eases back to the
ghost over 0.25 s while the kept weapon comes back unseen, so no weapon switch is drawn. +attack2
switches as any weapon key does. Lab: `se-quickweapon` (its `tap-chart`).

### Weapon groups

The number keys pick from groups of weapons instead of one weapon each, on the client
(`plugins/shooter/weapon_groups.hpp` for any shooter, `client/weapon_groups.hpp` the game's
wiring), so it works in the window's own match and on any server joined.

| Cvar | Default | What it does |
| --- | --- | --- |
| `cl_weapongroups` | 0 | 1: a number key whose group is set picks from it |
| `cl_weapongroup1` .. `cl_weapongroup9` | empty | the key's weapons, best first, by weapon key; empty: the game's own group; `0`: no group (the key picks its own weapon) |

The classic game's groups are 1 `7 6` (the rocket launcher, then the grenade launcher), 2 `8` (the
beam gun), 3 `3 2` (the shotguns), 4 `5 4` (the nailguns), 5 `1` (the melee weapon); keys 6 to 8
have none, so they still pick their own weapon. The sequel's, by its own keys, are 1 `8 7 6` (the
explosives), 2 `10` (the railgun), 3 `3 2` (the shotguns), 4 `9 5 4` (the rapid-fire guns), 5 `1`
(the blaster) and 6 `11` (the BFG). A key asks for the best weapon of its group carried with a
shot's ammunition; pressed again within 2 s it asks for the next, round to the first. The number
key's `impulse` (the default binds) is read as the group, before the quick weapon sees it.

The HUD's `wgroup` element, in every look (moved and sized by its `hud_wgroup_*` cvars), is a column
on the right edge: on a key it opens to the group's weapons carried, best first, the one asked for
framed; the attack, or 2 s without a press, folds the others down into it over 0.2 s; folded it
shows the weapon in hand alone, with a gold `+` while its group holds another weapon carried. A
weapon without ammunition shows faded. Lab: `se-weapongroups`.

## Labs

Every row lands its labs (G9), and every lab runs headless in the container:

| Lab | Kind | What it shows |
| --- | --- | --- |
| `pak` | sim | Archives mounted in order, every file's winner, lookup cost; digest of the file table |
| `bsp` | picture | A map from above: leaves, the PVS of a point, hulls, entities; lump sizes and limits |
| `qc` | sim | A progs run: the test program's statements and calls per function at five sizes (a heatmap), builtins hit, a fault's QC stack trace, opcode throughput by group (a box chart) (built, M18c) |
| `pmove` | scenario | Scripted movement on fixture maps: ground acceleration, air control, strafe jumping, bunny hopping, stairs, ramps, water; trajectories charted against the committed recordings (built, M18o1) |
| `framemove` | scenario | The original player move (`plugins/shooter/framemove.hpp`) against its 35 recorded scenarios (`tests/golden/shooter_framemove.txt`) over the classic move's scenes: each scenario's worst position and velocity error and first step out, a chart of the worsts, every path from the side over the scene's brushes (built, M18s3) |
| `se-loop` | scenario | The classic loop on the test arena: two clients over latency and loss, a rocket, a bouncing grenade, a door a player blocks, a plat; the program's events and the loop's counts, paths from above and the side, the door and the plat against the clock, the prediction's corrections; the thrown entities against their recorded vectors (built, M18o2) |
| `se-parity` | scenario | The same scripted inputs through our movement and an existing server's, measured against each other within a tolerance (needs the reference program; skipped with a reason when its cache is absent) |
| `se-baseline` | scenario | The baseline mod compiled from its sources and played by bots on the arena with its door and plat: the build's counts, frags and deaths, obituaries by weapon, pickups, every path and missile from above, the frags and the movers' heights against the clock (built, M18d) |
| `se-mod` | scenario | A mod (the test mod, or a player's progs from `--progs`) played by bots on the test arena to its time limit: the hooks it was called on, the builtins it called, frags, every path from above, the hooks' timeline (built, M18p) |
| `se-campaign` | scenario | A single-player program (crc 5927: the player's own from `--game`, or the test mod's single-player stub) on its layout table: the program, the layout check entry by entry, the builtins it declares against what the server installs for its form, whether the full game's packs are there; the two forms' offsets compared name by name, the autoaim on the arena from the side; the campaign host on the campaign stub (a level change, a restart: the parms and runes carried) and, with `--game`, on the pack's own map (`--map`, `--skill`, `--then=changelevel e1m2`, `--forward` and `--yaw`: the player walked on the server's move; the monsters and secrets counted, the faults, the player's parms; `--exit=1`: the player stood in the map's exit, the level's end read as the window reads it, the attack held on to the next map; `--fire=1`: the attack held on the walk; the client data on the walk charted: the ideal pitch, the punch, the health and every blow; the effects on the walk counted by kind, and anything left unread; `--save=<s>`: the game saved after the walk, loaded into a second campaign and both walked on that long with the same commands, their digests compared every frame and charted as `save-continue`; then the classic text save written, read into a third campaign and written again (the same text?), and that game walked on beside the original: its faults, the players' distance, the entities live; on the stub always: a save after the restart run on 200 frames) (built, M18s1 to M18s5) |
| `se-coop-episode` | drive | A co-op episode played through on the player's own pack (`--game`; without it the coop hall with an exit): a leader bot finding each map's way out (`bots/way_through.hpp`) and companions beside him, map after map; a row a map (out or not and how long, the monsters and who killed them, the deaths, the companions' share of the time near him, what his plan went for), each map from above with everyone's track, the exits and the kills, the leader's changes of plan (`--plan=1`) and a digest of every frame (`--first`, `--maps`, `--map-seconds`, `--skills`, `--leader`, `--seed`) (built, M18s6a) |
| `se-coop` | scenario | Co-op companions beside a scripted leader on the coop hall (our campaign in coop: the arena with a button, coop starts and monsters): the trails, the monsters and where each fell from above, each companion's distance from the leader against time (his waits shaded, the button, the door and the kills marked), a ribbon of each one's mode and idle act against the leader's legs, and how near it kept, its kills, its held shots and its idle acts tabled (`--skills`, `--traits`) (built, M18s6a) |
| `mod` | scenario | The engine's own C++ mod format: the sample mod's match beside the plain match (the loader's verdict, rockets, bounces, the mod's log, both matches' missiles, the rocket's distance and the grenade's height), and the modded match replayed to the bit (built, M52a) |
| `se-server` | drive | Eight bots deathmatch on a free map for a minute: frags, items, the server's frame cost, its digest |
| `se-nav` | scenario | Where a player can go on a map by the player move's own proof: the nav graph from above to scale (the floor shaded off the point hull, every node and link by kind, what a mover closes), the quickest routes as the proofs moved a player, from above and the side; on the nav course and the test arena (built, M18r1) |
| `se-pilot` | scenario | Whether the pilot plays a nav route through the move as fast as the graph proved it: every course route with plain and human hands, the trails from above coloured by mode, times against proven, the strafe jump's speed on a straight, the human axes against a person's bands; on the server's arena the pilot in a bot slot through the door, up the plat and barring the door shut from the east (built, M18r2) |
| `se-senses` | scenario | Whether a bot sees only what a player's client could and aims by its skill: on the server's arena a seer and a runner visit the goals once per aim skill; the trails from above with a sight line every quarter second it saw, the aim's error against time, how often it saw when a line joined them, the error's median and p90, the reaction and the goals kept (built, M18r3) |
| `se-bots` | scenario | Whether fighters play the baseline mod's deathmatch like players: four fighters on the arena, the trails, items and deaths from above, each one's mode through the match as a strip, the frags against time, and a table of frags, deaths, shots, pickups, switches, the share in each mode and the weapon held most (built, M18r4) |
| `se-moves` | sim | A demo's player followed through a real map (`--file`, `--player`, `--map` or `--basedir`): the map as a wireframe from above and at an angle with his path coloured by speed and his jumps marked chained or not, the longest chain close up with the view's ticks, the chain's speed and turn against the most the move allows, a chase view from inside the map and its GIF, every player's hops tabled, the lag and ground-frame histograms; on the fixture halls without a demo (built, M18r12) |
| `se-hop` | scenario | The demo player's longest trips replayed by the pilot from his start and speed: walk, strafe, the expert hop, the average's simple hop, the hop with human hands, the ideal hop, each one's time and hop reading against his; the trips from above and from inside; trips his time beats the bots' by a quarter flagged as tricks or shortcuts (built, M18r12) |
| `se-kills` | sim | Kills and rocket flights compiled from many demos of one map (`--dir`, `--map`, `--weapon`): every kill as a line from killer to victim over the wireframe, added up into a density, from above and at an angle; the GOLD SHOTS (killer and victim in the same `--cell` cubes at least `--gold` times) as numbered arrows with their xyz tabled; one small map a weapon; heat of deaths and of rocket and grenade landings; every flight, hit or miss, with the lanes flown most; the top gold shot from inside the map; distances by weapon; `kills.tsv` and `gold.tsv` for a bot's tables; four scripted matches on the fixture halls without demos (built, M18r13) |
| `se-lobby` | sim | The warmup on the listen server: bots seated on teams, the free roam, ready, break, the countdown and the map restarted for a clean match (built, M18q1); later clients find a hosted game on the LAN and through the rendezvous, fill the lobby, chat, start, play and return, and a dedicated server outlives the client that started it |
| `se-net` | sim | Compatible and native clients on the simulated network under every preset: bandwidth, prediction errors, corrections (the compatible server half built, M18g: scripted clients under every preset, bytes a second, loss, downloads, the digest) |
| `se-interop` | drive | Our client and server against existing programs on loopback (cache-gated like `se-parity`; a stub reporting "not run" with its commands, M18g) |
| `se-weapon-fx` | picture | Every weapon's effects in the window's frame, classic beside enhanced: the player handed every weapon, aimed at the wall nearest 9 m off, each weapon fired and pictured after its shot (a launcher's after its blast), from the eye and from a camera beside the line of fire; the fixture halls, or a pack's map and its own weapons with `--game` (private); the births, the blasts and the beams each look drew |
| `se-client` | picture | The `shooters_edge` window's frame without a window: the fixture halls from a pack, the baseline mod as a listen server, a rocket jump; the frame from the player's eyes with the weapon in hand, the HUD and the effects at the spawn, the blast (its sparks, light and red flash) and the top of the jump; the rise, the health it cost, the effects thrown and the shots the hand saw (built, M18h first slice) |
| `se-world` | picture | Named views of free maps in both looks, with captions |
| `se-models` | picture | The things a map places, as the window draws them: the armour, a health box, an ammo box, a weapon, a torch or a flame, the quad, the shield, another player in his colours and one holding both powers, each from a clear spot near it in both looks; a lift lowered and up, a door shut and open (M18i6); the things placed by kind and the light under each (built, M18i3; `--pak` and `--map` for a pack's own maps) |
| `se-menu` | picture | The main menu's pages in the modern look and in the classic look (the fixture's pictures, or the player's pack with `--pack`), every row the game cannot do yet dimmed, classic-enhance's menu under OPTIONS; at desktop, wide and phone sizes; the campaign's screens (the intermission, a finale typed part way, the solo scorebar with the level's figures) and a match's scoreboard (the players, the teams, the large game) in the engine's type, the fixture and the pack (built, M18k2, M18k3, M18s4) |
| `se-audio` | sim | Sounds of a scripted fight rendered, metered and written as WAV |
| `se-viewmodel` | picture | The game's eight free weapons in first person on the engine's corridor stage: every one held at rest, each fired in the dark lit by its own muzzle light, the shotgun switched to the rocket launcher, and the player's view-model cvars; each weapon's share of the picture, its muzzle on the screen, its flash's lift and the switch's time (built, M48d) |
| `se-ambient` | sim | A walk through rooms under sky and by water: the ambience by leaf charted, mixed and written as WAV (built, M18j2) |
| `se-demo` | sim | A demo of any format read headless (`--file` a real one, plain or gzip): the recording's counts, every player's path from above, each one's health and the view's over the demo's clock, a table of who played (built for reading, M18h); then seeked and re-recorded, with the digest of its frames |

## The `ge` tooling it needs

The port is also a test of `ge` (G6). These are the commands an agent will need to iterate on
it quickly; each is added to `ge` as the row that needs it starts, and any further gap found
during the work is added when it is found and recorded in `docs/goals/ge-backlog.md`.

| Command | What it answers |
| --- | --- |
| `ge pak ls/cat/extract/build <archive or dir>` | What a search path holds, which archive wins a file, a file's bytes; a fixture pack built from a directory |
| `ge bsp info/ents/shot <map>` | A map's lumps and limits, its entity lump as text, a top-down picture of leaves, PVS from a point and hulls, all on the CPU |
| `ge qc build/disasm/run/trace/profile` | Compile QC, disassemble a progs, run a function headless, trace a step's QC calls with arguments and results, time every function (`disasm`, `run`, `trace` and `profile` built in M18c; `build` in M18d) |
| `ge qc diff <progs-a> <progs-b>` | Which functions and fields changed between two builds of a mod |
| `ge server --map=<m> --bots=<n> --for=<s>` | A headless dedicated server running bots, its input log and digest, for `ge replay`, `ge state` and `ge diverge` |
| `ge demo info/frames/shot/diff <demo>` | A demo decoded to text frame by frame, a frame pictured, two demos compared to their first difference |
| `ge net inspect --protocol=classic <pcap>` | A capture decoded as the classic protocol's messages (the net channel, every server and client message, the deltas) |
| `ge pmove <scenario>` | A movement scenario's trajectory as a table and a chart, against the parity recording when one exists |
| `ge bot run/compare/sweep` | Bots on a map for a time, two bot builds or profiles against each other, a knob swept; frags, humanness and the decisions behind a fight |
| `ge console <server> "<command>"` | A command or cvar run in a headless server or client, its output returned |
| `ge play` with angle axes | An agent at the controls of a first-person game: look angles on the protocol's grid, the usercmd's milliseconds, several players a run |

## Rows

The roadmap rows are `docs/goals/M18*.md` (M18 and its lettered rows), after Fall Line. The order:
content (M18a) and the map (M18b) first, then QC (M18c, M18d), movement (M18e), the game loop and
multiplayer mods (M18o, M18p), the server (M18f) and its bots (M18r), the compatible protocol (M18g), the client (M18h), the picture (M18i), sound
(M18j), input and the 2D layer (M18k), hosting and the lobby (M18q), the modern features that need all of it (M18l), the
single-player campaign and the movement setting (M18s), the classic games as profiles (M18t) and what can wait (M18m).
