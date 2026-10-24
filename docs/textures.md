# Procedural textures

`engine/texgen` (`ge::texgen`, roadmap row M31, skill `textures`) makes a game's textures
from TEXT. A texture is a recipe of a few lines that an agent writes: what the texture is, the
knobs a level may turn, and the layers that build it. The engine bakes a recipe and a texture
reference into the maps a renderer samples. The bake is deterministic and seeded, and it runs
once per variant, because its files are cached on disk under a key of everything their bytes
depend on.

The aim is that a simple description becomes a texture that looks lived in. The pattern layers
cut structure into the height: joints, bevels, seams and rivets. The life layers then read that
height. Grime pools in the joints, wear rubs the edges, dust and snow lie on the ledges, moss
creeps out of the damp low places and glow fills the cracks. Their amounts are params, so the
same recipe can be a clean wall in one room and an old, wet, dirty one in the next.

## The recipe

```
# engine/texgen/library/brick_wall.tex
texture brick_wall          # the name: a-z, 0-9, _ and -
size    512                 # a power of two, 16..4096
seed    7                   # the recipe's own seed
surface stone               # what it is made of (footsteps, decals and impacts read it)
scale   2                   # the metres one tile covers in the world
bump    1                   # the normal map's strength (default 1)
tags    wall brick exterior # words a level or a tool picks textures by
param   wear 0.3 0 1        # a knob: name, default, and optionally its range
param   dirt 0.35 0 1

layer bricks  rows=10 cols=4 color=#8c4a32 vary=0.18 mortar_color=#a49c8e
layer noise   kind=perlin period=24 octaves=5 amount=0.35 blend=overlay height=0.06
layer noise   kind=white period=512 octaves=1 amount=0.12 blend=overlay
layer grime   amount=0.55
layer wear    amount=$wear color=#b07a5c
layer dirt    amount=$dirt period=3
layer streaks amount=$dirt count=24 length=0.4
```

- One statement a line. `#` starts a comment unless it opens a colour (`color=#8c4a32`).
- A `layer` line is a kind and `key=value` pairs. A value is a number, a colour `#rrggbb`
  (sRGB), a word, or a param: `$wear`, or `$wear*0.5` to scale it. A param can only set a
  number, and it is clamped to its own range and then to the key's.
- Every key has a default, so a layer names only what it changes. **`ge texture kinds` prints
  every kind and key with its default, range and meaning**, from the one table in
  `engine/texgen/layers.cpp`.
- Layers with the same `salt=` (on `noise`, `cracks` and `glow`) draw the same pattern. That is
  how a glow follows the cracks a cracks layer cut, or flagstone colours follow the joints.
- A mistake is a diagnostic with its line, what is wrong and what to write instead:
  `line 2: unknown key 'colr' for layer fill; it takes color height rough metal.`

### The layer kinds

| Group | Kinds | What they do |
| --- | --- | --- |
| base | `fill`, `noise`, `stripes` | Set a colour, height, roughness and metal; blend a noise field into them (mix, multiply, overlay, screen, add, with a two-colour ramp); draw stripes |
| pattern | `bricks`, `tiles`, `planks`, `panels`, `blocks`, `studs`, `scales` | Cut structure into the height: courses of bricks with ragged mortar, grouted tiles, boards with grain, knots, ends and gaps, steel plates with pressed seams and rivets, dressed stone in uneven courses, fields of domes, pyramids, diamond-plate bars or holes, and overlapping scales or arches. Each piece gets its own colour |
| life | `cracks`, `scratches`, `dirt`, `dust`, `grime`, `wear`, `rust`, `streaks`, `moss`, `snow`, `glow` | Age the surface by reading the height so far: cavities (`relief`) and ledges (`up_facing`), broken up by noise, with an `amount` a level can drive |
| finish | `speckle`, `shade`, `palette` | The paint over everything: grain a texel at a time with specks of a second colour gathering in the pits, the light baked into the colour off the height, and every colour snapped onto a few ramps of shades |

#### Techniques gathered from references

