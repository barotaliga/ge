---
name: render
description: "Use for engine/render and a game's draw code in gameengine2: the pass table and named targets, a pass's prepare/encode halves, GPU-driven instance families and two-phase culling, the depth prepass, the heightfield clipmap, shadows (horizon map, cached cascades, shadow atlas), the sky tables and clouds, TAA-U and post, particles, skinning, the quality ladder, lab captures and frame budgets."
---

# Render

The renderer reads the simulation through one extraction a frame (`FrameView`, plain
data, interpolated by `alpha`) and never writes simulation state. **`docs/rendering.md`
is the plan**: the workload, every technique and why, the pass order, and what was
rejected. Read the section for your pass before the first edit. If you change a
technique, change that page in the same PR.

## Where a piece goes

- `engine/render/`: a mechanism any game could use, with no genre's words (a
  heightfield, an instance family, a cloud volume, TAA-U).
- `plugins/<name>/app/`: a plugin's presentation half, such as `mountain/app`'s snow material or tree shapes (the `plugins` skill classifies a file).
- `games/<game>/app/`: the game's pass table, its quality ladder's values, its grade.

## A pass

```cpp
struct TerrainPass { /* GPU buffers, pipelines, CPU staging */ };
void prepare(TerrainPass&, const FrameView&, Upload&);   // CPU: decide, fill staging; testable with no GPU
void encode(TerrainPass&, gpu::Encoder&);                 // GPU: bind and dispatch or draw; makes no decisions
```

- The **pass table** is one function in the game's app that lists the passes in order.
  Each pass declares the named targets it reads and writes. Add a pass by adding a row
  there, never by drawing from inside another pass.
- `prepare` allocates nothing. It fills staging arrays reserved at load.
- `encode` makes no decisions. If it needs a branch, the branch belongs in `prepare`.

## The rules that keep it fast

1. **The CPU never walks instances in a frame.** Repeated things are an instance
   family, culled and LOD-selected in compute, drawn with indirect draws. The CPU
   writes only the instances that moved.
2. **Every opaque pixel is shaded once.** Opaque draws go in the depth prepass, and the
   colour pass tests `equal`. Opaque shaders never `discard`; alpha-tested foliage
   writes its coverage in the prepass only.
3. **Nothing is created mid-run.** No pipeline, no buffer, no texture after load (the
   `gpu` skill's manifest).
4. **What stands still is not redrawn.** Shadow cascades, sky tables and the horizon map
   are updated when their inputs move past a threshold, not every frame.
5. **One copy of each shading idea.** The haze, the sun, shadow lookups and decoding
   live in the shared WGSL library. A pass that needs them includes them.
6. **Quality is data.** A ladder stop changes pass parameters and `override` constants,
   never which code runs.

## Checking a pass

- **Capture it, in the container.** Every target is named, and the lab tools capture
  any of them headless on the software adapter, at the lab's small default size, with
  per-pass stats beside the picture (`docs/rendering.md` § Lab hooks). Put before and
  after captures in the PR. A pass that cannot be checked this way is not done.
- **Price it.** Draws, instances and triangles per pass come from the container. GPU ms
  only means something on a real GPU, so a timing claim needs the Mac: open a
  `needs-mac` issue (`docs/rendering.md` § Where it is developed) and say so.
- **Reset temporal state for stills.** A capture calls `reset_history()` and renders the
  settle frames, so a picture does not depend on what came before.
- **Reference pictures** for the demo game's look, and the mapping from the original
  renderer to these passes, are in the working note `porting/rendering-survey.md`.
