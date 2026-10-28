# Brush detail: bevels, decals and props

A level built of brushes (boxes, wedges and prisms laid flush) reads as boxes: every edge is a
perfect line, every surface clean, every floor empty. `engine/detail` (`ge::detail`, the `detail`
skill) adds what wear and use leave, for any game whose levels are made of brushes, in three
passes that share one vocabulary (`Solid`, `TriMesh`, `Receivers`). The rows are
[`goals.md`](goals.md) M42–M42d.

| Pass | Reads | Writes | When |
| --- | --- | --- | --- |
| Bevels (`bevel.hpp`) | a level's convex solids | the same solids with planes added | once, at load |
| Decals (`decals.hpp`) | the level's triangles (`Receivers`) and a game's events | small meshes in a fixed pool | at run time, as things happen |
| Props (`props.hpp`) | the level's triangles, rules and a seed | a list of placed props | once, at load |

All three are simulation code (deterministic, libm only): the same level and seed give the same
bevels and props, and the same run's events the same decals, on every compiler.

## Solids and their faces

A `Solid` is a list of planes, the points inside all of them; that is what a brush is in every
brush-built format. Its faces are not stored: `polygons_of` clips a large square on each plane by
every other plane, and a plane the others cut away has no face. `triangulate` fans the faces into
a `TriMesh` (positions, indices, one tag a triangle) whose winding is the engine's: a triangle's
front is the side `(b - a) x (c - a)` points to.

## Bevels

A real arris is never a line: stone is dressed, timber planed, concrete cast against a chamfer
strip. The bevel pass cuts every exposed sharp edge back into a round of `segments` facets
(one facet: a chamfer) of `radius_m`. It does it by ADDING planes, each tangent to the round's
circle tucked into the edge, and lets the clip find the faces again, so where three bevels meet at
a corner the corner's facets come out with no special case and the solid stays convex.

Every edge gets a verdict:

| Verdict | When | Why |
| --- | --- | --- |
| `bevelled` | none of the below | |
| `flat` | the faces turn by less than `min_turn_deg` (25) | an octagon's 45 degree folds may be rounded; a 10 degree fold is a surface, not an edge |
| `knife` | they turn by more than `max_turn_deg` (165) | the round would eat the solid |
| `thin` | either face is too shallow for a quarter of the radius at `max_share` of its depth | a slab's rim |
| `buried` | a neighbour solid holds a probe just off either face near the edge, or round the edge | the edge is inside the level's union: two blocks of one wall, a wall on its floor, a step on a step; a cut there opens a groove |

The set-back along each face is `radius * tan(turn / 2)` (the radius at a right angle). The
exposure probes sit at five points along the edge, half a set-back in from it on each face and a
quarter of the radius (2 to 20 mm) out; `bevel_all` answers them from a grid of every solid's box
(`SolidIndex`), so a level of thousands of brushes is bevelled in one pass. A bevel plane takes
the tag of the steeper of its two faces, so a floor's material does not wrap down a step's nose.