The kinds grow by studying textures other people painted (the `texture-references` skill):
what gives each its look is named, built as a kind or a key, and a recipe is steered toward the
reference with `texgen-compare` until it reads as the same kind of surface. No reference enters
the tree and none is named in it. What each round found and built:

| Technique | What it is | Where |
| --- | --- | --- |
| Baked light | A low-resolution texture draws its relief into the colour: the upper-left edge of every raised shape lit, the lower-right one and the joints dark. Strength, softness, highlight and occlusion | `shade` |
| Per-texel grain | Every texel a little brighter or darker than its neighbours (salt and pepper), sometimes in clumps of two or four | `speckle` `contrast`, `size` |
| Coloured specks | A second colour in a share of the grains, gathering in the pits: patina on copper, blue flecks in dark iron | `speckle` `amount`, `color`, `cavity` |
| A small palette | Every colour snapped onto a few ramps of shades, near black up to the colour and on toward white; an ordered dither between shades | `palette` |
| Uneven masonry | Courses of different heights, blocks of different widths, each course's joints shifted | `blocks` |
| Irregular ashlar | Even courses of long and short blocks, some running down through two courses (`span`), the joint drawn beside each tall block | `blocks` |
| Stud fields | Rows of domes, pyramids, the leaning bars of diamond plate, or holes | `studs` |
| Scales and arches | Discs in offset rows, each row lying over the one above, rimmed | `scales` |
| Rock chunks | Cellular edges cut into the height and lit by `shade` | `noise kind=edges` with `shade` |

The look in numbers (`look.hpp`): brightness, contrast, saturation, warmth, grain, edges and
colours, and a distance between two looks. It is how far a recipe is from its reference in
`texgen-compare`, and an agent's guide for which key to turn.

## Replacing a level's painted textures

A renderer that loads a map with hand-painted textures can offer to draw procedural ones in
their place, as an option (`replace.hpp`). For each texture:

1. **Measure it** (`likeness_of`): its look; which way its lines run (the share of texels with a
   hard step across against down: bricks step down far more than across, planks the other way);
   and how often its pattern repeats each way, the strongest peak of the luma's wrapped
   autocorrelation as a share of the tile, with how strong it is.
2. **Pick the nearest recipe** (`Matcher`, built once from a library with `make_matcher`, each
   recipe measured off two seeds at 64 px). The structure ranks them first, with the colour
   numbers left out since step 4 puts the colour back. Then the `refine` (6) nearest are baked
   with the texture's seed and colours and measured whole, and the nearest of those wins, so a
   recipe whose colours do not survive the recolouring falls behind. A renderer keeps the
   texture's own pixels when nothing comes within `max_distance` (0.8).
3. **Bake it** at the next power of two over the texture's size, under a seed of the texture's
   name, and resize it to the texture's size. An animation's frames (`+0name`, `+1name`) share
   one seed so they do not flicker between variants.
4. **Recolour it**: each channel's mean and spread moved onto the original's, the spread's ratio
   kept within a factor of two, so the room keeps its palette and brightness.
5. **Keep its lights**: the texels the original marks as full-bright stay exactly as they were,
   so a lamp still glows where the map put it.

A name starting with `*` (a liquid) or `sky` is never replaced; a renderer draws those itself.
`replace_indexed` takes a paletted texture and its 768-byte palette, and treats indices from 224
up as full-bright. The match and the bake are pure functions of the texture and the library.
The `texgen-replace` lab draws every texture beside its replacement, with the recipe picked, the
distance and the reference a level would store.

### Converting a map: references, never pixels

A tool that converts another game's map into ours may copy its structure but not its painted
textures. It asks `match_texture` (or `match_rgb`, `match_indexed`) for each texture and writes
the answer's `text`, a texture reference like any other:

```
red_bricks seed=2359537275 tint=#825540 spread=#281913
```

The seed is made from the texture's name (an animation's frames share it), and `tint` and
`spread` are the original's mean colour and per-channel spread. That is all that leaves the
call: a recipe's name and seven numbers. Every surface gets an answer even when no recipe is
near (`near` says which). A liquid comes back as `TextureKind::liquid` with its stem (`*04water1`
is `water`), and a sky as `TextureKind::sky`, for the converter to map to its own materials.
Build the `Matcher` once per conversion.

