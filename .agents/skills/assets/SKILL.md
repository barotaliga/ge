---
name: assets
description: "Use for engine/assets in gameengine2 and anything that brings an authored model into a game: the glTF 2.0 loader (cgltf, cmake/Cgltf.cmake), the Model it fills (skinned meshes, the skeleton, materials, baked clips), the engine's frame and the mirror from glTF's, posing and the bone palette, CPU skinning and the reach box, GPU skinning into an instance family (render/skinning.hpp), the `models` lab (bind, filmstrips, onion skins, joint-angle graphs), the `model-views` and `skier-views` labs (turntable, axes with a ruler, topology, close-ups, LODs at their sizes) and reference sheets (photographs, kept temporary, cropped beside the model posed and turned to match), building a model in headless Blender with `ge asset` (the manifest, the builder and ge_asset.py, GE_ASSET_DATA, the stamp, the size budget, the assets_check test), and a model's size in the repository."
---

# Assets

An authored model enters the engine ONCE, at load: `assets::load_gltf` reads a glTF 2.0 file
into a `Model` (`engine/assets/model.hpp`), and nothing after that reads a file format. Most of
what a game draws is still built in code (the render skill's shape builders, a plugin's
procedural meshes); a model is for the few rigged, detailed things where an authored mesh looks
much better (`docs/rendering.md` § Authored models).

## The contract

- **The frame is the world's**: y up, +z forward, +x right, metres. glTF's +x is the asset's
  left, so the loader mirrors x on everything (positions, normals, rotations as
  `(x, -y, -z, w)`, matrices) and swaps each triangle's last two indices so its front stays
  outside. A model therefore faces heading 0. A model that comes out facing backwards was
  exported facing -z: fix the export, never the loader.
- **Every mesh is skinned.** A rigid part (a mesh with no skin) is bound with weight 1 to its own
  node's joint, so a clip that moves a node moves its mesh, and rigid and deforming parts pose
  through one path.
- **Every node is a joint**, parents first, at most `kMaxJoints` (a vertex's joint index is a
  byte). A joint no skin names gets the inverse of its rest as its inverse bind.
- **Clips are baked at `kClipRateHz`** (60 Hz) at load, STEP, LINEAR (rotations on the arc) and
  CUBICSPLINE alike; `sample_clip` blends the two baked frames either side of a time.
- **Skipped, and named in `LoadResult::notes`**: textures (the base colour factor is used),
  morph targets, a second set of joints and weights, cameras, lights, animated morph weights.
  An error says what failed, where and the next action (`docs/architecture.md` § Diagnostics).

## Posing a figure each frame

```cpp
assets::sample_clip(model, clip, t, /*loop=*/true, local);   // or the game's own locals
assets::pose_globals(model, local, global);
assets::skin_palette(model, global, palette);
render::set_palette(device, skin, palette);                  // the GPU poses it in "skinning"
```

Every span is sized to the model's joints by the caller, once: posing allocates nothing.

On the GPU (`render/skinning.hpp`) a skinned mesh is a LOD of an instance family made by
`skinned_source(mesh, reach)`, so the family's cull, LODs and shadow casters work on it
unchanged. Its positions are packed into its REACH box (`reach_box`: bind plus every frame of
every clip, grown 5 %), so a pose outside the box is clamped to its walls. A game that poses a
figure procedurally passes a reach of its own. One family mesh is one pose: a figure that moves
on its own is a mesh of its own. `skin_packed` is the pass's CPU twin, and
`tests/render_skinning_test.cpp` holds the two within one 16-bit step.

## Looking at a model: the `models` lab

```sh
make lab LAB=models                                  # the test model, through a .glb
make lab LAB=models ARGS="--model=path/to/x.glb"     # any model
```

What comes back, and how to read it:

- `bind`: shaded with its edges and skeleton, from behind and from the right. A hole is a
  triangle wound inward. A joint outside the mesh where it bends is a misplaced bone.
- `clip-<name>-film`: evenly spaced frames from behind. The mesh should follow its bones with
  no fold or candy-wrapper twist at a joint.
- `clip-<name>-onion`: every frame's bones on one picture. Even ghosts mean even speed. A gap is a
  jump; a bunch is a hold.
- `clip-<name>-angles`: each joint's angle from rest, and the SIZE of its angular acceleration
  (the second difference of its turn, a rotation vector, so a swing through rest shows no cusp).
  A spike is a pop or a jolt at that time.
