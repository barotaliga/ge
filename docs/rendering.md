# Rendering

How the engine draws a frame: the GPU backend, the frame's passes, and the techniques
each pass uses. Milestones M6 to M8 in `docs/goals.md` build it, and this page is the plan
they build to: M6 built the backend (`engine/gpu`, `engine/platform`); the passes are M7 and
M8. Change this page in the same PR that
changes the plan.

The aim is a picture that looks good and costs little. Every technique below was
chosen against the workload in § The workload, and each one names what it replaces and
what it costs. A technique that does not pay for itself on that workload is listed under
§ Considered and not taken, with the reason, so the question is not reopened without
new evidence.

## The workload

The demo game draws an open mountain to a horizon several kilometres away, seen from a
fast chase camera close to the ground. Per frame, at the top of its picture ladder:

| What | Size | What it implies |
| --- | --- | --- |
| Terrain | One heightfield, about 4 km of reach, a few cm a texel near the camera through a deformable patch | Shading the ground's pixels is the largest single GPU cost, most of all on tile-based (phone) GPUs |
| Vegetation | 30–40 thousand instanced trees in a few shapes, three levels of detail plus impostor cards | The CPU cost of choosing and sending instances, and the shadow pass that draws them all again |
| Props and buildings | Lifts and their moving carriers, a village, gates, fences: hundreds of instanced meshes | Draw submission; per-instance culling, not per-mesh |
| Figures | The player, a few rivals, and up to a few hundred people in a crowd, all skinned | Skinning once and drawing many times (main view and shadows) |
| Snow marks | Trail maps stamped from the skiers' prints every frame | Render-to-texture with max blending, a fine window that follows the camera redrawn from the stamps' history, and a coarse map over the whole mountain |
| Particles | Tens of thousands of spray, snowfall and cloud particles | Overdraw of soft transparent quads; simulated on the GPU |
| Sky | A physical atmosphere, a raymarched cloud volume, a night sky, aerial perspective on every material | Lookup tables computed on the GPU; a temporal cloud buffer |
| Light | One sun or moon, the sky's light, at most a handful of point lamps at night | No need for a many-light pipeline |
| Shadows | The mountain shading itself over kilometres, sharp shadows under the figures | Two different problems, solved by two different techniques |

Measured on this scene, the costs that matter most are: 200–350 draw calls and about
1 M triangles a frame when meshes are drawn one by one; a sun shadow pass that redraws
most of the scene; CPU time spent choosing instances; shader programs compiled in the
middle of a run, which cause visible hitches; and, on a phone, the ground's fragment
shader taking more than half of the GPU's time. Each of those has a direct answer below.

## The backend: Dawn, native only

The targets are macOS, Linux and Windows on the desktop, and iOS, iPadOS, the Mac App
Store and Android on the stores. There is no web build. Under the hood that is Metal on
Apple, Vulkan on Linux and Android, and D3D12 or Vulkan on Windows.

**Decision: Dawn, compiled natively on every target, through its standard `webgpu.h` C
header.** The header's name comes from the API's origin as a browser standard; nothing
here runs in a browser or ships for the web. Dawn is a native library that translates
that API to Metal, Vulkan and D3D12, and the engine uses it only for that. With no web
build, the case rests on these reasons:

- **One API over every native backend.** Dawn translates to Metal, Vulkan and D3D12.
  Our own layer would mean at least two backends (Metal for the App Store, Vulkan for
  Linux and Android) for agents to keep correct, plus our own barrier tracking and
  memory management.
- **Mobile drivers come with workarounds.** Dawn is the GPU layer of a large browser on
  Android, macOS and Windows, so it carries per-vendor workarounds for driver bugs
  (Adreno, Mali, PowerVR, Apple, and the desktop vendors) that a hand-written Vulkan
  backend would have to rediscover one crash at a time.
- **Validation an agent can act on.** In debug builds, every call is validated and an
  error names the object by its label and says what was wrong. A GPU-side fault in raw
  Vulkan or Metal is often a hang or a corrupt frame with no message, which only agents
  can debug here. Release builds turn validation off (Dawn's `skip_validation` toggle)
  and drop the shader robustness transform (`disable_robustness`), so the checks cost
  nothing in a shipped frame. `gpu::create_device` sets both in a release build
  (`tests/gpu_device_test.cpp` draws with them on).
- **WGSL everywhere, compiled by one compiler.** Tint compiles WGSL to MSL, SPIR-V and
  HLSL, so there is no shader cross-compiler toolchain in the build, and
  `make shaders-check` validates every shader offline.
- **Native extensions without forking the code.** Dawn native exposes what the web
  spec lacks (multi-draw indirect, subgroups, push-constant-like "immediates",
  timestamps inside passes, memoryless transient attachments). The engine turns them
  on per adapter and keeps a path for when they are missing (§ Feature tiers).
- **A software GPU for the main development environment and CI.** Dawn runs on
  SwiftShader or Mesa's lavapipe (Vulkan drivers on the CPU) on Linux. The everyday
  environment and the CI runners have no GPU, and both can still run the renderer and
  capture screenshots (§ Where it is developed).

**What we give up**, and why it is acceptable for this workload:

| Missing from Dawn's core API | What it would buy | Why the game does not need it |
| --- | --- | --- |
| Bindless resources | Any texture from any shader with no binding | Few materials; texture arrays per size class and a material table cover it (§ Geometry and resources) |
| Mesh shaders | Meshlet culling inside the pipeline | Compute culling plus indirect draws gets the same culling at instance granularity, which is what a forest of identical shapes needs; most phones lack mesh shaders anyway |
| Ray tracing | RT shadows, reflections, GI | Outdoor snow under one sun; the techniques below cover the look, and phones could not afford it |
| Async compute queues | Overlapping compute and graphics | One queue. Compute work is ordered so it sits where the GPU would otherwise be idle, and a frame is small enough that overlap gains little |
| Explicit barriers | Hand-tuned synchronisation | Dawn places them. Passes are ordered so that independent work is not forced to wait |

If one of these gaps ever blocks a measured goal, the escape hatch is a Dawn-native
extension behind a feature tier, never a second graphics API.

**Alternatives** (the full comparison is in `docs/research.md` § GPU API):

- **Vulkan everywhere, with MoltenVK or KosmicKrisp on Apple.** One API with bindless,
  async compute and full control. It was rejected because agents would own manual
  synchronisation and memory, mobile driver bugs and the Metal translation layer, and
  the workload has no measured need for what it adds.
- **Our own thin layer over Metal and Vulkan.** The most control and the best fit to each
  platform, at the cost of two backends to keep in step.
- **SDL3's GPU API** (stable since SDL 3.2) covers Vulkan, Metal and D3D12, including
  iOS and Android, and SDL is already a dependency. It was rejected for now because it
  has no GPU timestamp queries, which the perf-first goal needs, and it needs a shader
  cross-compiler (SPIR-V, MSL, DXIL) in the build.
- **wgpu-native** implements the same header. Two implementations would double the
  platform matrix for shader and validation behaviour. Code that keeps to the standard
  header keeps the option open.
- **bgfx, sokol_gfx, Diligent, Filament**: each brings its own shader dialect, build and
  abstraction for agents to learn, and none gives more of what this workload needs.

**Where Dawn is built.** The prebuilt GitHub release covers Linux x64, Windows x64 and
macOS (arm64): `cmake/Dawn.cmake` pins one release tag and each archive's SHA-256, and
downloads it into `.cache/deps/` at configure time (the archive also carries `tint`, which
`make shaders-check` runs). The prebuilt library is a Release build, so an MSVC Debug build
refuses `GE_GPU=ON`. iOS, iPadOS and Android (and Linux arm64) will build Dawn from source with
its CMake build, cached in CI (M11). Until then those rows build with `GE_GPU=OFF`, which
builds and tests everything except `engine/gpu`, `engine/render` and the app.

**The C++ surface.** `engine/gpu` calls Dawn's `webgpu_cpp.h` (`wgpu::Device`,
`wgpu::Buffer`, …), whose handles are reference-counted RAII already. The engine adds
behaviour (the device's choices, the error lines, the manifest's counter, readback, timing)
and no second layer of handles, so the code reads as Dawn's API spells it.

