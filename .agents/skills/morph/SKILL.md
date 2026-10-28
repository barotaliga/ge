---
name: morph
description: "Use for plugins/morph in gameengine2: classic vertex-animated models and sprites read into one view (skins and their groups, the seam split, triangles, every frame as a morph target with its group and interval; sprites' frames and facing), refusing damage; the fixture writers; per-frame smooth normals; the optional point-normal rounding; the morph lab."
---

# The morph plugin

`plugins/morph/` (`ge_morph`, namespace `ge::morph`) reads the classic vertex-animated model and
sprite files into one view a game or a pipeline can use. It depends on the engine's core only;
its lab reaches `assets` and `lab` for the model view:
`ge_add_plugin(morph ENGINE core APP assets lab)`. It reads files; it never ships a classic
asset. The player's own pack is read by `plugins/classicenhance`, into their cache.

| File | What |
| --- | --- |
| `morph/model.hpp` | The view, stated once: `Skin`, `Frame`, `Model` (vertices split on the seam so each has one position a frame and one `st`, counter-clockwise triangles, `source_vertex`), `Sprite`, `SpriteFrame`, `Facing` |
| `morph/read.hpp/.cpp` | `read_model` and `read_sprite`: a checked cursor; every count and size held to a range before anything is read; a refusal names the file, what was wrong and the next step |
| `morph/write.hpp/.cpp` | `write_model`, `write_sprite` (fixture files that read back into the same view, positions to the packing's 1/255) and the fixtures `sample_model`, `sample_sprite` |
| `morph/smooth.hpp/.cpp` | `frame_normals` (area-weighted, summed over a position's seam copies); `smooth_model` (point-normal triangles, `level²` a triangle, creases kept straight) |
| `morph/labs/morph_lab.cpp` | The `morph` lab: a lumpy low-poly body flat, smooth-shaded and rounded in two frames |

## Rules

- **Smooth normals are the default; rounding is an option, off.** The user judged the rounded
  patches worse than smooth shading alone on the real models: the patches swell thin parts and
  soften silhouettes the artists drew on purpose. Keep `smooth_model` for a caller that asks.
- **The seam is the format's, not ours.** A file vertex on the seam is read twice when a
  back-facing triangle uses it: the second copy reads the skin's right half. Any other split (a
  twin anywhere but half a skin over) cannot be written back; the fixture writer marks a triangle
  back-facing when it reads such a copy.
- **Positions are packed bytes.** Each frame's are a byte an axis times the header's scale plus
  its origin, so a round trip is exact only to 1/255 of the model's extent. Compare within half a
  step, vertex for vertex through the triangles (the reader numbers vertices in the order the
  triangles name them).
- **Winding**: the file winds clockwise seen from outside; the view counter-clockwise. The reader
  and the writer each swap, once.
- **Never name the classic game**; the file kinds are "the classic model file" and "the classic
  sprite file" (magic `IDPO` version 6, `IDSP` version 1).

## Testing

`tests/morph_read_test.cpp` (round trips, the seam, groups and intervals, every truncation and a
wrong magic, version or vertex refused) and `tests/morph_smooth_test.cpp` (unit outward normals
shared across the seam; rounding keeps the corners, cuts `level²`, meets without cracks, stays
flat past the crease and bulges inside it). The lab: `make lab LAB=morph`.