- The metrics: weights that do not sum to 1 and influences naming no joint fail the lab.

## Looking at a model like a modeller: `model-views`, `skier-views`

The `models` lab checks a model's motion. These look at its FORM, on the sheets of
`engine/lab/model_sheets.hpp` over the CPU model view (`engine/lab/model_view.hpp`: perspective
or orthographic, z-buffered, lit, wired, boned, supersampled; the same pixels on every row).

```sh
make lab LAB=model-views ARGS=--asset=skier            # a built asset by name
make lab LAB=model-views ARGS="--model=x.glb --detail=head,wrist_l"
make lab LAB=skier-views                               # the skier as the game poses it
```

- `turntable`: eight azimuths in perspective (az 0 is the front, from +z; 90 its right, +x).
  Holes, missing faces, a part floating off the body.
- `axes`: orthographic from the front, the right, the back and above against a ruler (a tick
  every 10 cm). Proportions: a figure's height, a ski's length, left against right.
- `topology`: every edge, then the skeleton with every joint named. Triangles should be spent
  where the silhouette bends, and every joint should sit inside the mesh where it bends it.
- `details`: close-ups, two sides each, of the skeleton's ends and every joint whose own skin is
  3 to 35 cm round. A joint skinned to a bigger part (a ski's tip, a pole's basket) gets the
  15 cm round it. Pick your own with `--detail=a,b`.
- `lods`: every level of detail (meshes named `<name>.lod<N>` or `<name>_lod<N>`) with its
  triangles, then each drawn 96, 48, 24 and 12 px tall and scaled up. A level is good enough where
  its pixels are the same as the level above's at the size it is used.
- `clip-<name>`: a clip's frames from front right and from the right.
- `skier-views` adds the game's STANCES (stand, carve-left, carve-right, tuck, air, down) from
  four sides, the carve all the way round and close up. To film the game's motion, bake a run with
  `make lab LAB=anim` (it writes `skier-run.glb` into its previews) and pass that to
  `model-views --model=`.

## Reference sheets: the model beside photographs

Both labs take `--refs=<file>`. Each row of the sheet holds a photograph of the real thing,
cropped, on the left. On the right is the model, posed and turned to be seen as the photograph
sees it. The reference file's keys are in `engine/lab/model_compare.hpp`. The loop:

1. **Find photographs** of the subject from the angles that matter (a carve from the front and
   the side, a tuck from the side). Prefer freely licensed ones; Wikimedia Commons' API lists a
   file's licence (`prop=imageinfo&iiprop=url|extmetadata`).
2. **Keep them TEMPORARY.** Download into a scratch directory outside the tree. A photograph, and
   a sheet that holds one, is never committed, attached to a PR or published. Only the numbers
   and the changes it led to go in the tree. The `refs` shot is `measured`, so it is outside the
   digest.
3. **Convert** to an 8-bit PNG (`convert photo.jpg PNG32:photo.png`) and read the crop off the
   picture: `crop=x,y,w,h` round the subject, with a little snow under the skis.
4. **Find the camera.** Judge where the photographer stood relative to the subject's heading:
   az 0 is in front of it, 90 its right, 270 its left. Sweep with `az=a..b:step` (up to 12
   panels), with `el` a few degrees for a camera on the snow. Use `fov` near 30 for a long
   lens and wider for a close one. Keep the panel whose skis and shoulders line up with the
   photograph's.
5. **Pose it the same.** Use a lab's pose names: `rest` or `<clip>@<s>` in `model-views`;
   a stance plus changes such as `carve-left,lean:-0.9,edge:-1.0,tuck:0.5` in `skier-views`.
   Put several rows on one sheet, one for each photograph.
6. **Judge** what differs: proportions, silhouette, the stance's angles, the gear. Then change
   the MODEL (its builder) or the POSER, never the camera, to close the gap, and run again.

## Building a model: `ge asset`

A model the game ships is BUILT, never hand-exported: a Blender Python builder makes it off the
game's own data, `ge asset build` runs it headless, and the `.glb` is committed beside a stamp
(`engine/assets/manifest.hpp` states the rules; `plugins/skier/assets/skier/` is the worked
example).

