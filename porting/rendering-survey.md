# Working note: what game5 draws, and where each part lands

**This is a porting working document.** It may name game5 and its files, because it
exists to map them onto the engine. Delete it when M8 is done. The plan itself is
`docs/rendering.md`, which names no legacy source.

The old renderer is evidence of what the scene needs and what it costs, not an
architecture to keep. `docs/rendering.md` is designed on its own merits; where it lands
on a technique the old renderer also used (a horizon map, a clipmap), it is because the
measurements below argue for it. Nothing here is ported as code: the engine and its
games are C++.

Surveyed on game5 `3717f39` (2026-10-21), from `pwa/src/game/`, `docs/webgpu.md` and
`docs/performance.md` there.

## game5's renderer today

three.js r186, partway through a move from `WebGLRenderer` to the node renderer
(`WebGPURenderer` + TSL); `docs/webgpu.md` lists what is ported (the sky, the
terrain, the trail map, deform, the hero shadow, particles, the forest batch, the
grade). The look is GLSL grafted onto three's materials (`hazeMaterial`) and TSL
twins of the same uniforms.

## Pass by pass, game5 file to engine home

| game5 | What it does | Engine pass (`docs/rendering.md`) | Home |
| --- | --- | --- | --- |
| `sky-gpu.ts`, `sky-gpu-glsl.ts`, `sky-atmosphere.ts`, `sky-table.ts` | Transmittance, multi-scattering, sky-view tables; HIGH's froxel aerial perspective | Sky tables + aerial-perspective volume (compute) | `engine/render` |
| `haze.ts`, `haze-tsl.ts` (`skyColour`, `aerialShade`) | The haze every world material fades into | Shared WGSL haze function | `engine/render` (the function); the game's look values in `games/fall_line/app` |
| `cloud-volume*.ts`, `clouds.ts`, `cloud-noise-worker.ts` | Half-res raymarch, 1/16 fresh a frame, carried by reprojection | Cloud volume pass | `engine/render` |
| `night-sky*.ts`, `contrails*.ts`, `far-ranges*.ts`, `far-faces*` | Night sky, contrails, painted/3D far ranges | Sky pass; far ranges as a far terrain ring | sky: `engine/render`; far ranges: `plugins/mountain/app` |
| `terrain.ts`, `terrain-tsl.ts` (`TERRAIN_WEDGES`, `uHole` sinking) | Clipmap, 16 wedges culled by min/max boxes, seam sink without discard | Heightfield clipmap, tiles culled in compute | `engine/render` |
| `snow-glsl.ts`, `snow-trail-*.ts`, `snow-vertex-glsl.ts` | Snow shading, glitter, corduroy, grooves; adaptive deform patch | Terrain material function | `plugins/mountain/app` |
| `trail-map.ts`, `trail-stamp.ts` (`drawnDepth`), `deform-field.ts`, `deform-snow.ts`, `deform-shade.ts` | Trail map and deformable snow stamped from contacts | Toroidal snow-mark maps, stamped in compute | `plugins/mountain/app` |
| `terrain-shadow.ts`, `terrain-shade.ts`, worker | Horizon map, two bearings blended (`HorizonBlend`) | Horizon map (compute) | `engine/render` |
| `shadow-box.ts` (`sunDue`, `castsInto`), `shadow-depth.ts` | One sun map round a circle ahead of the lens, held when nothing moved | Cached cascades, the casts-into rule in culling | `engine/render` |
| `hero-shadow.ts` | Per-skier depth atlas | Figures' shadow atlas | `engine/render` |
| `forest.ts`, `forest-batch.ts`, `tree-shapes.ts`, `tree-bands.ts` | 34k trees, bands, multi-draw batch, CSR bins | Instance family + compute cull/LOD | mechanism `engine/render`; shapes `plugins/mountain/app` |
| `view-cull.ts`, `tile-split.ts`, `village-cuts.ts` | Per-instance frustum culling for crowd, traffic, carriers; village tiles | Instance families | `engine/render` |
| `skier-*.ts`, `skier-dress.ts`, `crowd-*.ts`, `skier-models.ts`, `model-merge.ts` | Skinned figures, dressed in code; merged glTF skis | Compute pre-skin (hero), vertex skin (crowd); cgltf | mechanism `engine/render`; figures `games/fall_line/app` |
| `spray*.ts`, `gpu-spray.ts`, `snow-cloud*.ts`, `snowfall-plan.ts` | Spray (GPU-flown), snow cloud, snowfall | Compute particle pools | `engine/render` (pools); emitters `plugins/mountain/app` |
| `grade-pass.ts`, `grade-tsl.ts`, `colour-grade.ts`, `region-picture.ts` | Region grade, tone map, sRGB by hand | Post pass, grade as 3D LUT | `engine/render` (pass); `games/fall_line/app` (the grade) |
| `settings-video.ts`, `picture-fit.ts` (`PICTURE_PRICES`, `fitPicture`) | The picture ladder and AUTO fit | Quality ladder | values `games/fall_line/app`; fit mechanism `engine/render` |
| `renderer-instruments.ts`, `benchmark*.ts` | GPU timer, `FrameCost` | Per-pass timestamps, frame stats | `engine/gpu`, `engine/render` |
| `flash-lights.ts`, `LAMP_COUNT` | Fixed lamp slots so no relink | Fixed light list | `engine/render` |

## Lessons from game5's perf ledger that shaped the plan

- Round 1, 4, 14, 15: programs linked mid-run were the worst hitches. → the pipeline
  manifest and the creation counter.
- Round 1, 10: the sun's shadow pass was 68 → 29 ms (software GL) and is now skipped on
  frames where nothing moved. → cached cascades.
- Round 1, 5: the forest refill was CPU-bound. → GPU-driven culling; the CPU writes
  only moved instances.
- Round 11, 12: on an iPhone 15 Pro the ground was over half the GPU, as pixels, and
  its `discard`s hurt a tile GPU. → depth prepass + `equal` test, no discard.
- Round 14: the deform patch's 5 cm triangles cost 2.5 ms in half-empty quads. →
  adaptive patch density.
- Round 6, 7: three culls meshes, not instances. → per-instance culling for every
  family.

## Not ported as game5 does it

- three's per-material uniforms and program-per-material model: replaced by vertex
  pulling, a material table and texture arrays.
- The WebGL MSAA-canvas and `drawsStraight` split: one HDR path, TAA-U or MSAA.
- Fog after tone mapping on WebGL: the engine hazes in linear light only, as game5's
  node renderer does.
