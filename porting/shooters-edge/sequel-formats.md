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

## Not yet specified

The recording format (`.dm2`, the demo's `q2demo1.dm2`) and the network protocol (version 34)
are not needed to play the demo maps under our own runner and are left for a later note when
playing the sequel's own recordings, or joining its servers, is a row.
