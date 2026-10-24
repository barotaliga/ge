---
name: texture-references
description: "Use to GROW engine/texgen's techniques by studying reference textures as inspiration: another game's wall and floor textures, a material photograph, a texture set's look. The loop: fetch the references into scratch (never the tree), sheet and study them, name the techniques the layer kinds lack (grain, baked light, palettes, patterns, wear), build them as layer kinds or keys, write recipes steered toward the references with texgen-compare (side by side, the look in numbers: brightness, contrast, saturation, warmth, grain, edges, colours), publish the comparison sheets, and delete the references. No pixel is copied and no source is named in the tree; M31d."
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