### The argued loop: `ge texture match`

A converter writes only a texture map an agent has argued (`<map>.texmap`,
`texture_map.hpp`). `ge texture match <dir> --map=<map>.texmap` reads the map's textures as PNGs
(kept outside the tree; a `textures.txt` beside them, as `labs bsp --textures-out=<dir>` writes,
gives each file's real name), writes a first reference for every texture, and re-measures every
run.
Per texture the agent writes four things into the map: `class` (what it is: bricks, blocks,
planks, panels, ...; candidates narrow to recipes tagged with it), `traits` (how that class
looks and how this one does: the laying pattern, the pieces a tile, the joints, the colour),
the clone (the `texture` line, tuned and ended `# pin`), and `judge` (pass or fail, trait by
trait). A look distance alone rates a wall of small regular blocks close to one of large
irregular ones, so ge also measures each texture's evidence (`evidence_of`: how many times the
pattern repeats across and down a tile, the courses and the pieces in each, which way the lines
run, the contrast) for the original and the clone, and prints each mismatch as a sentence to
answer: "the original has 4 courses a tile, ours 3 (turn rows)". A pinned line without the
original's `tint` and `spread` is warned about with the line to write. Each run draws a sheet per
texture (the original, the clone, the candidates, and under them the argument, the evidence and
the mismatches), failed and unargued first. The procedure and each class's traits are the
`texture-references` skill.

## The noise kinds

`value`, `perlin` (gradient), `cells` (the distance to the nearest point), `edges` (F2 - F1:
thin lines where cells meet), `voronoi` (one flat value per cell), `ridged`, `turbulence` and
`white`. Every kind takes octaves and gain, a domain `warp`, and (on the noise layer) a
`stretch` that squeezes it along y for grain and brushing. Each kind is periodic over the tile:
a lattice index is wrapped by its period before it is hashed, and a period or a stretch is
always a whole number, so **every texture tiles**. `texgen-noise` pictures every kind two tiles
across, and its test holds periodicity and the bits.

## Texture references: how a level names a texture

```
<name> [seed=<u32>] [<param>=<number> ...] [tint=#rrggbb] [spread=#rrggbb]
                                                e.g.  cave_rock seed=88123 wet=0.6 moss=0.2
```

`tint` and `spread` recolour the bake's albedo: each channel's mean moves onto the tint and its
standard deviation onto the spread, within a factor of two of the bake's own. A converted map
carries both (above); a hand-written level rarely needs them.

A level stores one reference per surface or room. The reference's seed is mixed with the
recipe's seed and each layer's place (`layer_seed`). So each room that names the same recipe
with its own seed gets its own variant, it is the same variant every time, and changing one
layer never re-rolls another. A param the recipe does not declare is an error, not a silent
default. `ref_text` writes a reference back in canonical order: the seed, then the params by
name.

The level format (row M32) embeds these references. Rooms and their procedural geometry carry
their seed in the same way.

## The maps

| Map | Contents |
| --- | --- |
| `albedo` | The colour, sRGB, RGBA8 with alpha 255 |
| `normal` | Tangent space off the height: +x right, +y up the tile, +z out of the surface |
| `orm` | Occlusion (from the relief), roughness and metalness in R, G and B, packed as glTF expects |
| `glow` | The emitted light, sRGB times brightness. It is only written when a texel glows |
| `height` | The height as grey: 0 deep, 1 proud. For parallax, and for a level's displacement |

Colours are authored and blended in sRGB, like a painter's. The renderer converts albedo and
glow to linear when it samples them.

## The bake and its cache: once per variant

`bake(recipe, ref)` paints the layers over float planes (colour, height, roughness, metal and
glow), then derives the maps. It is a pure function. The same recipe and reference give the
same bytes on every compiler, OS and worker count. `tests/golden/texgen_bake.txt` holds every
library recipe's digest, and a debug build matches a release build.