## Mobile GPUs

Phones and tablets (Apple's GPUs, Adreno, Mali) render in tiles. That changes a few
choices, and each is a feature tier, not a separate renderer:

- **No depth prepass on tile GPUs.** Apple's GPUs remove hidden surfaces in hardware,
  and Adreno and Mali have their own early-depth schemes, so a prepass mostly costs a
  second vertex pass. On a tiler, the opaque pass draws front to back with
  `less-equal`, and the two-phase occlusion culling runs one phase against last frame's
  pyramid. The rule against `discard` in opaque shaders matters even more there.
- **Memoryless attachments.** Depth, MSAA colour and other targets that do not outlive a
  pass are transient (Dawn's transient attachments: memoryless on Metal, lazily
  allocated on Vulkan), so they never leave tile memory.
- **Bandwidth over arithmetic.** Targets are 16-bit floats or packed formats where the
  look allows; post work is merged into as few full-screen passes as possible; MSAA
  resolves in the tile.
- **Thermal headroom.** A phone holds its clock only below full load, so the budget for
  the middle of the ladder leaves headroom (§ Measurement), and the game can cap the
  frame rate at 30 or 60 Hz.

## Feature tiers

The device asks the adapter for its own limits (the defaults are far below what a
desktop GPU offers) and turns on every optional feature from the list below that the
adapter has. Each feature has a path for when it is absent. Both paths are captured by
the lab tests, so neither one can rot.

| Feature | Used for | When it is missing |
| --- | --- | --- |
| `timestamp-query` | GPU time per pass | CPU timing only; the frame's stats show `gpu ms: n/a` |
| `indirect-first-instance` | Indirect draws that start at an offset into a culled instance list | Each draw's base offset is read from a small per-draw storage buffer |
| `multi-draw-indirect` (Dawn extension) | One call for every LOD bucket of a family | A loop of `drawIndexedIndirect`, one per bucket |
| `subgroups` | Prefix sums and reductions in culling and the depth pyramid | Workgroup shared memory |
| `shader-f16` | Half-precision particle and post maths | 32-bit floats |
| `float32-filterable` | Filtering the height and sky tables in 32-bit | 16-bit float copies of those tables |
| `texture-compression-bc` / `-astc` / `-etc2` | Compressed textures for authored assets (BC on the desktop, ASTC on Apple and most phones) | Uncompressed RGBA8 |
| Transient attachments (Dawn extension) | Depth and MSAA targets kept in tile memory on mobile | Ordinary textures |

Only Dawn's core API is a hard requirement. Compatibility mode (Dawn over GLES or D3D11)
is not targeted, because its limits (few or no storage buffers in the vertex stage)
would rule out vertex pulling. On Android that means a Vulkan 1.1 device, which covers
the phones that can run the game at all.

## The frame

The frame is a fixed, ordered **pass table**: one function in the game's app lists the
passes in order, and each pass declares the named targets it reads and writes. Dawn
schedules barriers from those uses. There is no general render graph; the table is the
graph, written out where an agent can read it. A pass has two halves:

```cpp
void prepare_terrain(gpu::Device&, Terrain&, const FrameView&);   // CPU: decide, stage, upload
void encode_terrain(const Terrain&, const gpu::PipelineSet&,      // GPU: bind and draw;
                    FrameRecorder&, wgpu::CommandEncoder&, ...);  // makes no decisions
```

**As built (M7a).** The table is a `constexpr` array of `render::PassRow {name, reads,
writes}` in the game's app (`games/fall_line/app/frame.cpp`, `kPassTable`), the read and
write lists space-separated target names; `render::check_pass_table` names any read before
its write (or a pass listed twice) with no GPU, and the `world` lab holds the game's table
to it. Each pass is opened through `render::FrameRecorder`, which gives it its timestamp
pair and its row in `FrameStats` (GPU ms a frame late, draws, dispatches, the CPU half).
The camera is at the origin of every matrix (the GPU works camera-relative in float, so a
point kilometres away keeps its millimetres), and depth is **reversed-z with an infinite
far plane** in `Depth32Float`, cleared to 0 and tested `greater`. The frame block (`FrameUniforms`,
`render/wgsl/frame_types.wgsl`) and the sky's tables are group 0 of every draw
(`render/wgsl/frame.wgsl`).

The order, with what each stage is for:

1. **Upload.** Each buffer that changed gets one `queue.writeBuffer` from a CPU staging
   array that `prepare` filled. Instance data sends only the ranges that changed.
2. **Compute, before any view is drawn:**
   - the sky's tables, only when the sun moved more than a threshold (§ Sky);
   - the aerial-perspective volume for this camera;
   - the terrain's horizon map, a slice a frame while the sun's bearing moves (§ Shadows);
   - stamping this frame's contacts into the snow-mark maps;
   - particle simulation;
   - skinning for figures that are drawn in more than one view;
   - instance culling, phase one (§ GPU-driven instances).
3. **Shadow views:** the cached sun cascades when they are due, and the figures' shadow
   atlas every frame.
4. **Depth prepass** (desktop GPUs; skipped on tile GPUs, § Mobile GPUs) of the terrain and opaque instances, then a **depth pyramid**
   (Hi-Z) built from it, then culling **phase two** for what the pyramid shows was
   wrongly culled.
5. **Opaque colour** with the depth test set to `equal`: every pixel of the expensive
   snow shader runs exactly once, whatever the draw order. This is the direct answer to
   the ground's fragment cost, and it lets the ground's shader drop `discard`, which
   tile-based GPUs pay for across a whole draw.
6. **Sky and clouds:** the sky behind the depth buffer; the cloud volume at a quarter of
   the pixels, reprojected from last frame (§ Sky).
7. **Transparent:** particles and glows, softened against the depth buffer.
8. **Temporal resolve:** anti-aliasing and upscaling to the output size (§ Anti-aliasing).
9. **Post:** one compute pass for exposure, bloom, the colour grade, tone mapping, dither
   and the output encoding (§ Post).
10. **UI:** the HUD and cards, at output resolution.

**As built (M7c).** Fall Line's table runs `sky.tables`, `terrain.cull`, `terrain.horizon`,
`terrain.depth`, `hiz`, `terrain`, `sky`, `taa`, `post`. The terrain draws twice from one
vertex stage whose position is `@invariant`: depth only (`terrain.depth`, `greater`, writing),
then shaded where the depth is `equal` with the depth read only (`terrain`), so a covered
pixel's snow shader runs once. The `world` lab's overdraw probe (`encode_terrain_overdraw`)
counts it: exactly 1 a covered pixel, but where two clipmap rings meet at the very same depth
and both pass `equal` (a few pixels in ten thousand); with no prepass the same draw shades 1.0
to 2.2 a pixel on seed 38's views, by how much of the mountain hides behind itself. The
**Hi-Z pyramid** (`render/hiz.hpp`) reduces the prepass's depth to an `R32Float` mip chain from
half the render size down to 1 × 1, each texel the farthest depth (reversed-z: the minimum)
of every texel below it that overlaps it, so an odd edge is never dropped; it matches its CPU
reference (`hiz_reduce`) texel for texel, and the instance families' second culling phase
reads it (§ GPU-driven instances). The colour pass and the sky write the **motion target** (`RGBA16Float` at
the render size: rg the uv this pixel's surface moved since the last frame, unjittered; b the
reactive mask, 0 for the ground and the sky but where the snow's glitter writes it (M8a), and
particles' later) from
the frame block's `prev_view_proj` and `eye_delta` (`FrameHistory`, `frame_types.wgsl`'s
`screen_motion`); the sky's motion is the camera's turn alone.

**As built (M7d1).** The table grows the instances around the terrain's: `sky.tables`,
`terrain.cull`, `instances.cull` (phase one, reading the last frame's pyramid),
`terrain.horizon`, `terrain.depth`, `instances.depth`, `hiz`, `instances.cull.late`,
`instances.depth.late`, `terrain`, `instances` (both phases' lists, tested `equal`), `sky`,
`taa`, `bloom` (M7d3), `post`. The late phase's depth lands before the colour pass, so it is still shaded
once; the late phase's draws are not in this frame's pyramid, which only makes the next
frame's phase one keep a little more.

**No pipeline is created after load.** Every pipeline the game can use is listed in a
manifest and created with `createRenderPipelineAsync` behind the loading card. The GPU
layer counts pipeline creations, and after load a creation is an error that names the
pipeline and says to add it to the manifest. A missing entry fails the lab tests
instead of causing a hitch in a run.

**As built (M7d2).** The sun's shadows land between the horizon bake and the prepass:
`shadows.cull` (the casters' cull into each map drawn this frame), then `shadows.near`,
`shadows.far` and `shadows.figures`, each a depth-only pass into its layer of the shadow maps;
the `terrain` and `instances` rows read `shadows.maps`. A cascade that is not due still opens its
pass (read-only, nothing drawn), so the recorder's GPU times keep their order.

## GPU-driven instances

Every repeated thing (trees, rocks, props, carriers, crowd members, gate poles) is an
**instance family**: one storage buffer of instances in SoA layout (position, rotation,
scale, kind, flags), a table of meshes with their LOD ranges and bounding spheres, and
the indirect draw arguments for each (mesh, LOD) bucket. The CPU never walks the
instances in a frame. It writes only the instances that moved.

Culling is a compute pass over every family for every view:

1. **Phase one:** test each instance against the view's frustum and last frame's depth
   pyramid, choose its LOD from its projected size (with a dithered cross-fade band
   between LODs, so a change never pops), and append the visible ones to a compacted
   list for each bucket. Atomics or a subgroup prefix sum give each instance its slot,
   and the bucket's instance count is written into its indirect arguments.
2. **Phase two**, after the depth prepass: retest the instances that phase one rejected
   by occlusion against this frame's pyramid, and draw the ones that are visible now.
   This two-phase scheme removes the one-frame-late artefacts of plain reprojected
   occlusion culling when the camera turns fast.

Shadow views run the same pass with their own rule. An instance casts when its shadow
can reach the shadowed area, not when it is near the camera, so riding towards a wood
never switches its shadows on.

Each bucket is one indirect draw, or one multi-draw indirect call for a whole family
where the feature exists. The forest and the crowd then cost a handful of draws
whatever their size, and the CPU's frame cost no longer grows with the instance count.

**As built (M7d1)** (`render/instances.hpp`, `instances_cull.wgsl`, `instances.wgsl`). A
family's meshes are built on the CPU (`render/mesh.hpp`: boxes, cylinders and cones,
ellipsoids, faceted, wound outward) and packed to three words a vertex: the position in
16-bit steps of the mesh's box, a part (a material slot) and the normal octahedral in two
snorm16s. The vertex stage pulls them from one storage buffer, so a family is one pipeline
whatever its meshes. The instances are five streams of `vec4f` in one storage buffer
(position and material, rotation, scale and mesh, last position, last rotation, the last two
for the motion target); `set_instances` writes a range, and the CPU keeps a mirror so a write
carries the old pose into the last one. A **bucket** is one (phase, LOD) pair with its own
`DrawIndexedIndirect` arguments; portable indirect draws carry no first instance, so the
bucket rides in `base_vertex` (`bucket << 20`) and the vertex stage reads its list slot from
`vertex_index >> 20` and `instance_index`.

The cull is one thread an instance. **Phase one** drops what lies outside the frustum, what
is under 0.35 px of radius, and (with a last frame) what the last frame's pyramid hides,
seen through the last frame's camera; that last kind is marked. **Phase two**, after the
pyramid, retests the marked ones against this frame's. The test projects the bounding
sphere's box, widens its rectangle by a render pixel (the last frame was drawn under a
jitter this camera does not have), reads the mip where the rectangle spans at most three
texels a side and compares the box's nearest depth with the farthest there. A mip's size is
taken from the pyramid's halving, never from `textureDimensions(hiz, mip)`, which the
software adapter answers wrongly when the level differs between threads. The **LOD** is
picked by the radius on screen against each LOD's least size, with a band of ±15 % round
each threshold where the instance is appended to both LODs and each discards its share of
pixels by a dithered threshold in the depth prepass only (the colour pass tests `equal` and
never discards); `pick_lod` is the CPU twin. The lit draw shades by the sun, the ground's own
horizon shadow at the instance's point (`terrain_common.wgsl`'s `ground_shadow`, shared with
the terrain) and the sky's light, hazed like the ground. Back faces are culled: the world is
left-handed and the view a reflection, so a front face is clockwise on screen.

The **cull check**: the `world` lab draws every view a second time with occlusion culling
off and counts the pixels that differ, which must be none (`cull-faults`);
`tests/render_instances_test.cpp` holds the phases' counts on a wall with boxes behind it.
On seed 38 at 640 × 360 a run's moment draws about 3,000 trees of 21,000 and the pyramid
hides about 9,000; the forest is three placeholder meshes at two LODs each.

## Geometry and resources

- **Vertex pulling.** Meshes of one vertex format share one large storage buffer, and
  the vertex shader reads its vertex by index. Pipelines are keyed by material model,
  not by mesh, so there are few of them.
- **Compact vertex formats.** Positions are 16-bit normalised integers inside the mesh's
  bounds, normals and tangents are octahedral-encoded 16-bit pairs, and texture
  coordinates are 16-bit. This is about a third of the bytes of 32-bit floats, and
  vertex bandwidth is what instanced meshes spend.
- **A substitute for bindless.** Material parameters live in one storage buffer indexed
  by a material id. Textures of a size class live in one texture array indexed by a
  layer id. A draw selects both by an index, so draws of different materials do not
  change bind groups.
- **Bind group frequency.** Group 0 is per frame (camera, sun, time, the sky tables),
  group 1 is per pass (the shadow maps, the snow-mark maps), group 2 is the material
  table and texture arrays, and group 3 is per draw (instance list, bucket offset).
- **Procedural meshes are built on the CPU at load**, from the game's seeded data,
  directly into the shared buffers. Nothing is generated mid-run except the instances'
  positions.

## Authored models

Most of what the game draws is built in code at load (above). A few things are worth
authoring: the rigged, detailed objects the camera sits close to (the skis, the player's
figure, the machines). They come in as **glTF 2.0** and nothing else.

- **Loaded once, into the engine's form** (`engine/assets`, the `assets` skill). The loader
  (cgltf) fills a `Model`: every node a joint, every mesh skinned (a rigid part bound
  whole to its node's joint), the base colours as materials, the clips baked at 60 Hz. glTF's
  frame is mirrored in x into the world's, so a model faces heading 0. Nothing after the
  load reads glTF.
- **Posed on the CPU, skinned on the GPU.** A frame samples a clip (or the game's own pose)
  into the joints' locals, chains them to model space and multiplies the inverse binds into
  a bone palette (`assets/pose.hpp`; no allocation). The `skinning` compute pass
  (`render/skinning.hpp`) moves each vertex by the linear blend of its four joints and packs
  it, in the packed vertex format above, into the mesh's slot in an instance family's vertex
  buffer. Every view that draws the family then reads the posed mesh, so the cull, the LODs
  and the shadow casters need nothing new. Positions are packed into the mesh's REACH (the
  box of every pose) and the mesh's sphere is the reach's, so the cull keeps a figure in
  every pose.
- **Made in Blender, by script.** A model's source is a Blender Python builder run headless
  (`ge asset`, roadmap row M8g), fed the game's own numbers; the committed `.glb` is the
  engine's input, and CI never runs Blender.

**As built (M8e1).** The loader, posing, the CPU reference and the `skinning` pass, held to
each other within one 16-bit step on the software adapter (`tests/render_skinning_test.cpp`),
and the `models` lab. Still to come with the figure drawn in the world (M8e2): last frame's
skinned positions for the motion vectors (a family's motion is its instance's, so a limb that
swings gets none of its own and TAA may smear it), the crowd skinned in the vertex stage from
per-instance palettes, and the skis and the skier as models.

## Terrain

A generic **heightfield renderer** in `engine/render`, which a game hands a height
texture and a material function.

- **Geometry clipmap.** Concentric rings of a fixed grid follow the camera, each ring
  twice the spacing of the one inside it. The vertex shader reads the height from the
  height texture, so the mesh itself never changes.
- **Seams without `discard`.** Where a coarse ring meets a fine one, the vertex shader
  morphs the fine ring's outer vertices onto the coarse grid and sinks the coarse
  ring's covered cells under the fine one.
- **Culled on the GPU.** Each ring is split into tiles. A min/max height grid baked at
  load gives each tile a tight box, and the culling compute pass writes one indirect
  draw a ring with only the tiles in view.
- **Normals from the height texture** in the fragment shader, which keeps the vertex
  small and gives per-pixel lighting at every distance.
- **Detail near the camera** (deformed snow) is a separate fine patch whose vertex
  density follows how much the surface is disturbed, so untouched snow does not pay
  for vertices it does not need.
- **The material function** is the game's or the plugin's: the snow, its age, its
  grooves and glitter (§ Where the code goes).
- **Past the map's edge the grid is read mirrored** (height, normals and albedo alike), so
  the ground runs on to the horizon as its own reflection, with no seam at the edge,
  instead of the edge's heights stretched outward. The far ranges replace it with M8.

**As built (M7a)** (`render/terrain.hpp`): eight levels of 4 × 4 tiles of 32 × 32 quads,
level 0 at 1 m a quad, so the clipmap reaches about 8 km from the eye; each level's square
is centred on the eye snapped to twice its spacing, which puts every finer square on its
coarser neighbour's grid. Every tile is an instance of one 16-bit indexed mesh whose
vertices are pulled from the vertex index; a compute pass culls the tiles (boxes from a
min/max pyramid with a texture's mip chain) into one indirect draw. The morph runs by the
vertex's distance from the eye, from 80 % to 95 % of a level's half side, so nothing pops
as the eye moves; a coarse vertex inside the finer square sinks two spacings. The normals
are baked once into an `RGBA16Float` texture by compute. The game hands the albedo as an
sRGB texture over the same grid (Fall Line's: snow, the packed piste, steep rock).

**As built (M8a): the ground's material.** The terrain's colour pass is a material's: a
shader that includes `render/wgsl/terrain_draw.wgsl` (the bindings of groups 1 and 2, the vertex
stage, the overdraw probe and the two halves every material shares: `ground_at`, the pixel's
place, normal, albedo and the sun that reaches it, and `ground_out`, the haze, the motion and the
reactive mask) and defines `fs_main`, with a bind group of its own at group 3
(`TerrainSource::shader`, `material`; `bind_terrain_material`). The prepass, the colour pass and
the probe all run the material's module, so their depths are still the same bits. `terrain.wgsl`
is the default: the albedo lit by the sun and the sky. Fall Line's is the snow's, in
`plugins/mountain/app/snow.wgsl`, over a ground map baked once a map (the packed share, rock by
slope, the piste's direction as its doubled angle): wind swells and grain bent into the normal on
the loose snow, a broken face on rock, the groomer's corduroy (14 cm a rib, worn in patches) and
lanes (5.2 m a swath, each its own shade) across the piste's way, every term faded by the pixel's
own footprint before it aliases; the wrap's blue terminator; the glare, the sun scattered forward,
harder on the piste; and the glitter, at most one crystal a cell of world space flaring as a point
where its facet lines the sun up with the eye, written to the reactive mask so TAA-U keeps it.
On the software adapter at 1280x720 the snow costs the terrain pass about 17 % over the plain
albedo (the `profile` lab's `--snow=0`).

**As built (M8b): the snow marks.** Every skier's skis lay two capsules a frame
(`plugins/skier/app/tracks.hpp`: a stance apart, the outside ski of a turn the deeper, a carve
narrower, a skid wider and shallower, a scuff on the piste and a hand's furrow in loose snow),
kept in two RGBA8 maps in `plugins/mountain/app/trails.hpp`: a coarse one over the whole map
(about 1.5 m a texel) and a fine window round the camera (96 m at 4.7 cm a texel). Each texel holds
the groove's depth and the berm's height, each as the root of its share of the channel's full
scale (so a centimetre's scuff keeps some 25 levels), and the pressed floor. A render pass draws
each capsule as an instanced quad with **max blending**: a groove ridden twice is as deep as the
deeper, and overlapping stamps in one draw cannot race, which a compute pass writing a texture
could not promise without atomics. Narrower than a texel, a stamp is widened and scaled by its
coverage, so the coarse map holds a faint line. The window is not toroidal: when the camera leaves
its middle half it is **redrawn from the stamps' history**, kept on the CPU and filed in 32 m
buckets (a frame's worst CPU cost, 1.7 ms on a window move in `profile`), so a track laid outside
the window comes back as sharp as it was cut. The snow's material reads the fine map inside the
window and the coarse beyond (blended over the window's rim): the slope of berm less depth, taken
across the larger of a texel and the pixel's footprint, bent into the normal (a groove narrower
than a pixel fades rather than shimmers); the sky's light dimmed in a deep trough; the pressed
floor smooth, with no grain, glitter or comb; and no berm inside a groove (the ring round one
stamp's end crosses the next stamp's groove). A pass with nothing to stamp opens an empty compute
pass under its name rather than loading and storing its map. Fall Line's table runs the two,
`trails.fine` then `trails.coarse`, after `terrain.horizon`; the colour pass reads both. On the software adapter at 1280x720
the tracks cost the terrain pass about 6 % and the two stamp passes about 0.5 ms each (the
`profile` lab's `--trails=0`).

The deformation field (the snow standing higher and lower than the ground, moving the vertices)
is not built: the grooves are light, not geometry, and a furrow's silhouette is the case that
would need it.

## Lighting and shadows

**Forward shading with a fixed light list.** The sun or moon, the sky's light (read
from the sky's irradiance table), and up to eight point lamps whose slots are dealt by
distance each frame. With this few lights, forward shading has the lowest bandwidth,
works with MSAA and transparency, and suits tile-based GPUs. **Clustered light
culling** (a compute pass that bins lamps into a froxel grid) is the planned upgrade,
and it is built when a scene measurably needs more than eight lamps.

**Shadows use three techniques, one for each scale:**

1. **The mountain's own shadow: a horizon map.** For each texel of a grid over the
   whole map, a compute pass stores the highest elevation angle the ground reaches
   looking towards the sun's bearing. A point is lit when the sun stands above its
   horizon. The sun's elevation can change with no rebake, and only a change of bearing
   needs a new map. Two maps, one baked behind the sun's bearing and one ahead of it,
   are blended so that a shadow's edge glides with the sun. This covers kilometres of
   ridge shadow for one texture read a pixel, with no shadow acne on grazing slopes.
2. **Everything that stands still: cached cascaded shadow maps.** Two cascades round
   the camera. The far one is redrawn rarely, the near one only when the camera has
   drifted or the sun has turned past a threshold. A cascade that is not redrawn keeps
   its matrix, so its shadows stay exactly where they were. This is what makes the
   shadow pass, which in a naive renderer redraws the scene every frame, cost almost
   nothing in a typical frame.
3. **The figures: a shadow atlas.** Each figure and its vehicle gets a small map of its
   own in one depth atlas that follows it, at millimetres per texel, redrawn every frame.

Filtering is hardware comparison sampling with a small rotated Poisson kernel. A
contact-hardening (PCSS-style) blocker search is a quality-ladder option for the
figures' atlas only.

**As built (M7d2)** (`render/shadows.hpp`, `wgsl/sun_shadow.wgsl`): the cascades and the
atlas are three layers of one `Depth32Float` array (2048 a side in Fall Line): the near cascade
(48 m round the eye), the far one (400 m), and the figures' atlas of 4 × 4 tiles (1.4 m round
each figure, 128 texels across: about a centimetre a texel). Each map is an orthographic view
along the sun, 2 km deep, with depth 1 toward the sun, cleared to 0 so nothing casts; casters
draw with `greater`, and a receiver is lit where its depth is at least the map's. A cascade is
redrawn (`update_cascade`, pure CPU) only when it was never drawn, the eye has drifted a quarter
of its radius across the sun, or the sun has turned 0.25° (near) or 1° (far); its centre is
snapped to its texels across the sun, so a redraw does not swim. Seed 38's whole run from the
chase camera redraws the near cascade on 187 of 14,382 frames and the far one on 22 (the
`world` lab's `cascade-redraws`). The casters are each family's own slots
(`create_family_casters`): the forest's two (one a cascade) culled against the map's box with
no depth pyramid, the figures' one culled once for every tile, each tile drawn under its own
viewport and scissor. A receiver (the terrain and every instance) offsets its point 1.5 texels
along its normal, adds a 5 cm bias and takes four rotated Poisson taps, each a hardware 2 × 2
comparison (interleaved gradient noise, turned every frame for the TAA to average); the near
cascade where the point falls inside it, else the far one, and the darkest of the figures'
tiles. Under the horizon nothing is drawn or read. On the software adapter a cascade's redraw
costs 2 to 30 ms (near) and 50 to 85 ms (far), which is why they are cached; the atlas costs
under 3 ms. The `wood` view (`games/fall_line/app/views.cpp`) stands across the sun from the
sunlit edge of a wood, where the trees' shadows lie on the snow.

**As built (M7b)** (`render/horizon.hpp`): the horizon map. A turn is split into 180 bearings
(2° each); the maps for the two bearings either side of the sun's sit in two `RGBA16Float`
textures, each bearing in the slot of its index's parity, so a step of the sun into the next
interval rebakes one map and leaves the other's bits alone. A map's texel is every second
sample of the field (Fall Line: 751 × 751 at 4 m over its 3 km map); the bake marches 96
samples toward the bearing, growing geometrically from one texel to 8 km, reads the field's
bilinear heights mirrored past the edge (as the terrain draws them), takes the planet's
curvature off each sample and keeps the highest tangent of elevation. The terrain's
fragment shader (`ground_shadow`, in `wgsl/terrain_common.wgsl`, which the instanced draws read too) turns each map into a soft step where the sun's tangent
crosses it (a penumbra of 0.008, the sun's disc and the grid's interpolation) and blends the
two by where the sun lies between their bearings. A bake is a whole map in one dispatch, in
its own `terrain.horizon` compute pass, only on a frame that needs one: two at load, then one
each time the sun crosses a 2° step (on seed 38, 69 bakes from 7 to 17 h, about one every
nine minutes of the day; the `shadows` lab counts them). On the software adapter a bake takes about 0.4 s; real-GPU
timings, and slicing a bake over frames if one shows as a hitch, wait for the `profile` lab
(M7d3). Past the map's edge the ground is drawn mirrored and so are its maps: the shadows
there fall at the mirrored bearing, which only the distant ground shows until the far
ranges replace it (M8).

## Sky, atmosphere and clouds

- **A physically based sky from lookup tables** (the scheme of Hillaire, 2020), all
  built by compute passes: the transmittance and multiple-scattering tables once at
  load, the sky-view table when the sun moves more than a threshold, and the
  **aerial-perspective volume** (a low-resolution froxel grid of in-scattered light and
  transmittance) every frame for the camera.
- **Haze is one function.** Every world material applies the aerial-perspective volume
  through a shared WGSL function. No material has its own fog.
- **Clouds are a raymarched volume at a quarter of the pixels.** Each frame marches a
  sixteenth of them, picked from a blue-noise sequence, and reprojects the rest from
  the previous frame, clamped by the fresh samples round each pixel. On a camera cut the
  whole buffer is marched again. The noise volumes are built by compute at load.
- **The night sky** (moon, stars, the galaxy band) is drawn into the sky's pass.

**As built (M7a)** (`render/sky.hpp`, `render/wgsl/atmosphere.wgsl`): an Earth-like air
(Rayleigh, Mie, an ozone layer) over a planet of 6360 km, the eye's altitude from the map's
own (`FrameView::altitude_m`). Tables in `RGBA16Float`: transmittance 256 × 64 and multiple
scattering 32 × 32 once; the sky view 192 × 108 (rows crowded at the horizon, columns round
the sun) and the **sky's light** (the irradiance an up-facing surface gets from the sky, and
the sun's transmittance at the eye, in a small storage buffer) when the sun turns past
half a degree or the eye moves 50 m; the aerial volume 32³ over the screen, its slices
spaced as the square of their index out to 8 km, every frame. Every luminance is per unit
of the sun's illuminance (`FrameView::sun_illuminance` scales them all). The sky pass is a
full-screen triangle at the far plane tested `equal`, with the sun's disc. `frame.wgsl`
holds the one copy of the haze, the sun's light at a point and the sky's irradiance.

## Anti-aliasing and resolution

**Temporal anti-aliasing with upscaling (TAA-U)** is the default. Each frame's
projection is jittered on a Halton sequence, a motion-vector target is written in the
depth prepass and the opaque pass, and the resolve pass blends the jittered frame into
a history at output resolution. The history is rejected by clamping it to the current
frame's neighbourhood in YCoCg space, and a reactive mask from particles and the snow's
glitter keeps them from smearing. Rendering at 50–100% of the output size, with this
pass reconstructing the rest, is the largest single lever on GPU time. It also removes
the shimmer of thin branches and glitter, which no amount of MSAA fixes.

The resolve is our own compact pass, written for this pass table. If it falls short of
the look, the open FSR 2 algorithm (MIT licence) is the reference to port, measured
against ours in the lab.

**As built (M7c)** (`render/taa.hpp`): the jitter is the Halton (2, 3) cycle, centred, of
8 points at full size and `ceil(8 / scale²)` (at most 32) below it, applied to the projection
as a sub-pixel shift (`fill_uniforms`'s `jitter_px`). The resolve is one compute pass at the
output's size. Each output pixel reconstructs the frame from the 3 × 3 render pixels nearest
it, each weighed by a Gaussian of its sample's distance with the jitter undone and by the
inverse of its exposed brightness (`frame.wgsl`'s `exposure`, now shared with post); it takes
the motion at the nearest depth of the 3 × 3, samples the history Catmull-Rom (five bilinear
taps) and clips it into the YCoCg box of mean ± 1.25 σ; it blends 10 % of the new frame,
more where the reactive mask asks, all of it off the screen or after a reset, and while the
history is young a plain running mean, so a still converges in one cycle. Two histories
ping-pong; post reads the one written. On a cut the game calls `reset_taa` and drops its
`FrameHistory` (`fall_line::app::cut`). The `taa` lab flies a camera and measures the moving
frame against the converged still (on seed 38's vista at 40 m/s turning 20°/s: 0.45 levels
mean, 0.9 % of pixels off by more than 8, thin lines along moving ridges) and a single
jittered frame against it (0.78 levels, 2.3 %). At 50 % scale the `world` lab's views are
hard to tell from full size by eye; what each scale costs on a real GPU waits for `profile`.

**4× MSAA with no temporal pass** is the fallback for the lowest tier, where a phone's
tile memory resolves MSAA almost for free.

## Post

One full-screen fragment pass reads the resolved HDR frame and writes the swapchain image
(a fragment pass, not a compute one: a compute shader may not store to the surface's
`BGRA8Unorm` without an optional feature, and a tile GPU keeps a full-screen fragment pass
in tile memory):

- exposure from the sky's brightness, not from a histogram, so it never pumps;
- bloom from a half-resolution mip chain (dual-filter down and up), cheap and stable;
- the colour grade as a 32³ 3D lookup table the game bakes from its look;
- tone mapping, a blue-noise dither against banding, and the output transfer function.

**As built (M7a)** (`render/post.hpp`): the exposure maps a white Lambertian surface under
the sun and the sky (read from the sky's light) to 0.78; the tone curve is the Khronos PBR
neutral operator (linear to 0.76, a smooth shoulder above, so snow keeps its colour); the
dither is interleaved gradient noise until the blue-noise texture lands; the sRGB encoding
is an `override` constant off for an `*Srgb` output.

**As built (M7d3)** (`render/bloom.hpp`, `render/post.hpp`). **Bloom** is a compute pass
(`bloom`) over a chain of half-size `RGBA16Float` mips of what the post pass reads (six levels
in Fall Line: half the output's size down to a 64th). Down, each level is the 13-tap filter of
the one above (four overlapping 2 × 2 boxes and a centre one, each a bilinear read); the first
level weighs each box by 1 / (1 + its brightness), so a lone glint cannot flicker into a blob.
Up, each level adds the level below through a 3 × 3 tent to its own down level, and alpha
counts the levels summed. The post pass mixes `strength` of the average in (Fall Line: 0.04):
nothing is thresholded, so the mix conserves energy (a flat field keeps its value; the test
holds it to one 8-bit level) and only what is much brighter than its surroundings visibly
glows. **The grade** is a 32³ `RGBA16Float` lookup table read trilinearly at the tone-mapped,
sRGB-encoded colour, so its steps are spent where the eye judges colour; it starts as the
identity, and the game bakes its own (`fall_line::app::bake_grade`: a little contrast
through the middle tones with the ends held, cooler shadows, warmer highlights, 5 % more
saturation). On the software adapter at 1280 × 720 the bloom costs about 11 ms and the post
pass 7 ms (the `profile` lab); the bloom's cost on a real GPU waits for the Mac's numbers.

Linear light is used throughout, and the conversion happens here, once.

## Particles and figures

- **Particles are simulated in compute** in fixed pools: emission appends to a ring, the
  simulation integrates and kills, and a compaction step writes the indirect draw count.
  They are drawn as camera-facing quads, softened against the depth buffer. Sorting is
  avoided by blend choice where possible; where a pool needs it, a bitonic sort in
  compute orders it.
- **Skinning** uses bone palettes in a storage buffer. A figure drawn in more than one
  view (the player in the main view, the sun's cascade and the shadow atlas) is skinned
  once by a compute pass into a vertex buffer that every view reads. Crowd members,
  drawn at lower detail and in larger numbers, are skinned in the vertex shader from
  their instance's palette.

**As built (M7d1).** Until skinning lands (M8) a skier is a **placeholder figure**
(`plugins/skier/app/figure.hpp`): 17 rigid pieces (skis, boots, shins, thighs, pelvis,
torso, arms, head, poles), each a unit box, cylinder or ball scaled to its size and posed
every frame from the `SkierState` (the skis on the snow's normal rolled by the edge, the
body rolled by the lean, the tuck folding the legs and the torso). Fall Line draws every
racer's pieces as one instance family (`games/fall_line/app/scene.hpp`), a jacket colour a
racer, written each frame; a cut writes them fresh so no motion is carried across it.

## Shaders

- **WGSL**, one file per pass, beside the pass's C++. A small build step resolves
  `#include` lines and embeds the result as a string. The engine never reads a shader
  file at run time.
- **Validated at build time.** `make shaders-check` compiles every WGSL file with Tint
  offline, so a shader error fails CI with a file and line, without a GPU.
- **Few permutations.** Quality switches are WGSL `override` constants set at pipeline
  creation, not text variants of a shader. Every combination the game uses is in the
  pipeline manifest.
- **One shared library** of WGSL functions for what many passes need: the haze, the
  sun's light, shadow lookups, octahedral decoding, blue noise. Each function exists
  once.

## The quality ladder

The game's settings rows (resolution scale, terrain reach, shadows, vegetation detail,
particles, clouds, anti-aliasing) are one data table in the app. Each row's stops map to
pass parameters and pipeline `override` constants, never to code paths. An automatic
preset measures a few frames per pass with timestamp queries and chooses the stops
that fit the display's budget. Each stop's price comes from a measured table that the
benchmark lab fills.

## Measurement

- Every pass in the table is wrapped in a timestamp query pair, so GPU time per pass is
  always available when the adapter supports it.
- Each frame keeps counters: passes, draws, dispatches, culled and drawn instances
  (read back from the indirect buffers a frame late), bytes uploaded, and pipelines
  created. The developer UI and the lab tests read the same struct.
- Each pass's CPU time is kept beside its GPU time: the recorder times the encoding from one
  pass's open to the next's, and the prepare before the first pass (`FrameStats::prepare_ms`).
  The prepare's own parts are timing zones (`engine/prof`, `GE_ZONE`), which the game's
  `profile` lab folds per frame with the simulation's zones between frames; `ge profile
  --render` runs it.
- **Budgets.** On the reference desktop GPU (the M3 Max), the top of the picture ladder
  at 1440p output fits in 8.3 ms of GPU (120 Hz). On a recent phone or tablet, the
  middle of the ladder fits in 12 ms of GPU, leaving thermal headroom under a 16.6 ms
  (60 Hz) frame. The renderer's CPU half
  (every `prepare` and `encode`) stays under 1 ms on one core, whatever the instance
  counts. The `perf` skill says how a claim against these budgets is proven.

## Where it is developed

**The main development environment is a Linux container with no GPU.** Every pass must
be buildable, runnable and verifiable by screenshot there. A secondary environment is
an Apple M3 Max laptop on macOS (Metal), used only for what the container cannot do.

**In the Linux container:**

- Dawn draws on a **software Vulkan adapter: Mesa's lavapipe** (it names itself
  `llvmpipe`), from Ubuntu 24.04's `mesa-vulkan-drivers` package, the same package the
  development container and CI's Linux x64 rows install, so a golden image made in one
  matches the other. Lavapipe was chosen over SwiftShader because it is a system package
  (no extra build of Dawn) and has the optional features the tiers test (timestamps,
  subgroups, f16). `gpu::create_device` prints the adapter a run used
  (`gpu::adapter_line`), and the golden tests compare pixels only on it.
- Software rendering is one to two orders of magnitude slower than a GPU, and the
  container has a few cores. Labs therefore render at a small default size (640×360),
  with the fewest settle frames that make a still repeatable, and a lab can name the
  views and passes it needs, so it does not draw the whole frame for one target.
- Every feature tier's fallback path runs here, since a software adapter lacks some
  optional features. A lab can also force a feature off, so both paths stay covered.
- **What it can verify:** that a pass builds and validates, every pipeline in the
  manifest, every named target, and the picture (by capture and golden comparison). It
  can count draws, instances and triangles, and check that nothing is created or
  allocated mid-run.
- **What it cannot verify:** speed. GPU milliseconds from a software adapter say
  nothing about a real GPU, so no performance claim is made from them.

**What needs the Mac**, and nothing else:

| Task | Why the container cannot do it |
| --- | --- |
| A GPU performance claim: per-pass ms, the frame budget, the quality ladder's price table, the automatic preset | Software timings do not predict a real GPU |
| A bug that shows only on Metal, or only on a real GPU (precision, filtering, driver behaviour) | The container has neither |
| The final look sign-off at full resolution and the top of the ladder, where a software render is too slow to iterate | Practical only on real hardware |
| Running the desktop app interactively (input feel, vsync, frame pacing) | No display, no GPU |
| Building, signing and running the iOS, iPadOS and Mac App Store builds on hardware | Needs Xcode and Apple devices (CI's macOS runner can build, not run on a device) |

When a task needs the Mac, the agent says so to the person, and opens a GitHub issue
labelled `needs-mac` with the exact commands to run, what to look at, and what to paste
back. Speed alone is not a reason: work that is only slower in the container stays in
the container. A check that needs a physical phone or tablet (Android or iOS) is
handled the same way, with an issue labelled `needs-device`.

## Lab hooks

The renderer is built to be photographed by the engine's labs. The labs own the
contract (scenes, captions, sheets). The renderer's half is:

- **Offscreen frames.** A frame can render into an offscreen target of a given size,
  with no window and no surface, so a lab or a test runs headless.
- **Named targets.** Every target in the pass table has a name (`final`, `depth`,
  `motion`, `shadow.atlas`, `sky.view`, `trails.fine`, …). `capture(name)` reads one back
  to the CPU after the frame, and a debug view can show any of them on screen.
- **Repeatable pictures.** The renderer reads time only from the `FrameView`, never a
  clock. TAA's jitter is indexed by frame number. `reset_history()` drops every temporal
  buffer (TAA, clouds) at a cut, and a capture renders a fixed number of settle frames
  first. The same scene, settings and frame count give the same picture on the same
  driver.
- **Distance with every capture.** `read_distance` (`render/frame.hpp`) reads the frame's
  depth back and unprojects each pixel to how far from the eye it lies (`depth_distance` in
  `render/view.hpp`: the near plane's point through the pixel, scaled by 1 / depth; infinity
  for the sky). A lab hands it with the shot, and the picture diff (`engine/lab/diff.hpp`,
  `docs/labs.md` § The picture diff) judges a change band by band, so far scenery made cheaper
  is held to a looser floor than what is at the skis.
- **Per-pass stats with every capture**: GPU ms, draws, instances and triangles, so a
  lab sheet can show what a picture costs beside the picture.
- **Golden captures on the software adapter.** The development container and CI's
  Linux x64 row run the same pinned software adapter, so a golden image regenerated in
  the container is the one CI compares against. The comparison is within a tolerance:
  a per-pixel delta and a cap on the share of pixels that exceed it. Pictures from
  different drivers (the Mac's Metal included) are never compared bit for bit.

## Where the code goes

| Home | Holds |
| --- | --- |
| `engine/gpu/` | The `webgpu.h` wrapper: the device and its feature tiers, RAII handles, the pipeline manifest and creation counter, timestamp queries, staging uploads, readback. The only directory that includes `webgpu.h` |
| `engine/render/` | Mechanisms any game could use: the pass table and named targets, the frame's stats, GPU-driven instance families and their culling, the depth pyramid, the heightfield clipmap renderer, the horizon map, cascaded shadows and the shadow atlas, the sky tables and aerial perspective, the cloud volume, TAA-U, post, compute particles, skinning, and the shared WGSL library |
| `plugins/<name>/app/` | A plugin's presentation half, shared by every game that links the plugin: `mountain/app` holds the snow material, the trail maps and their stamping, and the procedural tree shapes; `skier/app` holds the figure, the tracks' prints and the spray emitters (`docs/architecture.md` § Plugins) |
| `games/fall_line/app/` | The game's own: its pass table, its quality ladder's values, its grade, and whatever draws only this game has |

A piece goes in `engine/render` only if it carries no genre's words. A heightfield, an
instance family and a cloud volume are generic. Snow, a piste and a ski are not.

## Dependencies

Each library is added in the milestone that first needs it, pinned to a tag.

| Library | Licence | For | Milestone |
| --- | --- | --- | --- |
| Dawn | BSD-3-Clause | The GPU API on every target (prebuilt on the desktop, from source for iOS, iPadOS and Android); Tint for `make shaders-check` | M6 |
| SDL3 | zlib | The window and the surface (already the platform layer) | M6 |
| cgltf | MIT | Loading authored glTF models (`cmake/Cgltf.cmake`: one header, pinned by commit and SHA-256) | M8e1 |
| Dear ImGui | MIT | The developer UI, behind `GE_DEV_UI` | M10 |

Considered and not taken now: meshoptimizer (added if an authored mesh needs LOD
simplification or vertex-cache ordering that the procedural builders cannot do
themselves), Tracy (the engine's own zones, `engine/prof`, answer through `ge` headless and
need no viewer), the FidelityFX SDK (its algorithms are a reference, not a dependency),
any render-graph or scene library, and any shader cross-compiler.

## Considered and not taken

| Technique | Why not, for this workload |
| --- | --- |
| Deferred shading | A handful of lights does not repay a G-buffer's bandwidth, which tile-based GPUs pay most for; forward also keeps MSAA and transparency simple |
| Visibility buffer | Its benefit over a depth prepass with an `equal` test is small for this scene's few materials, and it needs 64-bit atomics or a large render target |
| Virtual shadow maps | They need a page table, sparse allocation and feedback; the horizon map plus cached cascades plus the figures' atlas give the same sharpness where it is seen, at a fraction of the complexity |
| Meshlet (cluster) culling | Instances are many and small, so instance-level culling removes the work; clusters would pay off only for large unique meshes, which the game does not have |
| Screen-space GI or reflections | Snow under open sky is lit by the sun and the sky's irradiance table; the look does not call for them |
| Async compute | Dawn's API has one queue |

Each of these can be revisited when a measurement on this workload shows the current
technique is the bottleneck. Record the measurement in the PR that does it.
