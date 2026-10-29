---
name: texture-references
description: "Use to MATCH a converted map's textures to recipes with ge texture match (the argued loop: class, traits, clone, judge, in the .texmap) and to GROW engine/texgen's techniques by studying reference textures as inspiration: another game's wall and floor textures, a material photograph, a texture set's look. The loop: fetch the references into scratch (never the tree), sheet and study them, name the techniques the layer kinds lack (grain, baked light, palettes, patterns, wear), build them as layer kinds or keys, write recipes steered toward the references with texgen-compare (side by side, the look in numbers: brightness, contrast, saturation, warmth, grain, edges, colours), publish the comparison sheets, and delete the references. No pixel is copied and no source is named in the tree; M31d."
---

# Gather techniques from reference textures

The engine's textures are recipes an agent writes (`textures` skill). This skill is how the
recipes and the layer kinds grow: look hard at textures someone else painted, work out HOW they
get their look (the grain in every texel, the light painted into the edges, the few-colour
ramps, the way stone is cut), build that technique into `engine/texgen`, and steer a recipe
toward each reference until it reads as the same kind of surface. It works for any source:
another game's textures, a photograph, a material library.

Load `textures` and `labs` beside it. Read the lessons first: `make lessons
ARGS=texture-references`.

## The rules that make it inspiration, not copying

- **References live in the scratchpad for one session, never in the tree.** Not in a commit, a
  test fixture, a golden, `porting/` or a lab's default. Delete them at the end.
- **No pixel crosses over.** A recipe is text an agent writes after looking: layer kinds, keys,
  colours chosen by eye or from the look's numbers. Nothing samples, traces, stamps or embeds a
  reference's pixels, and no code reads a reference except `texgen-compare`, which only draws it
  beside the bake and measures it.
- **Name no source in the tree.** Recipes, comments, docs, lessons, commits and PR text describe
  the MATERIAL ("dark iron plate with heavy bronze bolts"), never the game, the file or the
  texture's name it was studied from. The pairing of a recipe with its reference lives only in
  the lab's command line and the session's notes. `make agents-check` scans for the classic
  game's names; the rule is wider than the scan.
- **Comparison pictures go on a private Artifact page for review**, never into the tree.

## The loop

1. **Fetch** the references into `<scratchpad>/refs/` as PNGs, one per texture, named by the
   source's own stem. Get them from wherever the person points (an archive they own, a public
   shareware release, a folder of photographs), with a throwaway script in the scratchpad; a
   level format the engine reads (M32) can be the extractor once it exists. Keep only what you
   need to look at.
2. **Sheet them**: a contact sheet of everything (eighty to a page, names under each), then a
   focus sheet of the ten or so you will chase, at four times their size with the nearest pixel.
   Look at the sheets (read the PNGs).
3. **Name the techniques.** For each focus texture write down what makes it read: the base
   colour and its ramp, the grain (per texel? clumped? coloured specks?), the structure
   (blocks, panels, studs, scales, cracks), where the light is painted (lit upper-left edges,
   shadowed joints), the palette (how many shades, how banded), the wear. Map each to a layer
   kind or key (`ge texture kinds`). What has no kind is the work.
