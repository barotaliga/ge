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
  with spaces goes in quotes (`-basedir "/Volumes/Old Games/classic"`). **`-game <folder>`**
  picks a game folder other than `id1`. **`-progs <file>`** runs a mod's progs in place of the
  baseline mod (§ Running your own mod), and **`-models <dir>`** names the held weapons' folder
  (by default the tree's `games/shooters_edge/content/models/view`). Its packs are read in order (`pak0.pak`, `pak1.pak`, ...
  until a number is missing; any case), and a loose file in the folder (`maps/mymap.bsp`)
  overrides the packs (`client/files.hpp`).
- **`+<command>`**: console lines run at start. `+map dm6` loads that map; `+map arena` the
  built-in test arena (no packs needed, only the sky drawn). With no `+map`, the map `start`
  when the packs have it, else the arena. `--size=WxH` sets the window's size, `--frames=N`
  quits after N frames.
- **The keys**: W A S D move, SPACE jumps, the left button fires, 1 to 8 pick a weapon (7 the
  rocket launcher), the wheel the next one. The mouse looks while the window has it (raw
  counts, captured); a click takes it back after the console; closing the window quits.
- **The main menu** (ESCAPE; ESCAPE again goes back, and out of it at its root): the classic
  game's pages (§ The main menu), every row the game can do working and the rest dimmed.
- **The console** (the key under ESCAPE): `map <name>` changes the map, `sv <line>` runs a line on
  the server's console (`sv timelimit 10`, `sv fraglimit 20`, `sv sv_gravity 600`),
  `sensitivity <degrees a mouse count>` (0.066 by default), `m_invert 1` (both cvars, saved
  with the player's settings), `menu_look auto|classic|modern`, `togglemenu`, `r_look modern` (our
  sky, filtered textures and real-time lights; `classic` is the default) with the picture's other
  cvars (`r_fx_lights`, `r_fx_glow`, `r_bloom`, `r_lava_glow`, `r_slime_glow`; the bloom's from
  the next map) and `quit`.
- **A rocket jump**: 7 for the launcher, look straight down, then SPACE and the left button
  together. The blast costs about half your health and throws you about four times as high as
  a jump (held by `tests/shooters_edge_listen_test.cpp` and the `se-client` lab).

What the first slice draws: the map in its classic look (its textures, lightmaps, warped liquids
and sky) over the engine's sky, from the first-person rig (the view height off the mod's view
offset, stairs smoothed, a landing's dip); the game's effects (`client/world_fx.hpp`: a rocket's
trail and glow, an explosion's sparks, smoke and light on the walls, the red flash of a hit and
the tint under water, slime or lava); the weapon in hand (`client/held.hpp`: the game's free
models, switched with the number keys, kicked and lit by its muzzle flash as the mod fires it,
lit by the map's baked light under the player; `r_drawviewmodel`, `r_viewmodel_fov`, `cl_gunx`,
`cl_guny`, `cl_gunz` and `cl_gunsway` in the console); the HUD (`hud_style
classic|modern|compact`); the console; the main menu (below). Not yet: the other players' and
items' models, sounds, and the network (the rest of M18h, M18i).

### The main menu

