---
name: assets
description: "Use for engine/assets in gameengine2 and anything that brings an authored model into a game: the glTF 2.0 loader (cgltf, cmake/Cgltf.cmake), the Model it fills (skinned meshes, the skeleton, materials, baked clips), the engine's frame and the mirror from glTF's, posing and the bone palette, CPU skinning and the reach box, GPU skinning into an instance family (render/skinning.hpp), the `models` lab (bind, filmstrips, onion skins, joint-angle graphs), exporting from Blender, and a model's size in the repository."
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

## Exporting from Blender

The builders that make the game's models in Blender, and the `ge asset` command that drives it
headless, are their own roadmap row (`docs/goals.md`, M8g). Until then, the export rules a model
must meet:

- Apply the object transforms. Triangulate. Export normals for smooth shading; a mesh exported
  without normals is made faceted.
- At most four influences a vertex, normalised. One rest pose for every skin.
- **Bake the action at 60 frames a second** (the scene's frame rate, or the exporter's sampling
  rate). A clip sampled at 24 is a run of straight lines, and the 60 Hz bake turns every kink
  into an acceleration spike in the lab's graph.
- glb, base colour factors, no textures.

## A model's size in the repository

A `.glb` does not diff, so every re-export adds a whole copy to the history. Commit only the
game-budget LODs (never a `.blend`, a render or a source-detail mesh), re-export only when the
model's sources change, and hold every model to a size budget in a test (the M8g row lands it).

## Where to look

| File | What |
| --- | --- |
| `engine/assets/model.hpp` | The Model, TRS and the affine, lookups by name |
| `engine/assets/gltf.hpp` | The loader: what it takes, the mirror, what it skips |
| `engine/assets/gltf_write.hpp` | A Model written as a .glb, for the tests and the labs |
| `engine/assets/pose.hpp` | Clip sampling, the palette, CPU skinning, the reach, the turn from rest |
| `engine/assets/test_model.hpp` | The test model: a skinned limb, a rigid fin, two clips |
| `engine/render/skinning.hpp` | The GPU pass, the family LOD, the CPU twin |
| `engine/lab/labs/assets_models_lab.cpp` | The `models` lab |
| `cmake/Cgltf.cmake` | The cgltf pin |