```
<home>/assets/<name>/
  asset.txt     builder: build.py / data: <set> / source: <file> / budget-kib: <n>
  build.py      imports ge_asset (engine/assets/blender/ge_asset.py, compiled into ge)
  data.cpp      GE_ASSET_DATA({"<set>", "<what>", make}): the game's numbers as JSON
  <name>.glb    the model, as built
  <name>.stamp  the hash of the builder, every source and the data; the Blender that built it
```

```sh
make ge ARGS="asset list"                 # every asset, its weight against its budget, fresh?
GE_BLENDER=python3.13 PYTHONPATH=~/.cache/bpy make ge ARGS="asset build skier"
make ge ARGS="asset check"                # what the assets_check test checks
make lab LAB=models ARGS="--model=plugins/skier/assets/skier/skier.glb"   # then LOOK
```

- **The data is the point.** A builder reads every number it shares with the code (a skeleton's
  joints, a ski's length and sidecut) from its data set, never a copy typed into the script, so
  a change in the code makes the model stale and `assets_check` fails until it is rebuilt. The
  skier's skeleton is `plugins/skier/app/rig.hpp`; `tests/skier_rig_test.cpp` holds the built
  model's joints to it.
- **Blender in the container** is the bpy module: `python3.13 -m pip install --target
  ~/.cache/bpy bpy==5.2.2` (about 1 GB, 20 s), then `GE_BLENDER=python3.13
  PYTHONPATH=~/.cache/bpy`. In the bpy module `bmesh` exists only after `import bpy`.
- **Work in the engine's frame.** `ge_asset.to_blender` turns (x right, y up, z forward) into
  Blender's (the front toward -y, the right toward -x), and the export and the loader turn it
  back, so the model faces heading 0. Set the weights yourself (`MeshBuilder`'s `{joint:
  weight}` a vertex) rather than Blender's automatic ones: the same data then gives the same
  bytes, which is what makes a rebuild with no change a no-op in git.
- **Bones from joints.** `ge_asset.armature` puts a bone at every joint, its tail at the first
  child, so the loaded model has one joint a bone by the game's names, plus one for the
  armature object and one for the mesh node.
- The export rules still hold for anything authored: normals for smooth shading, at most four
  influences a vertex, one rest pose a skin, an action baked at **60 frames a second** (24 shows
  as a row of kinks in the lab), glb with base colour factors and no textures.

## A model's size in the repository

A `.glb` does not diff, so every rebuild adds a whole copy to the history. Commit only the
game-budget LODs (never a `.blend`, a render or a source-detail mesh), rebuild only when the
stamp says the sources moved (`ge asset build` with no names builds only the stale), and keep
every model under its manifest's `budget-kib`; raising a budget is a decision the PR states.

## Where to look

| File | What |
| --- | --- |
| `engine/assets/model.hpp` | The Model, TRS and the affine, lookups by name |
| `engine/assets/gltf.hpp` | The loader: what it takes, the mirror, what it skips |
| `engine/assets/gltf_write.hpp` | A Model written as a .glb, for the tests and the labs |
| `engine/assets/pose.hpp` | Clip sampling, the palette, CPU skinning, the reach, the turn from rest |
| `engine/assets/test_model.hpp` | The test model: a skinned limb, a rigid fin, two clips |
| `engine/assets/manifest.hpp` | A built asset: the manifest, the stamp, the budget, the check |
| `engine/assets/data.hpp` | `GE_ASSET_DATA`: the game data a builder is fed |
| `engine/assets/tool.hpp` | The `assets` executable: list, data, check, stamp |
| `engine/assets/blender/ge_asset.py` | The builders' library: start, the frame, bones, meshes, export |
| `engine/cli/asset.hpp` | `ge asset list`, `build`, `check`: finding Blender, the run, the stamp |
| `engine/render/skinning.hpp` | The GPU pass, the family LOD, the CPU twin |
| `engine/lab/labs/assets_models_lab.cpp` | The `models` lab |
| `engine/lab/model_view.hpp`, `model_sheets.hpp` | The CPU model view and the sheets every model lab shows |
| `engine/lab/model_compare.hpp` | Reference sheets: the file's keys, photographs beside the model |
| `engine/lab/labs/assets_model_views_lab.cpp` | The `model-views` lab |
| `plugins/skier/labs/skier_views_lab.cpp` | The `skier-views` lab: the game's stances |
| `cmake/Cgltf.cmake` | The cgltf pin |
