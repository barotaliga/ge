---
name: assets
description: "Use for engine/assets in gameengine2 and anything that brings an authored model into a game: the glTF 2.0 loader (cgltf, cmake/Cgltf.cmake), the Model it fills (skinned meshes, the skeleton, materials, baked clips), the engine's frame and the mirror from glTF's, posing and the bone palette, CPU skinning and the reach box, GPU skinning into an instance family (render/skinning.hpp), the `models` lab (bind, filmstrips, onion skins, joint-angle graphs), building a model in headless Blender with `ge asset` (the manifest, the builder and ge_asset.py, GE_ASSET_DATA, the stamp, the size budget, the assets_check test), and a model's size in the repository."
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
| `cmake/Cgltf.cmake` | The cgltf pin |