`games/shooters_edge/menu/` (M18k2) is the menu ESCAPE opens over the running game (the listen
server's frames go on under it). Its pages are the classic game's:

| Page | Rows |
| --- | --- |
| main | SINGLE PLAYER, MULTIPLAYER, OPTIONS, HELP, QUIT |
| SINGLE PLAYER | NEW GAME (`map start`), LOAD, SAVE |
| MULTIPLAYER | JOIN A GAME, NEW GAME (the rules, then START), SETUP (name, shirt and pants colours) |
| NEW GAME | MAX PLAYERS, GAME TYPE (`deathmatch`), TEAMPLAY, SKILL, FRAG LIMIT, TIME LIMIT, MAP, START |
| OPTIONS | CUSTOMIZE CONTROLS, GO TO CONSOLE, RESET TO DEFAULTS, SCREEN SIZE, BRIGHTNESS, MOUSE SPEED (`sensitivity`), MUSIC VOLUME, SOUND VOLUME, ALWAYS RUN, INVERT MOUSE (`m_invert`), LOOKSPRING, LOOKSTRAFE, VIDEO OPTIONS, CLASSIC ENHANCE |
| VIDEO | WORLD LOOK (`r_look`), HUD (`hud_style`), MENU LOOK (`menu_look`), FULLSCREEN |
| HELP | the keys (in the classic look, the pack's help pages) |
| QUIT | YES (`quit`), NO |

- **Every row is the console's.** A setting reads its cvar when the menu opens and writes it with
  a console line when it changes, so the console, a config and the menu are one setting; NEW
  GAME's rules are the listen server's cvars, run as `sv <cvar> <value>` by START before its
  `map`. An action is a console line.
- **What the game cannot do yet is dimmed**, shown and skipped by the focus. A row names the cvar
  or command it needs (`kEntries` in `menu/main_menu.cpp`) and lights up by itself the day the
  console has it (`load`, `connect`, `volume`, `viewsize` ...); TEAMPLAY and SKILL stay dimmed
  while the baseline mod reads neither.
- **Two looks** (`menu_look`): with `auto` (the default) the menu is drawn in the pack's own
  pictures whenever the packs on the search path carry them (`gfx/mainmenu.lmp` and the rest,
  coloured by `gfx/palette.lmp`, the glyphs from `gfx.wad`): the plaque, the titles, the strips
  with the spinning cursor, the glyph rows, the help pages and the quit box on the classic
  320 x 200 screen at a whole-number scale. Without them, or with `menu_look modern`, it is the
  engine's menu view over the darkened game. Nothing of the pack is copied or committed.
- **OPTIONS ▸ CLASSIC ENHANCE** opens classic-enhance's own menu in its place: the pack (the packs
  on the search path), ENHANCE into the local cache watched stage by stage, every `ce_` switch.
- `togglemenu`, `menu_main`, `menu_options` and `menu_quit` open it from the console or a bind.
  The `se-menu` lab pictures every page in both looks.

## Running your own mod

The mod server (`games/shooters_edge/server`, M18p) runs a multiplayer `progs` as a compiler
wrote it. The tree ships one in source: the **baseline mod** (`games/shooters_edge/qc/`, M18d),
a complete deathmatch (weapons, ammunition, armour, health, deaths and respawns, frags, doors,
plats, buttons and triggers) written in QC for modders to copy and change. Its README is the
modder's guide; `ge qc build games/shooters_edge/qc/progs.src` compiles it to `progs.dat` with
the tree's own QC compiler (`plugins/progs/qc_compile.hpp`, the language in
`porting/shooters-edge/qc-language.md`), and the `se-baseline` lab plays it against bots.

- **Point the game at it** with a path: `--progs=<file>` on the command line, or the `sv_progs`
  cvar in the server's console lines (`sv_progs mods/arena/progs.dat`, or `set sv_progs ...`).
  The path given on the command line wins, then the cvar, then the built-in test mod. Today the
  `se-mod` lab is the front end: `make lab LAB=se-mod ARGS="--progs=<file> --bots=4
  --timelimit=1"` plays a match against bots on the test arena and draws it. The game window
  takes it as `-progs <file>` (§ Playing it); the dedicated server (M18f) takes the same flag.
- **Line numbers in faults**: put the line table beside the file as `<file>.lines` (a line
  number per statement, the sidecar `ge qc` reads: `parse_lines` in `plugins/progs/file.hpp`). Without it a fault names the function and the
  statement.
- **What the server offers**: every standard builtin (1–82), and the extensions in
  `server/catalogue.hpp`, each answered by `checkextension` (the maths, `tokenize`, `tracebox`,
  `randomvec`, `registercvar`, `min`/`max`/`bound`, `cvar_string`, the string extensions and
  `strzone`, `calltimeofday`, pausing, visible weapons, the console's `set`, client commands,
  `items2`, the extra move types). A builtin the server does not offer faults with its number;
  an unknown map key is skipped with a message (`x is not a field`).
- **When it does not load**: the message says why (a refused record, a missing engine-side
  global or field, a layout of another version) and what to do next. A mod built for the
  single-player game, or for a server with a different field layout, is refused at load.
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
   building that subsystem. Research that needs to read source to answer a question writes the
   answer as a fact in the note, in a separate session, and the building session reads only
   the note.
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
  light costs no re-upload. Coloured light from `.lit` files when present (to come).