4. **Build what is missing** in `engine/texgen` (the `textures` skill's "Add a layer kind"),
   each technique general enough for any recipe: a kind in its group (`pattern` for structure,
   `life` for aging, `finish` for the paint over everything), keys in the table, tiling held.
5. **Write a recipe per focus texture**, named by its material, at the reference's size (64 or
   128 for an old game's walls; the recipe still bakes at any size a level asks for).
6. **Compare and iterate**:
   `make lab LAB=texgen-compare ARGS="--refs=<scratchpad>/refs --pairs=<recipe>:<ref>,..."`.
   Look at each sheet (REFERENCE, OURS, OURS LIT, two tiles across) and the table: brightness,
   contrast, saturation, warmth, grain, edges, colours, and the look distance. Change one
   thing, rerun. Stop when it reads as the same kind of surface at a glance and the distance is
   under about 0.5; a lower number is no goal in itself, the eye is.
7. **Publish** the comparison sheets (and the before and after of each new kind) on one
   Artifact page and link it from the PR and the report.
8. **Land** the kinds, the recipes, the goldens (`texgen_bake_test`) and the lab digests in one
   PR (`textures`'s "Change a look" for the gates). Add each technique you built to
   `docs/textures.md`'s techniques table, described by what it does.
9. **Delete the references** and the sheets from the scratchpad. Record what you learned
   (`skill-reflection`): which numbers mattered, which technique carried the look.

## Reading the numbers

| Number | What it says | Usual fix when ours is off |
| --- | --- | --- |
| brightness | mean luma | the base colour, the palette's ramp |
| contrast | spread of the luma | noise amount, shade strength, occlusion |
| saturation | mean chroma | the colours; a palette pulls toward its ramps |
| warmth | red minus blue | hue of the base and the grain's specks |
| grain | per-texel step against the neighbours | `speckle` contrast and size |
| edges | share of texels with a hard step | structure: joints, studs, cracks; `shade` |
| colours | distinct colours over texels | `palette` steps and dither (few shades: low) |

## Matching a map's textures: the argued loop

A map converted from another game may keep its structure but never its painted textures: each
one becomes a recipe reference in the map's `<map>.texmap` (`engine/texgen/texture_map.hpp`),
which the converter reads. `ge texture match <dir> --map=<map>.texmap` picks a first reference
for every texture, but **a pick is not a match until the agent has argued it**. A look distance
rates a wall of small running-bond blocks close to a wall of large irregular ones; only an
argument about what the texture IS catches that. Per texture, in the texmap:

1. **`class <name> <class>`: what it is.** One of the recipe tags: `bricks`, `blocks` (cut
   stone), `flagstones`, `tiles`, `planks`, `panels`, `plate`, `metal`, `rock`, `plain`,
   `scales`, `stripes`, `logo` (any mark someone owns: never cloned, see the table), or a new one (then a recipe must carry it). The candidates narrow to
   recipes tagged with it.
2. **`traits <name> <description>`: how a texture of that class looks, and this one.** Name the
   traits that make the class read, then this texture's value for each (the table below). Write
   numbers you can check on the sheet: "4 courses a tile, 2 to 3 blocks a course".
3. **The clone: the `texture` line.** Pick a candidate, then turn its knobs (`rows`, `cols`,
   params, the seed, the tint) until the traits hold. End the line with `# pin`. When no recipe
   can show a trait (blocks that span two courses, rounded corners), the technique is missing:
   build it (the loop above) before pinning.
4. **`judge <name> pass|fail <reason>`: the clone held against the traits, trait by trait.**
   Read the sheet (`<dir>/sheets/<name>.png`: the original, ours, the candidates; under them
   both textures' evidence and every MISMATCH ge measured). A mismatch ge prints (the pattern
   repeats 4 times down the original and 6 down ours, the lines run the other way, half the
   contrast) is answered in the judge line or fixed, never ignored. Pass only when every trait
   holds; a fail stays visible at the top of every run until it is fixed.

5. **Seams: the 3x3 panels.** Every sheet tiles the original and the clone 3x3. Look for a
   line where the tile meets itself: a band cut by the edge, a gradient that does not wrap, a
   lit edge on one side only. ge measures it too (`seams` in the evidence) and asks when ours
   has a hard edge the original lacks; a plate's own rim on the border is not a seam, so say
   which it is in the judge line.
6. **Clusters: textures that meet.** Group the textures a map shows together, in the order they
   meet: a wall and its trim, a floor and its skirting, a family of variants (`cluster red_wall
   citya1_1 city5_4 city5_7`). ge draws `sheets/cluster_<name>.png` (the originals side by side,
   each tiled 2x2, the clones under them) and asks where the set comes apart: one member lighter
   than another in the originals and darker in ours, a family whose brightness spread is twice
   or half the originals', a join that jumps where the originals' does not. Judge the set with
   `judge <cluster> pass|fail ...`: the courses and bands line up across the joins, the family
   reads as one material, and no two members are the same tile copied (give each its own seed).

Run `ge texture match` again after every change: unpinned lines are re-picked (inside the
class), pinned ones are re-measured, and the sheets are redrawn, failed and unargued first. The
texmap is done when every texture line is pinned and judged pass. Liquids and skies are named
for their renderers and need no argument.

| Class | The traits to argue | Usual knobs |
| --- | --- | --- |
| bricks | the bond (running, stack, Flemish), courses a tile, the brick's length to height, mortar width and colour against the brick, each brick's own shade | `bricks rows= cols= offset= mortar= mortar_color= vary=`, `palette`, the tint |
| blocks (cut stone) | regular courses or irregular (big and small mixed, blocks spanning courses), courses a tile, the blocks' size spread, edge rounding, the joints' darkness | `blocks rows= cols= vary_size= span= bevel= chip=`, `shade` |
| flagstones | the stones' size and spread, polygonal or rounded, the gaps' width | `cells`, `voronoi`, `shade` |
| tiles | square or offset, tiles a tile, grout width and colour, glaze | `tiles rows= cols= mortar= mortar_color=` |
| planks | the boards' direction, boards a tile, their ends, the grain's run, gaps | `planks`, `noise stretch=` |
| panels | panels a tile, seams (pressed, gaps), rivets (where, how many, how big) | `panels rows= cols= rivet_*` |
| plate | the stud pattern (domes, diamonds, holes), the studs' pitch a tile | `studs rows= cols= shape=` |
| metal | flat or brushed, the grain, specks, stains and their colour | `speckle`, `noise stretch=`, `rust`, `grime` |
| rock | chunk size, cracks, how lit the faces are | `noise kind=edges`, `cells`, `shade` |

| logo, trademark, emblem | **never cloned.** A game's mark, a maker's logo, a wordmark or any emblem someone owns is not copied, not even as a lookalike: the texture becomes a blank recipe (tagged `blank`, e.g. `blank_plate`) recoloured to the surface around the mark. Argue what the place needs (a plate, a panel, a slab) and judge that the mark is gone | the tint, `blank_plate`'s params; a new `blank` recipe for another surface |

With a mark's class, ge draws the candidates from the `blank` recipes only, skips the likeness
mismatches (a blank is meant to differ) and warns when a pinned line names a recipe that is
not blank. When in doubt whether a picture is a mark (a letter cast in metal, a symbol on a
door), treat it as one.

Lights, signs and tool textures (a lamp's grille, a face, a trigger volume) have no class to
match yet: say so in the traits, judge them fail, and leave them for a technique of their own.
