---
name: assets
description: "Use to add or change one of this game's MODELS (a rigged figure, a vehicle, a prop) built in headless Blender: the asset's directory and manifest (asset.txt), the builder script and the engine's ge_asset library it imports, the game data it is fed (GE_ASSET_DATA), ge asset build, list and check, the stamp that says what a model was built from, the size budget, and looking at the result with the models and model-views labs, beside reference photographs; and SOUNDS taken in from free packs with ge sound match, import and split."
---

# Built assets

Most of what this game draws is built in code. A MODEL is for the few rigged, detailed things
where an authored mesh looks much better: it is made by a Python builder in headless Blender,
off the game's own data, and the .glb is committed beside it. Nothing in the game runs Blender:
the engine loads the .glb (`assets::load_gltf`).

## An asset

```
<home>/assets/<name>/
  asset.txt     builder: build.py / data: <set> / source: <file> / budget-kib: <n>
  build.py      the builder (Blender Python)
  data.cpp      GE_ASSET_DATA({"<set>", "<what it holds>", make}): the game's numbers as JSON
  <name>.glb    the model, as built
  <name>.stamp  the hash of every source it was built from, and the Blender that built it
```

`<home>` is wherever the code it belongs to lives. The data set is the point: a builder reads
the game's numbers (a skeleton's joints, a vehicle's dimensions) from the data, never a copy
typed into the script, so a change to the numbers makes the model stale and the `assets_check`
test says so.

## The builder

```python
import ge_asset
data, out = ge_asset.start()                      # the data set and the output path
rig = ge_asset.armature("rig", data["joints"])    # bones from the game's joints
mesh = ge_asset.MeshBuilder()
mesh.loft(rings, "jacket")                        # tubes, boxes, spheres in the GAME's frame
mesh.finish("figure", rig, materials)
ge_asset.export(out)
```

Work in the game's frame (x right, y up, z forward, metres); `ge_asset.to_blender` turns it into
Blender's and the export and the loader turn it back, so the model faces heading 0. Set every
vertex's weights yourself (at most four, normalised by the library) rather than relying on
automatic weights, so the same data always gives the same model.

## The commands

```sh
ge asset list                    # every asset, its size against its budget, fresh or not
ge asset build <name>            # build, stamp and check one (no name: every stale one)
ge asset check                   # what the assets_check test checks: stamps and budgets
ge lab models -- --model=<home>/assets/<name>/<name>.glb   # LOOK at it: bind, skeleton, clips
ge lab model-views -- --model=<home>/assets/<name>/<name>.glb   # turntable, ruler, close-ups, LODs
ge lab model-views -- --model=<glb> --refs=<file>           # beside photographs of the real thing
```

**Reference sheets** (`--refs`, keys in `lab/model_compare.hpp`) set each photograph, cropped, on
the left of a row and the model on the right, posed and turned to match. `az=a..b:step` sweeps
the angle until it lines up. Download the photographs to a scratch directory and convert them to
PNG (`convert x.jpg PNG32:x.png`). Never commit, attach or publish a photograph or a sheet that
holds one. Then change the builder, never the camera, until the model holds up beside them. A
game's own lab poses the model its own way through `lab/model_sheets.hpp`.

Blender comes from `--blender=<cmd>`, `GE_BLENDER`, `blender` on the PATH, or a python3 that can
`import bpy`. With no Blender, `pip install bpy` into a directory and point `GE_BLENDER` at the
Python and `PYTHONPATH` at the directory (the finding gives the exact line).

## Free sounds

A sound someone else made free comes in through `ge sound`, never by hand:

- `ge sound match <reference> <pack-dir>... --pool=door|slam` ranks every sound in the packs
  against a reference (the sound it stands in for, or one you like) by length, energy in seven
  bands and loudness envelope. The pool decides more than the score: keep it to the right kind of
  sound, then take the best one or two and say in the PR that nobody listened.
- `ge sound import <file> <group/name> --to=<dir>` trims the silence, cuts to `--max` seconds,
  fades (`--loop` for a longer tail), brings it to -16 LUFS under a -1.5 dBFS peak and writes
  mono 16-bit WAV at 22.05 kHz. A loudness it cannot reach under the peak is reported.
- `ge sound split <file> <group/name>` cuts a recording of many sounds at its silences.

ffmpeg on the PATH decodes the sources. Take only what may be redistributed, changed and sold
(CC0 first; no share-alike, noncommercial or no-derivatives licence), name every file by what it
is in this game rather than its pack's name, and record the pack, author, URL and licence of each
one in the game's credits file in the same commit.

## The rules

- Commit the .glb and its stamp together, always from `ge asset build`; never hand-edit either.
- A model stays under its `budget-kib`. Over it, cut triangles or LODs in the builder; raising
  the budget is a decision the PR states.
- A .glb does not diff, so every rebuild adds a whole copy to the history: rebuild only when the
  stamp says its sources moved.