`bake_to_disk(recipe, ref, dir)` writes `<dir>/<name>-<key>_<map>.png` and then
`<dir>/<name>-<key>.meta`. The key is an FNV-1a 64 hash of the recipe's canonical text, the
reference's seed and the resolved params (the size is in the canonical text). The `.meta` is
written last, so finding it means every map before it is complete. **A variant is baked once:**
the next call with the same key reads the `.meta` and returns the files without painting
anything. Rooms that name the same variant share one set of files. Editing a recipe changes its
key, so a stale map is never served.

```
texture lava_rock
ref lava_rock seed=3 heat=2
key 9a7c7b9b1dae7dae
size 256
surface stone
scale 3
tags rock lava natural light
param heat 2
file lava_rock-9a7c7b9b1dae7dae_albedo.png
...
```

The PNGs are written with the labs' stored-deflate writer for now. Once the compressing
deflater from the charset work lands in `engine/lab`, the bake will use it.

## The command line

```sh
ge texture kinds                                     # every layer kind and key
ge texture check textures/*.tex                      # every mistake, at its line
ge texture bake textures/brick_wall.tex --ref="seed=12 wear=0.8" [--size=256] [--out=dir]
```

## The labs

| Lab | What it shows |
| --- | --- |
| `texgen-specimens` | Every library recipe lit, lit at 2x2, and its five maps. The seam score is the colour's step across the wrapped edge over the largest step across any other line. A texture that tiles scores at most 1, and the lab fails anything over 1.2 |
| `texgen-layers` | A recipe built one layer at a time and lit after each step, with how far each layer moved the colour and the height |
| `texgen-variants` | One recipe under four seeds (four rooms), with each param swept from its low end to its high end |
| `texgen-noise` | Every noise kind at one octave, at five octaves, and at five octaves warped, two tiles across, with its range and a periodicity check |
| `texgen-cost` | Each library recipe's bake time at 256, 512 and 1024 px, and each layer kind's cost |
| `texgen-compare` | Each recipe beside the reference texture it is steered toward, kept outside the tree: both two tiles across at the nearest pixel, ours lit, and both looks in numbers with their distance |
| `texgen-replace` | A level's textures, kept outside the tree, each beside the procedural replacement the likeness matcher picks, with the recipe, the distance and each texture's repeat; with no `--refs`, the library's own bakes tinted |

The lit preview (`preview.hpp`) shades the maps on the CPU. It uses one light from the top
left, ambient light scaled by the occlusion, a Blinn-Phong highlight sharpened by smoothness
and tinted by metal, and the glow added on top. It is close enough to judge relief, shine and
glow before the GPU path (M31b) exists.

## Speed

On the 4-core container, release build, a 512 px library recipe bakes in 55 to 165 ms
(`texgen-cost`). The heaviest kinds are the ones that sample several noise fields per texel
(`planks`, `rust`, `moss`, `snow`, `dirt`: 25 to 40 ms each at 512 px), and deriving the maps
takes about 17 ms. Each variant pays this once, the first time a level names it. M31e makes
the bake faster: float painters with vectorised hot loops, reused temporary planes, and several
variants baked side by side.

## Decisions

- **Bake on the CPU to files, not per frame on the GPU.** A texture is computed once and then
  only read. Files can be looked at, diffed, digested and shipped. The bake runs headless on
  the GPU-less container where every lab runs. A GPU path can later compute the same recipe if
  load time needs it, held to the CPU's bytes.
- **A line format, not a node graph.** An agent writes and edits lines well. A node graph
  needs an editor, and the engine has none (`docs/goals.md`, non-goals). Layers in order cover
  what a graph's common chains do, and `salt` shares one pattern between layers where a graph
  would share a node.
- **One table for every kind and key.** The parser, `ge texture kinds`, the canonical text and
  the docs all read it, so a key is added in one place.
- **Integer hashing for noise.** Hashing in integers is faster than hashing in doubles, and its
  bits are trivially portable. Fractional-period tiling was not taken, because it cannot be
  seamless.
- **Life layers read the height.** Placing aging by the structure, rather than by noise alone,
  is what makes a recipe look lived in. Each life layer takes one reading of the height
  (`relief` or `up_facing`) and one noise breakup.