The test hall's 23 brushes have 300 edges: 111 are rounded and 189 buried (`make lab
LAB=detail-bevel` draws every edge in its verdict's colour). Rounding them at three facets takes
the hall from 308 triangles to 1372, a cost paid once at load.

## Receivers

Decals and props ask the level's triangles two questions: which triangles are in this box, and
what is the nearest triangle along this ray. `bin_receivers` bins a `TriMesh` once into a grid of
1 m cells; `triangles_in` and `raycast` answer into a caller's scratch, so a frame that places a
decal allocates nothing. `buried_at` says whether a point on a surface is inside a solid: a level
of brushes keeps the faces one brush hides of another (a floor's top under the wall standing on
it), and the first surface met along such a face's normal is met from behind.

## Decals

A decal is a box round a point: `size_m` square across the surface and `depth_share` of that deep
along its normal. Every receiver triangle in the box that faces the decal's way within the kind's
`max_angle_deg` is clipped to the box (Sutherland-Hodgman, six planes), lifted a millimetre off
its surface, and given its (u, v) across the box and a `fade` that thins it on triangles turned
away. A piece wholly on buried faces is dropped. So a scorch at a wall's foot wraps onto the
floor, blood runs over a step's rounded nose, and nothing floats past an edge.

| Kind | Mask (`mask_at`) | Look (`look_at`) | Life |
| --- | --- | --- | --- |
| `scorch` | a ragged burnt core, tongues thrown out by the blast, a soot halo; embers in patches of the core and along the tongues | near-black, matte; its embers glow white-orange at the blast and cool through red, out by 1.5 s (`ember_s`) | kept |
| `bullet_hole` | a dark hole, a crater to a pale rim, three to six cracks, a halo of dust | dark grey | kept |
| `blood` | a splash with a rim, a run one way, six to ten droplets | wet red, drying to matte brown over 45 s | fades over the last 60 s of 120 s |

The masks are pure functions of the kind, a variant number and (u, v); a renderer bakes them once
(`bake_atlas`: cover in red and alpha, the relief in green, the embers in blue), a CPU lab reads
them directly. A look's glow (`er`, `eg`, `eb`, linear and above 1 when hot) times the mask's
`ember` is light the mark gives off: a renderer adds it after lighting, and a light pass may cast
it. The
POOL is made once (`make_pool(capacity, verts_per_decal)`): a decal takes a free slot, else the
oldest one's; one that needs more vertices than a slot holds keeps the triangles nearest its
centre. `expire` frees what is past its life. Every number of a kind is in `decal_spec`.

## Marks from a game's events

`detail/hits.hpp` is what a game calls when a fight happens; the game owns the event (its round,
its damage, its seed), this module where the marks go and how big they are. Neither call draws
from anything but a `core::Rng` made of the event's seed.

- `shot_hit(pool, receivers, ShotAt{from, dir, far_m, size_m, now_s, seed})` casts the round's
  ray and presses a bullet hole where it lands, facing back toward the shooter from whichever side
  of the surface the round came; a round into the open presses nothing. A shotgun calls it once a
  pellet.
- `bleed(pool, receivers, WoundAt{at, shot_dir, health_lost, max_health, now_s, seed})` scales
  everything by the share of health the hit took (`s`, 0..1, clamped):

| What | How many | Where | Size (decal across) |
| --- | --- | --- | --- |
| spatter | 1 + round(5 s) | cast on along the shot in a cone of 0.3 + 0.45 s rad, drooping, within 0.8 + 2.7 m s: the wall or floor behind the victim | (0.25 + 0.75 s) m, times 0.7 to 1.1 |
| the pool | 1 | straight down from the wound | (0.35 + 1.75 s) m |
| drops | round(4 s) | down from round the wound, 0.15 to 0.5 + 0.5 s m out | 0.12 to 0.22 m |

So a twentieth of the bar leaves a spot under the victim, a quarter a pool and a mark on the wall,
the whole bar paints the wall behind and leaves a pool about a metre across. A game keeps blood,
holes and scorches in pools of their own when one kind must not push out another.

## Props

A prop is a small shape (a lump of rubble, a shard or a splinter of debris, a cushion of moss)
standing on a surface. Nothing is placed by hand and nothing is stored: RULES are scattered over
the level off a seed.

- A `PropRule` names a kind, the surfaces it may stand on (tags, floors or walls), a density per
  square metre where its weight is 1, and the weight: rising to 1 near the OTHER surface (a
  floor's walls, found by eight level probes; a wall's floor, by one probe down) over `near_m`,
  `open_weight` beyond it, a caller's damp field adding `damp_weight` (and a rule that lives only
  in the wet keeping `dry_share` of the rest where it is dry), gathered into heaps by a low noise.
- The scatter draws each triangle's count off its area with a stream of its own (editing one room
  moves no prop in another), keeps a sample by its weight, drops one on a buried face, and drops
  any closer than `spacing_m` to a kept prop of the same rule.
- `default_rules` dresses a stone-and-timber interior: rubble heaped at walls' feet, debris thin
  over floors, moss on damp floors and low on damp walls. A game passes its own rules.

`prop_mesh` builds each kind's variants in code (faceted, a unit across, base at y 0);
`prop_frame` turns a prop's up, yaw and size into its frame.

## Labs

| Lab | Shows |
| --- | --- |
| `detail-bevel` | the hall sharp and bevelled; five corners sharp, chamfered and rounded; every edge by its verdict |
| `detail-decals` | every kind's masks; the hall after a fire fight; marks wrapping corners; blood drying and fading; the decals' own triangles |
| `detail-props` | every shape; the hall bare and dressed; the plan; close-ups; all three passes together |
| `detail-hits` | shotgun blasts and a rifle's burst with every round's ray; one victim's blood at four shares of health lost; a scorch's embers cooling |

They draw on the CPU (`engine/lab/labs/detail_view.hpp`: per-pixel light with a key, a warm lamp,
a sky fill, a glint and a screen-space occlusion), so they run in the GPU-less container and every
pixel is in the lab's digest. The test hall they share is `detail/test_hall.hpp`.

## Not yet (M42e, M42f)

M42e: the GPU frame's decal pass (the atlas, the pool's vertices, the embers' glow) and the props
as instances. M42f: the passes applied to the room format's and the BSP families' levels on load,
with a per-material bevel radius and a no-props flag; receivers a BSP model each (the world binned
once, a moving door's or platform's carried by its transform, so a mark rides with it); and the
callers of `shot_hit` and `bleed`, which are a game's code: Shooter's Edge's (M18) and, for the
shareware maps, the temp-entity messages of a demo played back.