- **Materials**: animated texture sequences ("+0" .. "+9", five frames a second, each texture
  starting at its own frame), warped liquids, the two-layer scrolling sky (a skybox to come),
  fullbright palette colours as emissive.
- **Models and sprites**: MDL frames as morph targets in one buffer, interpolated on the GPU;
  the player's colours remapped in the shader; sprites as instanced quads by orientation type.
- **Effects** (`games/shooters_edge/app/effects.hpp`): our own trails and bursts on the engine's
  particles (a pool over no ground: nothing lands): a rocket's embers and smoke, a grenade's
  smoke, an explosion's sparks and smoke, a hit's sparks and dust, a wound's droplets (dark: the
  pool has no colour channel yet), a teleport's column of cold light, lava thrown up. **Dynamic
  lights** (`set_dynamic_lights`): the classic adds them to the lightmaps' light, falling off in
  a straight line to their radius; the modern draws them as point lights. **The screen's blend**
  (`render/post.hpp`'s `set_post_blend`): the liquid round the eye, the powers held and a hurt's
  fading red folded into one colour and share the post pass mixes in after its grade.
- **Two looks, one renderer** (`games/shooters_edge/app/look.hpp`): the classic look (crisp
  texels, the lightmaps and their styles, the map's own sky, no bloom, and the post pass's grade
  pulling every colour to the nearest of the palette's 256) and the modern look (filtered,
  real-time lights with shadows, our sky and liquids, bloom; TAA-U and replacement textures to
  come). The `se-world` lab pictures every named view in both. The player sets the picture with
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
all, naming the first frame that leaves it. The headless `shooters_edge_server` binary runs the
host with its slots filled by bots, prints a status line every few seconds and the digest at the
end, writes the input log (`--log`) and replays one (`--replay`); `--realtime` paces it to the
wall clock. Until M18r's bots land, a seeded stand-in wanders the arena and fires at whoever is
ahead; until M18b's maps, the server runs the test arena under a roof. The `se-server` lab runs a
full server and replays its log.

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

**The lobby (M18q).** Before the match, on M17d's sessions: the players and their pings, teams,
ready, chat to all or the team, a map vote, kick, and the host's start; at the end of the match
everyone returns to it. Joining is from the server browser, an invite (M17j's friends), or an
address, with NATs crossed by hole punching or the relay (M17e). A client speaking the
compatible protocol skips the lobby and joins the match, as it would on an existing server.

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
| Better | Skeletal models (IQM, glTF) beside MDL, on the engine's skinning | M18l |
| Better | A data-driven particle system for mods (effects named in a script) | M18l |
| Better | The common QC extension builtins and fields (string handling, tracing, the extra entity fields), registered by name | M18c, M18l |
| Later | Client-side QC (a mod drawing its own HUD and predicting its own entities) | M18m |
| Later | Single-player campaigns and co-op on the single-player `progs` | M18m |
| Later | Splitscreen; touch controls for phones and tablets | M18m |

## Labs

Every row lands its labs (G9), and every lab runs headless in the container:

| Lab | Kind | What it shows |
| --- | --- | --- |
| `pak` | sim | Archives mounted in order, every file's winner, lookup cost; digest of the file table |
| `bsp` | picture | A map from above: leaves, the PVS of a point, hulls, entities; lump sizes and limits |
| `qc` | sim | A progs run: the test program's statements and calls per function at five sizes (a heatmap), builtins hit, a fault's QC stack trace, opcode throughput by group (a box chart) (built, M18c) |
| `pmove` | scenario | Scripted movement on fixture maps: ground acceleration, air control, strafe jumping, bunny hopping, stairs, ramps, water; trajectories charted against the committed recordings (built, M18o1) |
| `se-loop` | scenario | The classic loop on the test arena: two clients over latency and loss, a rocket, a bouncing grenade, a door a player blocks, a plat; the program's events and the loop's counts, paths from above and the side, the door and the plat against the clock, the prediction's corrections; the thrown entities against their recorded vectors (built, M18o2) |
| `se-parity` | scenario | The same scripted inputs through our movement and an existing server's, measured against each other within a tolerance (needs the reference program; skipped with a reason when its cache is absent) |
| `se-baseline` | scenario | The baseline mod compiled from its sources and played by bots on the arena with its door and plat: the build's counts, frags and deaths, obituaries by weapon, pickups, every path and missile from above, the frags and the movers' heights against the clock (built, M18d) |
| `se-mod` | scenario | A mod (the test mod, or a player's progs from `--progs`) played by bots on the test arena to its time limit: the hooks it was called on, the builtins it called, frags, every path from above, the hooks' timeline (built, M18p) |
| `mod` | scenario | The engine's own C++ mod format: the sample mod's match beside the plain match (the loader's verdict, rockets, bounces, the mod's log, both matches' missiles, the rocket's distance and the grenade's height), and the modded match replayed to the bit (built, M52a) |
| `se-server` | drive | Eight bots deathmatch on a free map for a minute: frags, items, the server's frame cost, its digest |
| `se-nav` | scenario | Where a player can go on a map by the player move's own proof: the nav graph from above to scale (the floor shaded off the point hull, every node and link by kind, what a mover closes), the quickest routes as the proofs moved a player, from above and the side; on the nav course and the test arena (built, M18r1) |
| `se-pilot` | scenario | Whether the pilot plays a nav route through the move as fast as the graph proved it: every course route with plain and human hands, the trails from above coloured by mode, times against proven, the strafe jump's speed on a straight, the human axes against a person's bands; on the server's arena the pilot in a bot slot through the door, up the plat and barring the door shut from the east (built, M18r2) |
| `se-senses` | scenario | Whether a bot sees only what a player's client could and aims by its skill: on the server's arena a seer and a runner visit the goals once per aim skill; the trails from above with a sight line every quarter second it saw, the aim's error against time, how often it saw when a line joined them, the error's median and p90, the reaction and the goals kept (built, M18r3) |
| `se-bots` | scenario | Whether fighters play the baseline mod's deathmatch like players: four fighters on the arena, the trails, items and deaths from above, each one's mode through the match as a strip, the frags against time, and a table of frags, deaths, shots, pickups, switches, the share in each mode and the weapon held most (built, M18r4) |
| `se-lobby` | sim | Clients find a hosted game on the LAN and through the rendezvous, fill the lobby, pick teams, chat, start, play and return; a dedicated server outlives the client that started it |
| `se-net` | sim | Compatible and native clients on the simulated network under every preset: bandwidth, prediction errors, corrections (the compatible server half built, M18g: scripted clients under every preset, bytes a second, loss, downloads, the digest) |
| `se-interop` | drive | Our client and server against existing programs on loopback (cache-gated like `se-parity`; a stub reporting "not run" with its commands, M18g) |
| `se-client` | picture | The `shooters_edge` window's frame without a window: the fixture halls from a pack, the baseline mod as a listen server, a rocket jump; the frame from the player's eyes with the weapon in hand, the HUD and the effects at the spawn, the blast (its sparks, light and red flash) and the top of the jump; the rise, the health it cost, the effects thrown and the shots the hand saw (built, M18h first slice) |
| `se-world` | picture | Named views of free maps in both looks, with captions |
| `se-menu` | picture | The main menu's pages in the modern look and in the classic look (the fixture's pictures, or the player's pack with `--pack`), every row the game cannot do yet dimmed, classic-enhance's menu under OPTIONS; at desktop, wide and phone sizes (built, M18k2) |
| `se-audio` | sim | Sounds of a scripted fight rendered, metered and written as WAV |
| `se-viewmodel` | picture | The game's eight free weapons in first person on the engine's corridor stage: every one held at rest, each fired in the dark lit by its own muzzle light, the shotgun switched to the rocket launcher, and the player's view-model cvars; each weapon's share of the picture, its muzzle on the screen, its flash's lift and the switch's time (built, M48d) |
| `se-ambient` | sim | A walk through rooms under sky and by water: the ambience by leaf charted, mixed and written as WAV (built, M18j2) |
| `se-demo` | sim | A demo played back, seeked and re-recorded; the digest of its frames |

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
(M18j), input and the 2D layer (M18k), hosting and the lobby (M18q), the modern features that need all of it (M18l) and what
can wait (M18m).
