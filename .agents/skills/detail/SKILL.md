---
name: detail
description: "Use for engine/detail in gameengine2 and anything about dressing a level built of brushes: convex solids as planes and their faces, bevels and chamfers on sharp brush edges (which edges are buried, thin or flat), the receivers grid (box queries, raycasts, buried faces), decals (scorch marks and their embers, bullet holes, blood that dries and fades; the masks, the atlas, the clip, the pool), the marks a game's events leave (shot_hit: a bullet hole where a round lands; bleed: blood by the health a hit took), seeded detail props (rubble, debris, moss; the rules, the damp field, the scatter, the shapes), and floor clutter by surface class and open sky (a texture's class by its name and colour; grass, pebbles, rocks, dirt, twigs, branches, shells, bones; clutter_rules); the test hall and yard and the detail-bevel, detail-decals, detail-props, detail-hits and detail-clutter labs; the M42 rows."
---

# Brush detail

`engine/detail/` (`ge_detail`, namespace `ge::detail`) dresses a level built of brushes for any
game: bevels on exposed edges, decals that age, props by rules. The design is
[`docs/detail.md`](../../../docs/detail.md); the rows are `docs/goals.md` M42–M42f. It is
simulation code: libm only, no allocation in a decal's placement after the pool is made.

| File | What |
| --- | --- |
| `detail/vec.hpp` | `Vec3` arithmetic, `ramp` (a Hermite ease either way round) |
| `detail/mesh.hpp` | `TriMesh`: the triangles every pass reads, one tag a triangle |
| `detail/solid.hpp` | `Solid` as planes; `polygons_of` (the clip), `box_solid`, `prism_solid`, `triangulate` |
| `detail/bevel.hpp` | `bevel`, `bevel_all`, the edge verdicts, `SolidIndex` |
| `detail/receivers.hpp` | `bin_receivers`, `triangles_in`, `raycast`, `buried_at` |
| `detail/decals.hpp` | kinds, masks, looks by age, `bake_atlas`, the pool, `place_decal`, `expire` |
| `detail/hits.hpp` | `shot_hit` (a round's hole where its ray lands), `bleed` (blood by the share of health lost) |
| `detail/props.hpp` | `PropRule`, `scatter_props`, `default_rules` |
| `detail/prop_shapes.hpp` | `prop_mesh`, `prop_frame` |
| `detail/test_hall.hpp` | the brush-built hall the tests and labs share, and its damp field |

## Rules

- **A bevel only adds planes.** Never edit a solid's faces by hand: add the round's tangent planes
  and let `polygons_of` find the faces. That keeps the solid convex and the corners right.
- **Sharp where it is buried.** An edge is cut only when it is exposed; the exposure is asked of
  the caller (`SolidAt`) or of every other solid (`bevel_all`). A new probe or margin moves
  `tests/golden/detail_bevel.txt` and the `detail-bevel` lab's verdict picture: look at both.
- **Brush levels keep hidden faces.** A floor's top runs on under the walls standing on it. A pass
  that writes onto surfaces asks `buried_at` before it spends anything there.
- **Masks are functions.** A mask is `mask_at(kind, variant, u, v)`, pure; anything that draws it
  (an atlas, a lab) samples it. Keep it inside its circle (`r < 0.5`): the box clips anything
  past it with a straight edge. The box's depth (`depth_share`) must reach as far along the
  normal as the mask does across a corner, or the mark is cut off in a line.
- **The rules are data.** A dressing is a list of `PropRule`; a behaviour no field gives is a new
  field with its unit, never a branch on a kind. What a floor is made of is a class the caller
  hands over (`ScatterEnv::class_of_tag`), never a texture name read inside the scatter.

## Changing a look

1. Run the lab before: `make lab LAB=detail-decals ARGS=--tag=before` (or `detail-bevel`,
   `detail-props`, `detail-hits`, `detail-clutter`).
2. Change the mask, the look, the rule or the bevel's numbers.
3. Run it again with `--compare=before`, look at every picture, and rewrite the golden the change
   moves (`GE_GOLDEN_WRITE=1 build/release/tests/detail_<name>_test`).
4. Rewrite the lab's smoke digest (`build/release/labs <lab> --smoke --write-digests=tests/labs_digests.txt`), and put both runs' pictures in the PR.

## Lessons

Read them first: `make lessons ARGS=detail`.
