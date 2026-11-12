# The sequel, specified: the index

The classic game's sequel (Quake II, id Software, 1997) as Shooter's Edge will load it: its packs,
maps, models, rules and move, in our words (README.md, the rules). This page is the map of the
note set and the facts every other page leans on; the builder of the sequel's content (M18t3) and
of its rules runner and move (M18t4) reads these pages and never the source.

**Where each fact came from.** A research session (2026-11-05) read the sequel's game source as
id Software released it under the GPL (the 3.21 release: its server, its shared movement, its
collision model and its game library) and opened the freely distributed demo (the 3.14 demo
installer, `q2-314-demo-x86.exe`) to list and measure its pack. Facts marked **measured** were read
off the demo pack's own bytes by the research session's scripts (outside the tree); the rest are
**read** from the source. Nothing of the source, the demo or the scripts is in the tree. Exact
parity is the goal for the move and the collision (`sequel-movement.md`), close parity for the
match rules (`sequel-rules.md`), and the spirit, not the bits, for the monsters
(`sequel-monsters.md`).

| Page | What it holds | For |
| --- | --- | --- |
| `sequel.md` | This page: the game's shape, the demo pack, the game library's calls as our runner's | everyone |
| `sequel-formats.md` | The search order, contents and surface flags, the collision lumps, WAL, PCX and the palette, MD2, SP2, the sky, player models, skins and sounds, configs | M18t3 |
| `sequel-movement.md` | The player's move (shared by server and client), the packed state, the command, the brush trace, point contents | M18t4 |
| `sequel-rules.md` | The server frame at 10 Hz, entities and physics, items, armour, damage, every weapon and projectile, a player's life, deathmatch and its flags, the map entities of the demo | M18t4 |
| `sequel-monsters.md` | The monsters' senses, attack choice and the demo's roster with their numbers | M18t4 (single player) |

## The game's shape

- **One base folder, `baseq2`**, beside the executable. A mod is a sibling folder named by the
  `game` setting, searched before the base (`sequel-formats.md` § Search order). The full game
  installs `pak0.pak`; its point releases add `pak1.pak` and `pak2.pak` (verify on a player's copy
  before relying on the count; the demo has `pak0.pak` alone).
- **The rules are a native library** in the game folder (`gamex86.dll` on Windows, `gamei386.so`
  on Linux), loaded by the engine and called through one table of functions each way (§ The game
  library). Until M18v10 the adapter's `find_program` names it with the form "native game
  library, server side" and passes over it (`runs_form`); our own rules runner (M18t4) plays its
  maps instead. From M18v10 (Niclas's choice, 2026-11-11) our servers load it where the platform
  matches, so existing mods are hosted unchanged (`sequel-plan.md` step 10).
- **The server runs at 10 frames a second** (100 ms a frame, `FRAMETIME` 0.1) while each player's
  move runs once a command, at the client's own rate (commands carry their length in
  milliseconds). This is the biggest difference from the first game's loop (`gameloop.md`): the
  world thinks at 10 Hz, players move at 30 to 120 Hz.
- **The move is shared code**: the server and the predicting client run the same move on the same
  packed state, so the state is integers (§ in `sequel-movement.md`). Our move is a third
  `sv_playermove` value (`sequel`), and our client predicts with it.
- **Axes** are the first game's: x and y horizontal, z up, angles pitch (down positive), yaw
  (counter-clockwise from +x), roll, in degrees.

## The demo pack (measured)

The 3.14 demo installer is a self-extracting zip (sha256
`7ace5a43983f10d6bdc9d9b6e17a1032ba6223118d389bd170df89b945a04a1e`, 39 015 499 bytes). Under
`Install/Data/` it holds `baseq2/pak0.pak` (49 951 322 bytes, sha256
`cae257182f34d3913f3d663e1d7cf865d668feda6af393d4ecf3e9e408b48d09`), the game library, and the
player models and skins **as loose files** under `baseq2/players/` (not in the pack). The
classic-enhance profile row recognises the pack by that sha256, the way it names the first
game's releases.

The pack has 1106 files:

| Folder | Files | What |
| --- | --- | --- |
| `maps/` | 3 | `demo1.bsp` ("Outer Base"), `demo2.bsp` ("Installation"), `demo3.bsp` ("Comm Center"), all `IBSP` version 38; each names the next in its world's `nextmap` (1 → 2 → 3 → 1) |
| `maps.lst` | 1 | one line a map: its name and its title in quotes, CR LF line ends (the menu's map list) |
| `textures/` | 303 | WAL textures, under `textures/e1u1/` (the unit's set) and a few shared folders |
| `models/` | 229 | MD2 models and their PCX skins: `items/` 72, `objects/` 68, `weapons/` 50 (`g_*` lying about, `v_*` held), `monsters/` 31, `ships/` 6, `deadbods/` 2 |
| `sprites/` | 52 | SP2 sprites and their frames as PCX |
| `pics/` | 127 | the HUD and the menus as PCX: `colormap.pcx` (the palette), `conchars.pcx`, `num_*`, `anum_*`, `w_*`, `a_*`, `i_*`, `p_*`, `k_*`, `m_*` |
| `env/` | 12 | the sky `unit1_` as six faces, each both PCX and TGA |
| `sound/` | 377 | WAV under `sound/<group>/`; the player's own under `sound/player/` and `sound/player/male/` |
| `demos/` | 1 | `q2demo1.dm2`, the attract loop's recording |
| `default.cfg` | 1 | the default binds and settings (console text) |

The three maps hold both single-player and deathmatch starts (9, 11 and 14
`info_player_deathmatch`), so the demo plays deathmatch on all three and the single-player unit in
order. Every class name the three maps use is in `sequel-rules.md` § The demo's map entities.

## The game library: the calls each way

The engine and the game library talk through two tables. Their shape is the seam our runner
(`docs/shooters-edge.md` § The rules runner) already has; this is how the sequel's calls map onto
it, so the builder knows what each must do.

**The game answers** (the engine calls the game):

| Call | When | Our runner's |
| --- | --- | --- |
| init | once, when the library loads: the settings are registered and the clients' and entities' arrays sized (`maxclients`, `maxentities` 1024) | part of `start` |
| shutdown | the library unloads | — |
| spawn entities (map name, entity text, spawn point) | a map starts: the level is wiped, the entities read and spawned (`sequel-rules.md` § Spawning) | `start` |
| write game, read game, write level, read level | a save and its load (single player) | `carry` and the save (later) |
| client connect (entity, user info) → yes or no | a player begins connecting; may refuse (password, ban) | `connect` |
| client begin (entity) | the player is in, every level | `connect`'s second half |
| client user info changed | the player changed their name, skin, hand, fov | `command` |
| client disconnect | the player leaves | `drop` |
| client command (entity) | a console command from the player (`say`, `use`, `drop`, `inven`, `kill`, `score`, `give`, `noclip`) | `command` |
| client think (entity, command) | one movement command of one player, as soon as it arrives | `frame`'s commands |
| run frame | one 100 ms server frame | `frame` |
| server command | the server's console `sv <...>` (the ban list) | `command` |

The game also exports its entity array (the engine reads each entity's shared head: the network
state, the bounds and links, the solid kind, the clip mask, the owner, the server flags) and its
size. Our view (`RulesWorld`) is filled from that head the same way.

**The engine answers** (the game calls the engine): print to all, to one, in the centre; play a
sound on an entity or at a point; set a configuration string (the map's lists: models, sounds,
pictures, light styles, the items' names, the player skins, the status bar layout); find or add a
model, sound or picture index; set an entity's model (a map's inline model `*n` sets its bounds);
**trace** a box through the world and the solid entities with a contents mask and a pass entity;
**point contents**; in the potentially visible set, in the potentially hearable set; open or close
an area portal; are two areas connected; link and unlink an entity in the area tree; list the
entities in a box (solid or trigger ones); **run the player move** (`sequel-movement.md`); send a
message to the players who can see or hear a point, or to one player (reliable or not); write the
message's values; memory with tags (the game's and the level's); settings get, set and force; the
command's arguments; queue console text (`gamemap`, `menu_loadgame`).

The trace, point contents, area links, visibility and the move are the engine's half of the rules
and are specified exactly (`sequel-movement.md` § The trace, § Point contents). The rest is plumbing
our host already has.
