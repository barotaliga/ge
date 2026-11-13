# The sequel's files, specified

What the sequel's files hold beyond the first game's formats (`formats.md` has the pack, and the
BSP 38 lump table), in our words (README.md, the rules). For M18t3, the sequel's content drawn.
**Source**: `read` (from the sequel's GPL source, by the research session of `sequel.md`),
`measured` (from the demo pack's bytes, `sequel.md` § The demo pack), `verify` (to be measured by
a lab before code depends on it).

## Search order (read)

- A **game folder** is searched as: its packs from the highest number down, then its loose files.
  The packs looked for are `pak0.pak` to `pak9.pak`, each optional (a missing number is skipped,
  not the end); a file in a higher-numbered pack wins over the same name in a lower one, and
  **any pack wins over a loose file** of the same name. This is the reverse of the first game's
  "loose files on top" (`formats.md` § Search order); the sequel's adapter keeps its own order.
- The **base folder** `baseq2` is added first, then the mod folder (the `game` setting, when it
  is not empty and not `baseq2`), so the whole mod (its packs, then its loose files) is searched
  before the whole base.
- A pack holds at most 4096 files; names are compared case-sensitively by the original on
  Linux and case-insensitively on Windows (our reader: case-insensitive, as `engine/vfs` does).
- The demo puts `players/` beside the pack as loose files (`sequel.md` § The demo pack), so the
  adapter reads loose files from the start.
- The **game library** (`gamex86.dll` and its kin) is looked for in the same folders, loose only.

## Contents and surface flags (read)

A brush's **contents** is a bit set; a leaf's contents is the OR of the brushes in it.

| Bit | Name (ours) | Meaning |
| --- | --- | --- |
| 0x1 | solid | blocks everything; an eye is never inside it |
| 0x2 | window | blocks movement and shots, drawn see-through |
| 0x4 | aux | unused by the game |
| 0x8 | lava | a liquid; hurts (§ in `sequel-rules.md`) |
| 0x10 | slime | a liquid; hurts |
| 0x20 | water | a liquid |
| 0x40 | mist | drawn, not solid |
| 0x8000 | area portal | the brush of a `func_areaportal`; splits areas |
| 0x10000 | player clip | blocks players (and dead players) only |
| 0x20000 | monster clip | blocks monsters only |
| 0x40000 … 0x200000 | current 0, 90, 180, 270 | a flow toward +x, +y, −x, −y; any may combine |
| 0x400000, 0x800000 | current up, down | a flow toward +z, −z |
| 0x1000000 | origin | stripped by the map compiler (marks a rotating model's centre) |
| 0x2000000 | monster | never on a brush: what a box entity answers when traced (§ `sequel-movement.md`) |
| 0x4000000 | dead monster | a corpse's box, for traces that ask for it |
| 0x8000000 | detail | a compile hint |
| 0x10000000 | translucent | set by the compiler when a face is see-through |
| 0x20000000 | ladder | climbable (the move's ladder rule) |

The **masks** the game traces with: solid = solid | window; player solid = solid | player clip |
window | monster; dead solid = solid | player clip | window; monster solid = solid | monster clip
| window | monster; water = water | lava | slime; opaque (sight) = solid | slime | lava; shot =
solid | monster | window | dead monster; current = the six current bits.

A face's **surface flags** (the texinfo's, `formats.md` lump 5): 0x1 light (the value is its
brightness), 0x2 slick (no ground friction, `sequel-movement.md` § Friction), 0x4 sky, 0x8 warp
(a liquid's wobble), 0x10 drawn at a third opacity, 0x20 drawn at two thirds, 0x40 flowing
(scrolls toward −s, 64 texture widths every 40 seconds, verify), 0x80 not drawn. The `value` is
the light's brightness for the compiler and unused at run time.

## The collision lumps (read)

The move and the shots collide with **brushes**, not the first game's clip hulls (`formats.md`
lumps 8, 10, 14, 15 of BSP 38; `plugins/brush` reads the surfaces today, and M18t4 adds the
brushes to its view):

- A **brush** is a convex solid: a run of brush sides (first side, count) and a contents. A
  **side** is a plane index and a texinfo (−1 for none: such a side has no surface, an empty name
  and no flags). The solid is the intersection of each side plane's back half-space.
- A **leaf** lists the brushes that touch it (first leaf brush, count, into the leaf-brush index
  lump) and carries a cluster (−1 outside any) and an **area**.
- **Planes** come in pairs (2n and 2n + 1 face opposite ways); a plane's type is 0, 1 or 2 when its
  normal is an axis (then only that coordinate is tested) and 3 to 5 otherwise.
- Each **model** (the world is model 0, a door is `*1`, …) has its own head node into the same
  node tree. Its stored bounds are spread by 1 unit each way when loaded.
- The **node tree** splits space by planes: a child under 0 is the leaf `−1 − child`. A point's
  leaf: at each node go to child 0 when its distance to the plane (the coordinate minus the
  distance for an axial plane, the dot product minus the distance otherwise) is 0 or more, else
  child 1.
- **Areas and area portals**: a map is cut into areas by its `func_areaportal` brushes; an area
  lists its portals (count, first), a portal names its number and the area beyond it. A door
  that owns a portal opens it while open; two areas are connected when a chain of open portals
  joins them (a flood fill run each time a portal changes). Entities in areas not connected to
  the viewer's are not sent, and sounds do not carry through a closed portal.
- **Visibility**: the visibility lump gives each cluster a potentially visible set and a
  potentially hearable set, each run-length coded as in the first game. The server uses the
  hearable set for sounds and the monsters' hearing.

## WAL, the wall texture (read; measured on `textures/e1u1/metal2_1.wal`)

A map's texinfo names a texture `e1u1/metal2_1`; the file is `textures/e1u1/metal2_1.wal`. The
header is 100 bytes, then four mip levels of 8-bit palette indices:

| Field | Bytes | Meaning |
| --- | --- | --- |
| name | 32 | the texture's own name, NUL padded |
| width, height | 2 u32 | of mip 0 (64 × 128 measured) |
| offsets | 4 u32 | each mip's start from the file's start (100, 8292, 10340, 10852 measured: each half the last across) |
| next | 32 | the next frame of an animation, a texture name, or empty |
| flags, contents, value | 3 i32 | the surface flags, contents and value the compiler copies into the texinfo |

At run time the texinfo's own flags and `nexttexinfo` chain are what count (`plugins/brush`
reads both); the WAL's copies are the compiler's input. A map's texture sizes are only in the WAL
files, so a reader that lays out lightmaps or texture coordinates opens them.

## PCX and the palette (read; measured)

- Every 8-bit picture is PCX: manufacturer 10, version 5, run-length encoding 1, 8 bits a pixel,
  one plane, the bytes-per-line equal to the width (measured on every picture checked), then the
  run-length pixels (a byte of 0xC0 or more is a run of its low six bits' count of the next byte),
  then a 0x0C marker and 768 bytes of RGB palette at the end. `engine/assets/pcx.hpp` reads it.
- **The game palette** is the palette at the end of `pics/colormap.pcx` (a 256 × 320 picture,
  measured; its pixels are the software renderer's light and blend tables and unused by a
  modern renderer). WAL textures, model skins, sprites and pictures all index it.
- **Index 255 is transparent** in every 8-bit picture loaded for drawing (pictures, skins,
  sprite frames).
- `pics/conchars.pcx` is the console font: 128 × 128, 16 × 16 cells of 8 × 8 (measured). The
  status bar's digits are separate pictures (`num_0` … `num_9`, `num_minus`, 16 × 24 measured; the
  alternate `anum_*` set for low values); icons are `pics/<icon>.pcx` named by the items
  (`sequel-rules.md` § Items).
- A picture name without a folder means `pics/<name>.pcx`; a name starting with `/` or `\` is a
  path from the game's root.

## MD2, the triangle model (read; measured on `models/weapons/v_rocket/tris.md2`)

Header: magic `IDP2`, version 8, then fifteen i32: skin width, skin height, frame size (bytes a
frame), skin count, vertex count, texture coordinate count, triangle count, draw command count
(32-bit words), frame count, then the byte offsets of the skins, the texture coordinates, the
triangles, the frames, the draw commands, and the end of the file. Measured: the held rocket
launcher is 252 × 194, 197 vertices, 284 coordinates, 356 triangles, 55 frames, frame size 828.

- **Skins**: each a 64-byte path (`models/items/quaddama/skin.pcx`, measured), the PCX file in
  the game's files. A player's skin is not in the model (§ Players).
- **Texture coordinates**: two i16 (s, t) in texels; divide by the skin's width and height.
  There are more coordinates than vertices: a vertex on a seam has two.
- **Triangles**: three vertex indices and three coordinate indices (six i16). The model's
  winding is the opposite of a front face's in our view: verify against a lab picture.
- **Frames**: scale (3 f32), translate (3 f32), a 16-byte name (`run1`, `attack3`: a stem and a
  number), then one 4-byte vertex a model vertex: x, y, z as bytes, decoded `byte × scale +
  translate`, and a normal index into the same table of 162 unit normals the first game's MDL
  uses (`formats.md` § MDL: derived, not copied). Frame size = 40 + 4 × vertex count.
- **Draw commands** (strips and fans with their own floating coordinates) duplicate the
  triangles for one old renderer and are ignored.
- **Animation**: a model has no clips: the game sets the frame number each 100 ms server frame,
  and the client blends from the last frame to the new one over the 100 ms (the back lerp).
  `plugins/morph` reads MDL into the shared form; MD2 is a second reader into the same form
  (every frame a morph target), its seams split by the coordinate index.
- **Linked models**: an entity may carry up to four models (its own and three more that share
  its origin and frame: a player's weapon, the gun's flash) by index (`sequel-rules.md`).

## SP2, the sprite (read; measured on `sprites/s_explod.sp2`)

Magic `IDS2`, version 2, frame count (i32), then a frame each: width, height, origin x, origin y
(i32: the point of the picture that sits on the entity's origin, from its top-left), and a 64-byte
path of the frame's PCX (`sprites/s_explod_0.pcx`, measured; 56 × 56 with its origin at 28, 28).
Sprites always face the viewer. The entity's frame number picks the frame.

## The sky (read; measured)

The world's `sky` key names a sky (`unit1_` on all three demo maps, measured); its six faces are
`env/<sky><side>.pcx` and `env/<sky><side>.tga` for the sides `rt`, `bk`, `lf`, `ft`, `up`,
`dn`. The TGA faces are 256 × 256, uncompressed 24-bit, rows bottom to top (measured); the PCX
faces are the same pictures in the palette. `skyrotate` (degrees a second) and `skyaxis` (a
vector) turn the box. Sky faces in the map (the sky surface flag) are where the box shows
through.

## Players: models, skins, sounds (read; measured)

- A player chooses `skin` = `<model>/<skin>` (default `male/grunt`). The model is
  `players/<model>/tris.md2`, the skin `players/<model>/<skin>.pcx`, the scoreboard's face
  `players/<model>/<skin>_i.pcx` (measured: about 2 KB pictures), the weapon in the player's
  hands (seen by others) `players/<model>/weapon.md2` with `weapon.pcx`. The demo ships the
  models `male` (15 skins with faces, and a `skin.pcx` with none) and `female` (11 skins), loose
  (measured).
- **Fallbacks, so every player is drawn**: a missing model becomes `male`; a skin missing from a
  model other than `male` is looked for under `male` (the model then becomes `male`); a skin still
  missing becomes the model's `grunt`.
- **Sounds a player's model owns** are named with a leading `*` in the game (`*jump1.wav`,
  `*death1.wav` … `*death4.wav`, `*fall1.wav`, `*fall2.wav`, `*gurp1.wav`, `*gurp2.wav`, and
  `*pain<h>_<n>.wav` for h = 25, 50, 75, 100 and n = 1, 2): the
  client looks for `players/<model>/<name>` (outside `sound/`), and, when absent, plays
  `sound/player/male/<name>`. The demo's male sounds are in the pack under `sound/player/male/`
  (measured); the shared ones (steps, wading, burning) under `sound/player/`.
- The status bar shows each item's icon `pics/<icon>.pcx`, and the HUD's layout is a program
  in the configuration strings (the status bar string), not a fixed picture: M18t3 draws it from
  the layout language (verify and specify when the HUD half lands).

## Configs (read; measured)

`default.cfg` (in the pack) and the player's `config.cfg` and `autoexec.cfg` are console text in
the first game's language (`configs.md`); the sequel's own commands and settings are named in
`sequel-rules.md` § Settings.

## Animation (read)

Read from the sequel's released source; frame indices measured by listing each MD2's frame names.
A server frame is 100 ms; every frame number below is an index into the model's frame list.

- **Items turn on the client.** The entity's effects bit 0x1 (rotate) makes the client set yaw to
  client time in ms ÷ 10 (whole division): 100°/s in whole-degree steps, pitch and roll 0, the
  map's angle ignored. The clock is the client's game time, held within 100 ms of the server's, so
  viewers agree to within about 10°. Set on armour, the power screen and shield, weapons with a
  world model, powerups, bandolier, pack and keys (placed or dropped); not on ammo or health. No
  bob. Every item on the ground also has render flag 512 (glow): its light gains 0.1 × sin(7 t),
  t in client seconds, never below 80% of the unpulsed light. An item's "no touch" spawn flag (1)
  removes both. Other effect bits make the client pick frames itself (0/1 or 2/3 at 2 Hz, all
  frames at 2 Hz or 10 Hz): animated brush textures and the BFG ball.
- **The player model** (male, female, cyborg: 198 frames, same names and order): stand 0–39
  (stand01–40), run 40–45, attack 46–53, pain1 54–57, pain2 58–61, pain3 62–65, jump 66–71, flip
  72–83, salute 84–94, taunt 95–111, wave 112–122, point 123–134, crouched stand 135–153, crouched
  walk 154–159, crouched attack 160–168, crouched pain 169–172, crouched death 173–177, death1
  178–183, death2 184–189, death3 190–197.
- **Player priorities**, low to high: basic, gesture (and landing), jump, pain, attack, death,
  reverse. One frame per server frame, stepped after that frame's movement, firing and damage, so a
  sequence's first frame is the first sent. Basic (stand, run, crouched stand or walk) loops and is
  re-chosen when a sequence ends; running is any horizontal speed. A duck change cuts anything below
  death. No ground at gesture or below: jump1, then hold jump2; regaining ground plays jump3–jump6 at
  gesture priority. Firing starts attack1–8 (crouched crattak1–9) at any priority; the hyperblaster
  holds attack1 while firing, the machine gun shows attack1 or attack2 (3 in 4) each frame, the chain
  gun alternates them. Pain (any damage that frame) only below pain priority; standing pains rotate
  2, 3, 1 on one counter shared by all players; crouched uses crpain. Death cuts everything, rotates
  2, 3, 1 likewise (crouched: crdeath) and holds its last frame. `wave <n>`: 0 flip, 1 salute,
  2 taunt, 3 wave, other point; refused crouched or above gesture priority. Weapon put-away plays
  pain304 down to pain301 (reverse) from 4 gun frames before the end; raising (and every spawn)
  sends pain302–304. A standing grenade throw plays wave07 down to wave01; crouched crattak1–3.
- **The carried weapon** is a linked model drawn with the body's frame, previous frame, blend,
  origin and angles. The skin number's high byte picks it from `players/<model>/`: 0 `weapon.md2`,
  then `w_blaster`, `w_shotgun`, `w_sshotgun`, `w_machinegun`, `w_chaingun`, `a_grenades`,
  `w_glauncher`, `w_rlauncher`, `w_hyperblaster`, `w_railgun`, `w_bfg` (1–11); missing falls back to
  that `weapon.md2`, then male's; `cl_vwep 0` draws only 0. Their frames are the body's 0–172 (plus an
  unused 173 `g_view` in most); the weapon is removed at death. Body pitch is view pitch ÷ 3, roll up
  to 4 × 2° with sideways speed (`sv_rollangle` 2, `sv_rollspeed` 200).
- **Monsters** think every 100 ms over an action: first and last frame, per frame a movement kind,
  a step distance (× model scale) and an optional routine, and an end routine. Frames advance by one
  and wrap; a switch goes to the new first frame unless the current frame is inside the new range; a
  routine may jump within the action or hold the frame. A death action's end stops the think, so the
  corpse keeps its last frame. Frame numbers equal model frame indices for every shipped monster
  (measured), so stems give ranges, but an action may use a subrange or frames of another stem.
  Exceptions: the supertank's names do not follow its actions; the final boss's walker is drawn with
  the rider model plus the walker as a linked model on one frame (rider frames 0–187 repeat the
  walker's); `misc_actor` uses a frame list that does not fit the player model it draws (no shipped
  map places one); a frame past a model's end draws frame 0.
- **The soldier** (475 frames): stand 146–175 and 176–214, walk 215–247 (usually 215–224) and
  256–265, start run 97–98, run 99–104, attacks 0–11, 12–29, 30–38 (duck and shoot), 39–44
  (machine gun, holds attak403 for 0.3–1.0 s), 109–122 (run and shoot), duck 45–49, pains 50–54, 55–61,
  62–79, 80–96 (thrown upward), deaths 272–307, 308–342, 343–387 (head shot), 388–440, 441–464,
  465–474, each holding its last frame. Frames 127–145 (`runt`) are unused.
- **Gibs** spin 0–600°/s on each axis by the server (angles += spin × 0.1 each frame, the client
  blends), stopping when they land; a landed small meat piece plays frames 1–10. A gibbed monster
  becomes its head, frame 0, spinning yaw at up to ±600°/s; a gibbed player becomes a head without
  spin. Player corpses keep their last death frame in 8 recycled slots.

## Monster frame runs (measured; the mapping derived)

Each monster model's frames, grouped by the stem of their names (the frames sharing a name in
order), read from the full game's first pack by a scratch script (measured). Which run plays which
action is derived from the names where the reader's note gives no table (all but the soldier):
`server/sequel_poses.cpp` holds the runs it uses. Notable bends: the infantry's and the gunner's
`stand` run is one long run (its fidget and its stand are not told apart by name); the boss2
model's stand begins with stand30–50 before stand1–29; the flyer has no death run (it explodes);
the flipper's swim is `flphor`, its bite `flpbit`; the hover's move is `forwrd`; the mutant's
`attack` run serves its claws and its leap alike.

## The carried weapon on the wire (read)

- **The names come from the model configstrings.** An entry starting `#` is a carried-weapon file
  name relative to `players/<model>/`, never a model to load or download. The game registers, while
  the world entity spawns (after the map, its `*n` brush models and the Blaster's models; before
  every other entity's): `#w_blaster.md2`, `#w_shotgun.md2`, `#w_sshotgun.md2`, `#w_machinegun.md2`,
  `#w_chaingun.md2`, `#a_grenades.md2`, `#w_glauncher.md2`, `#w_rlauncher.md2`,
  `#w_hyperblaster.md2`, `#w_railgun.md2`, `#w_bfg.md2` (CTF adds `#w_grapple.md2` last).
- **The client's list** is built once per level load: slot 0 `weapon.md2`, then each `#` entry in
  configstring order (walking up from model index 1 to the first empty one), at most 20 slots.
  Extra entries are ignored, and a `#` entry set mid-level waits for the next load. `cl_vwep 0`
  (archived, default 1) loads only slot 0. A cyborg missing a file tries male's. A player's info is
  valid only if body, skin, icon and `weapon.md2` all load; otherwise it draws as male/grunt.
- **The high byte** (skin number shifted right 8) indexes that list directly, so it is the 1-based
  position among the `#` entries; the stock 1–11 only holds because the game registers them in its
  item order. The game writes item number × 256 + slot on every weapon change, only while the body's
  model index is 255; 0 when there is no weapon (not reached in stock play). An index of 20 or more,
  or `cl_vwep 0`, means slot 0; a slot that did not load falls back to that player's slot 0, then
  male's `weapon.md2`. The skin number goes on the wire as 1, 2 or 4 bytes (below 256, below 65536,
  else), so a player with a weapon sends 2.
- **The low byte** is the slot (entity number − 1), naming the player-skin configstring
  `<name>\<model>/<skin>`. Body model index 255 means "body and skin from that client info"; model
  index 0 draws nothing, linked models included. Sexed sounds go by entity number − 1, so the low
  byte must equal it.
- **Model index 2 = 255** on a live player (set at spawn) is the carried weapon; 0 at death and at
  intermission (where the body's index and index 3 go to 0 too). The game sends nothing in indices
  3 and 4 for players. The client draws any non-zero index 2, 3 or 4 as extra models with the body's
  origin, angles, frame, previous frame and blend, skin 0 of the file and no render flags (the
  colour shell stays on the body); index 2 with bit 0x80 set draws model (value − 128) translucent
  at alpha 0.32.
- **The view model** is the player state's gun index, an ordinary model index (the item's
  `models/weapons/v_*/tris.md2`), reset to frame 0 on each change, 0 at intermission or with no
  weapon; independent of all the above. A client never draws its own player entity (body or linked
  models), so a player sees only that view model.

## Not yet specified

The recording format (`.dm2`, the demo's `q2demo1.dm2`) and the network protocol (version 34)
are not needed to play the demo maps under our own runner and are left for a later note when
playing the sequel's own recordings, or joining its servers, is a row.
