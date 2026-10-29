# Grass

Grass the engine grows blade by blade (`docs/goals.md` M34): short and long, thick and thin, on
a heightfield's ground or on any mesh's floors, swaying in the engine's wind (`engine/wind`,
`docs/water.md` § Wind). Any genre uses it: a summer meadow under a mountain, a lawn in a
level's courtyard, reeds round a lake, a field a player hides in.

| Part | Home | Lab | Test |
| --- | --- | --- | --- |
| Kinds and recipes | `engine/grass/kind.hpp` | `grass` | `tests/grass_kind_test.cpp` |
| Covers and the blade scatter | `engine/grass/cover.hpp` | `grass` | `tests/grass_cover_test.cpp` (golden) |
| A blade in the wind | `engine/grass/sway.hpp` | `grass` | `tests/grass_sway_test.cpp` (golden) |
| The grass drawn | `engine/render/grass.hpp`, `grass.wgsl`, `grass_scatter.wgsl`, `wgsl/grass_common.wgsl` | `gpu-grass` | `tests/render_grass_test.cpp` |

## Research: how real-time grass is drawn

What the current practice is, and what this engine takes from it.

- **Texture cards** (a quad with a picture of a tuft, alpha-tested) are the old answer: cheap,
  but they read as flat sheets up close, sway as rigid sheets, and alpha testing aliases and
  overdraws. Not taken except as the far ground's colour.
