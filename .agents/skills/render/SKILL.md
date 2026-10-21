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

## What is built (M7a to M7c), and where

| File | What |
| --- | --- |
| `engine/render/view.hpp` | `FrameView` (what a game extracts each frame), the camera's matrices (camera-relative, reversed-z, infinite far), the frustum's planes |
| `engine/render/frame.hpp` | `FrameUniforms` (twin of `wgsl/frame_types.wgsl`: the jittered camera, the last frame's, the jitter), `FrameHistory`, `FrameTargets` (`hdr`, `depth`, `motion`), `PassRow` and `check_pass_table`, `FrameRecorder` (timestamps and `FrameStats` per pass), `make_layout` / `make_group` / `make_buffer` / `make_texture`, `read_red` (any target's channel 0 read back, for labs and tests) |
| `engine/render/sky.hpp` | The atmosphere's tables by compute (`sky_tables.wgsl`), the sky pass (`sky.wgsl`), `frame_layout` and `frame_group` (group 0 of every draw) |
| `engine/render/terrain.hpp` | The GPU-culled geometry clipmap (`terrain.wgsl`, `terrain_cull.wgsl`, `terrain_normals.wgsl`), `clipmap_levels`, `min_max_pyramid`; drawn twice, `encode_terrain_depth` (the prepass) then `encode_terrain` (`equal`, writing colour and motion); `encode_terrain_overdraw` is the labs' probe |
| `engine/render/hiz.hpp` | The depth pyramid (`hiz.wgsl`): the farthest depth per texel down to 1 × 1, `hiz_reduce` its CPU reference, `read_hiz` and `read_depth` |
| `engine/render/taa.hpp` | TAA-U (`taa.wgsl`): `taa_jitter` and `taa_phases`, `taa_render_size`, the history ping-pong, the resolve, `reset_taa`, `taa_output` (what post reads) |
| `engine/render/horizon.hpp` | The ground's own shadow: two horizon maps either side of the sun's bearing (`horizon.wgsl` bakes one), the bake decision (`prepare_horizon`, `Horizon::bakes`), the CPU reference `horizon_tan`, `read_horizon`; the terrain owns one and its draw reads it (`horizon_light` in `terrain.wgsl`) |
| `engine/render/post.hpp` | Exposure from the sky, the tone curve, dither, encoding (`post.wgsl`) |
| `engine/render/wgsl/` | The shared WGSL: `frame_types.wgsl` (and `screen_motion`, the one motion vector), `atmosphere.wgsl` (the air, the tables' maps, the scattering march), `frame.wgsl` (group 0, the haze, the sun's light, the sky's irradiance, the exposure), `terrain_common.wgsl` (the terrain's params block, the horizon's blend in it) |
| `games/fall_line/app/frame.hpp` | Fall Line's pass table (`kPassTable`) and the `GameFrame` that owns the GPU half; `create_frame` is the manifest, `FrameOptions` the TAA switch and the render scale, `cut` the camera cut |
| `games/fall_line/app/views.hpp`, `look.hpp` | The named views and the map's day; the ground's albedo |

Group 0 is the frame block and the sky's tables; group 1 is the pass's own. A compute pass
that WRITES a table binds it only in its group 1 (a dispatch may not read and write one
texture), so the sky's table passes use their own group 0 (`sky.in`).

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
- **Reset temporal state for stills.** A capture cuts first (`fall_line::app::cut`: the
  frame's `FrameHistory` dropped and `render::reset_taa`) and renders the settle frames, so a
  picture does not depend on what came before. One jitter cycle (`taa_phases`) converges a
  still exactly: a young history is a running mean.
- **A new opaque draw writes the motion target** beside its colour (`screen_motion` with its
  camera-relative position, w = 0 for a direction at infinity) and joins the depth prepass
  with the same `@invariant` vertex stage, or the `equal` test drops its pixels.
- **While porting**, reference pictures of the same views are made as
  `porting/subsystems.md` § render says, and `porting/rendering-survey.md` maps the
  measured costs onto these passes. Put both pictures in the PR.

## Labs

**`world`** (`games/fall_line/labs/gpu_world_lab.cpp`): Fall Line's frame from each named view
of a seed (`--seed`, `--views`, `--hour`, `--width`, `--height`, `--taa`, `--scale`,
`--settle`), captured headless; the tiles the cull drew, every pass's GPU ms, the CPU half,
the shimmer of a still and the overdraw probe (shaded per covered pixel, with and without the
prepass) are measured metrics beside each picture.
Run it before and after any change to a pass, and look at every view's picture against its
`look_for`. For a shadow change, run it at several `--hour`s.

**`shadows`** (`games/fall_line/labs/gpu_shadows_lab.cpp`): the horizon maps themselves, the
map from straight above at each `--hours` hour with its shadowed share, and the bake counter
of a sun swept from `--from` to `--to` (deterministic, no GPU: `sweep-bakes` is two plus one
per bearing step crossed). A change to the bake or its decision runs it before and after.

**`taa`** (`games/fall_line/labs/gpu_taa_lab.cpp`): a camera flown from a view (`--speed`,
`--turn`, `--frames`, `--scale`), its last frame against the converged still of the same
camera (ghosting, and a picture of the difference eight times brighter), a single jittered
frame against it (aliasing), and two stills after a cut, which must be the same bits. A change
to the resolve, the jitter or the motion vectors runs it before and after.

Every pass lands with a `picture` lab. Until the GPU capture exists (M6–M7), a lab draws on
the CPU raster (`engine/lab/raster.hpp`). After that, a lab renders a named view offscreen
(`docs/rendering.md` § Lab hooks), reads it back to a `ge::lab::Picture` and hands it to
`Lab::shot`, so it gets the same caption band, sidecar and contact sheet. Caption every view
with what to look for. The design is `docs/labs.md` § Pictures before and after the renderer,
and the `labs` skill owns how.