- **Geometry blades** are what current open-world games draw near the camera: every blade its
  own few triangles, generated on the GPU so nothing per blade is stored. Two published designs
  shaped this one:
  - A per-blade physical model (Jahrmann and Wimmer, "Responsive Real-Time Grass Rendering for
    General 3D Scenes", I3D 2017): a blade is a quadratic Bézier from its root through a guide
    point to its tip; forces (gravity, wind, a recovery toward rest, collisions) move the tip;
    the guide is placed from the tip's height, and then both are scaled so the curve's length
    stays the blade's length. Blades may stand on any mesh, not only a heightfield. Culling by
    orientation, frustum, distance and occlusion keeps only what matters.
  - GPU-generated fields with clumps (a 2021 talk on procedural grass in a large open-world
    game): blades generated per tile every frame by a compute pass, each with its own look
    hashed from its position; blades grouped into clumps (a Voronoi of jittered points) that
    share height and lean so the field reads as grass and not fur; a blade seen edge-on widened
    toward the camera so it never thins to a thread; a low-detail blade at distance; wind as a
    noise blown across the field.
- **Wind** in shipping games is usually a scrolling noise. Here it is the engine's own wind, so
  a rotor's downwash or a blast flattens the grass where the simulation says it blows.

## Kinds

A kind (`grass::Kind`) is a handful of numbers: the blade's height and how much it varies, its
width at the root, the blades a square metre (`density`), the rest lean, the clumps (`clump`:
their size; `pull`: how far a blade leans toward its clump's middle), the wind's `give` (the
speed at the blade's height that bends it halfway) and its own `sway` frequency, and the colours
(root, tip, a `dry` share turned to `dry_tip`), how long a trampled blade takes to stand up
(`spring`), and its **heads**: a `flowers` share of blades topped by a cup `flower_size` m
across in `flower` (or, for `flower2_share` of them, `flower2`), and a `seeds` share whose top
`seed_length` of the blade swells into a seed head in `seed`. Tall or short is `height`, thick or
thin is `density` and `width`.

| Preset | Height | Width | Blades/m² | Give | Sway | Spring | Heads | What it is |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `lawn` | 0.07 m | 3 mm | 900 | 11 m/s | 2.8 Hz | 3 s | none | short, dense, stiff |
| `meadow` | 0.35 m | 5 mm | 400 | 6 m/s | 1.6 Hz | 8 s | 6 % yellow and white flowers, 6 % seed | shin high, some dry |
| `long` | 0.85 m | 7 mm | 220 | 3.5 m/s | 0.9 Hz | 25 s | 2 % purple and white flowers, 20 % seed | knee to thigh high, soft |
| `tussock` | 0.5 m | 4 mm | 450 | 5 m/s | 1.3 Hz | 12 s | 1 % flowers, 12 % seed | tufts, strongly clumped |
| `steppe` | 0.45 m | 2.5 mm | 300 | 7 m/s | 1.4 Hz | 15 s | 30 % straw seed | thin, wiry, mostly dry |
| `reeds` | 1.6 m | 12 mm | 90 | 9 m/s | 0.6 Hz | 5 s | 35 % dark plumes | tall, stiff, sparse |

A **recipe** is a preset's name and overrides, the form a level file and the command line
write: `meadow height=0.6 density=300 width=0.004 dry=0.2`. `parse_kind` reads one and names
every fault (an unknown preset or field, a value that is not a number); `print_kind` writes the
canonical recipe back.

## Cover

A cover (`grass::Cover`) is a list of triangles, each with its kind and the share of the kind's
density at each corner. It comes from:

- **a heightfield** (`cover_heightfield`): a region at a cell of its own, each corner's kind and
  share asked of the game (nothing on a path, long grass in a hollow);
- **any mesh** (`add_mesh`): every triangle whose normal is within `acos(min_up)` of vertical,
  either winding. This is the hook for a level's floors: the room format's grass material and a
  classic map's grass-textured faces (`docs/levels.md`), grown by `rooms::level_grass` and
  `grow_on_brushes` / `grow_on_rooms` (`plugins/rooms/grass.hpp`, below).

**A level's grass** (M34d). A room level's material grows grass when its surface reference starts
with the word `grass`: `surface turf grass meadow height=0.6 density=300` in a level's text, or
`surface grass1 grass lawn` in a cloned map's texture map (a bare `grass` is a meadow). What
follows is a recipe as `parse_kind` reads it. Materials with the same recipe share one kind; a
recipe that does not read, or a ninth kind, is a fault naming the material and the fix, and that
material grows nothing. The cover takes every drawn, solid brush face of such a material whose
outward normal is at least 0.8 up (`kFloorUp`, 37 degrees off level), and every grown room
triangle whose three corners wear it and whose normals into the room are as far up, so a floor
and a ramp grow and a wall, a ceiling or a bank of the same material stay bare. The cover's seed
is the rooms' seeds mixed. The `rooms` lab's `vault-grass` view and the `clone` lab's `--grass`
(the fixture's tiled side room a lawn by default) picture it; `tests/rooms_grass_test.cpp` holds
it.

A triangle whose blades would pass half of `kMaxBladesPerTri` is halved across its longest edge
until they fit, so a large floor is never capped.

**The scatter** is a pure function of the triangle: its seed is its corners at a centimetre,
hashed (so the same ground grows the same blades in any order); it holds `floor(A × D × S)`
blades and one more by its seed's chance; each is a uniform point (the square-root fold of two
hashed numbers), kept by the share at that point; its clump is the nearest jittered centre of
the clump grid, which scales its height and pulls its lean. Each blade has a **rank** in [0, 1),
which is what the picture thins it by: the lowest ranks are kept, so thinning never reshuffles.
No transcendental is called and every float is in the GPU shader's order, so the CPU knows every
blade the GPU draws.

## Sway

A blade's shape is a pure function of its kind, its numbers, the air at its root and the clock
(`blade_shape`, and its WGSL twin):

- **The air**: the engine's wind (`wind::wind_at` at the blades' height, with the field's
  shelter and every local source) sampled on a grid round the eye (`fill_wind_grid`). The gusts
  are **carried downwind**: a node reads the field's air at `t − (its distance along the mean
  bearing) / (the mean speed)`, so a gust crosses a field as a front. The grid is read
  bilinearly and ruffled by a 6 m value noise blown with the wind: the cat's-paws.
- **The bend**: `b = w² / (w² + give²)` (the drag grows with the square of the speed, the stiffness
  pushes back). The rest lean gives way to the wind's bearing as `b` grows, to at most 1.35 rad.
- **The sway**: a sine at the kind's frequency (±15 %, each blade's phase its own), largest in a
  middling wind and least in a calm or a gale that holds the blade flat, with a smaller sway
  across the wind.
- **The curve**: the tip from the bend, the guide straight up from the root to `h × max(1 − sin θ,
  0.05)`, then both scaled so the Bézier's length (between its chord and its guide polygon) is
  the blade's length.

## Trampling

`engine/grass/trample.hpp`: grass pressed down by whatever walks, rolls or lands on it, standing
back up over its kind's `spring` (a lawn in 3 s, a meadow in 8 s, long grass in 25 s).

- **The cells**: `n × n` cells of `cell` m (256 × 0.25 m by default) that wrap round the world. A
  cell is found by its world column and row modulo `n` and keeps its key, so a stale cell reads as
  unpressed and a trail can be laid anywhere; where two places share a slot the newest print wins.
  Each cell is 16 bytes: when it was pressed, how hard (0..1) and the plan way its blades lie (two
  snorm16s).
- **A stamp** (`stamp`): a disc of `radius` m pressed flat over its middle 60 % and falling off to
  its rim (`1.6 (1 − (d / r)²)`, clamped to 1), its blades laid along the stamper's way of travel
  and splayed outward from the middle. A cell keeps the harder of its old press and the new one.
  A game stamps a foot, a wheel, a body or a landing; nothing else writes the cells.
- **Standing back up** (`press_at`): `amount × r²` with `r = 1 − age / spring`, read bilinearly
  between the four cells round a blade's root. Nothing is stepped: the press is a function of the
  cells and the clock, so a replay or a seek draws the same trail.
- **The blade**: pressed by `p`, it gives up that share of its lean and sway to lie along the
  press's way, 1.45 rad off the vertical at `p = 1`. Drawn, it lies on top of the field, so it is
  lit to its root and shows its paler back: a trodden line reads lighter than the field round it.

`render::set_grass_trample` uploads the cells a stamp changed (the dirty range) before a frame;
the scatter's probe and the draw read the same cells as the CPU, held to them by
`render_grass_test`.

## Drawn

`engine/render/grass.hpp`, two passes:

1. **`grass.scatter`** (compute): one workgroup a cover triangle, culled by the frustum (its
   sphere raised by its kind's tallest blade) and the reach; each thread takes every 64th slot,
   scatters the blade, keeps it by rank against the distance's share (all within `full_m`,
   `(full_m / d)²` past it, none past `reach_m`), culls it by its own sphere, shrinks it to
   nothing near its rank's threshold and near the reach (no popping) and widens the fewer far
   blades. It is appended to the fine list (within `near_m`, 5 segments) or the coarse list (2
   segments), each an indirect draw held to its capacity.
2. **`grass`** (draw, after the ground's colour and before the sky, depth written): one triangle
   strip a blade; each vertex bends its blade in the air at its root and steps out by the width,
   tapering to the tip, never under about 0.7 px, turned toward the eye when edge-on. Lit
   two-sided: the sun past the ground's horizon and the shadow maps on the face toward the eye,
   the sun through the blade from behind (translucency, strongest at the tip), the sky dimmed
   toward the root (a dense field shades its own floor from the sky), a faint sheen; hazed.
   **The field's own shade**: the sun reaching a point of a blade has crossed the canopy above it,
   `exp(−0.5 · lad · L)` of it getting through, where `lad` is the kind's blade area a cubic
   metre (density × width: 2.7 for a lawn, 1.5 for long grass), the 0.5 the share of a blade
   facing the sun's way, and `L` the path along the sun from the canopy's top (the blade's own
   tip, so a field laid over by a gale or trampled flat is lit deeper) down to the point. A high
   sun lights long grass halfway down; a sun 8° up lights only its tips, the rest glowing
   through from behind (the light through a blade is scattered forward, so the canopy dims it
   by the square root of that). The fine blades
   bend again at the last frame's time and write their own motion, so temporal anti-aliasing keeps
   a waving field sharp; the coarse ones move by less than a pixel and carry the camera's.

**Why the blades do not cast into the shadow maps.** The sun's cascades are cached and redrawn
only when the eye drifts or the sun turns (`render/shadows.hpp`); a swaying field would have to
redraw the near cascade every frame, and draw hundreds of thousands of blades into it, to put on
the ground a shadow the blades themselves hide. The canopy's shade above gives the field its
depth for the price of an exponential a vertex. What stands on the grass (trees, figures) does
cast onto it: the blades read the shadow maps.

**Far away** there are no blades: the game paints the ground's albedo under a cover with each
kind's `far_colour` (the blades' mean as seen from afar, darkened by the field's own shade), so
the field thins into a ground of its own colour with no ring.

On the software adapter the `gpu-grass` lab's ten views (about 130,000 blades each at 480 ×
270) take about 11 s together.

## Rows

| Row | What | Status |
| --- | --- | --- |
| M34a | Kinds, covers, the scatter and the sway on the CPU; the `grass` lab | done |
| M34b | The grass drawn: the scatter pass, thinning, the blades, the light; the `gpu-grass` lab | done |
| M34c | The rest of the look: blades pressed down by what walks or drives through them and springing back (a trample map), the canopy's own shade from the sun, flowers and seed heads, the sway's own motion vectors, a stop on the picture's quality ladder priced | in progress: trampling, motion vectors, the canopy's shade, flowers and seed heads built |
| M34d | A level's grass: a room format material's `surface <material> grass <recipe>` line and a cloned classic map's texture-map `surface <texture> grass <recipe>` rows grown into covers (`plugins/rooms/grass.hpp`, with M32): each recipe one kind, a brush face or a grown room's floor at least 37 degrees off a wall grown, hidden, clip and liquid faces never | done |
| M34e | Fall Line's summer grass: the summer ground's meadows, pastures and runs as covers, the rotor's downwash flattening them | open |
