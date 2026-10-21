# Rendering

How the engine draws a frame: the GPU backend, the frame's passes, and the techniques
each pass uses. Nothing here is built yet. Milestones M6 to M8 in `docs/goals.md` build
it, and this page is the plan they build to. Change this page in the same PR that
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
| Snow marks | Trail and deformation maps stamped from the simulation's contacts every frame | Render-to-texture or compute writes, in a window that follows the camera |
| Particles | Tens of thousands of spray, snowfall and cloud particles | Overdraw of soft transparent quads; simulated on the GPU |
| Sky | A physical atmosphere, a raymarched cloud volume, a night sky, aerial perspective on every material | Lookup tables computed on the GPU; a temporal cloud buffer |
| Light | One sun or moon, the sky's light, at most a handful of point lamps at night | No need for a many-light pipeline |
| Shadows | The mountain shading itself over kilometres, sharp shadows under the figures | Two different problems, solved by two different techniques |

Measured on this scene, the costs that matter most are: 200–350 draw calls and about
1 M triangles a frame when meshes are drawn one by one; a sun shadow pass that redraws
most of the scene; CPU time spent choosing instances; shader programs compiled in the
middle of a run, which cause visible hitches; and, on a phone, the ground's fragment
shader taking more than half of the GPU's time. Each of those has a direct answer below.

## The backend: WebGPU through Dawn

**Decision: WebGPU through the standard `webgpu.h` C header, implemented by Dawn on
every target.** Desktop builds link Dawn's prebuilt release, and the web build uses
emdawnwebgpu, Dawn's own Emscripten port. Both use the same implementation, the same
WGSL compiler (Tint) and the same validation, so a shader that compiles on the desktop
compiles in the browser.

Why:

- **One API on desktop and in the browser** (G5). WebGPU is the only modern API that a
  browser exposes. Writing a Vulkan, Metal and D3D12 renderer for the desktop would
  still leave the web build needing a fourth.
- **Dawn already is a thin RHI.** It translates to D3D12, Metal and Vulkan, tracks
  resource state and places barriers, and validates every call with an error message
  that names the object. Writing our own layer over three native APIs would duplicate
  that work, and agents would have to maintain three backends.
- **Native extensions without forking the code.** Dawn native exposes features that are
  not, or not yet, on the web (multi-draw indirect, subgroups, push-constant-like
  "immediates", timestamps inside passes). The engine turns them on per adapter and
  keeps a path for when they are missing (§ Feature tiers).
- **A software GPU for CI.** Dawn runs on Mesa's lavapipe (a Vulkan driver on the CPU)
  on Linux. CI runners have no GPU, but they can run the renderer and capture
  screenshots with it (§ Lab hooks).

**What we give up**, and why it is acceptable for this workload:

| Missing from WebGPU | What it would buy | Why the game does not need it |
| --- | --- | --- |
| Bindless resources | Any texture from any shader with no binding | Few materials; texture arrays per size class and a material table cover it (§ Geometry and resources) |
| Mesh shaders | Meshlet culling inside the pipeline | Compute culling plus indirect draws gets the same culling at instance granularity, which is what a forest of identical shapes needs |
| Ray tracing | RT shadows, reflections, GI | Outdoor snow under one sun; the techniques below cover the look |
| Async compute queues | Overlapping compute and graphics | One queue. Compute work is ordered so it sits where the GPU would otherwise be idle, and a frame is small enough that overlap gains little |
| Explicit barriers | Hand-tuned synchronisation | Dawn places them. Passes are ordered so that independent work is not forced to wait |

If one of these gaps ever blocks a measured goal, the escape hatch is a Dawn-native
extension behind a feature tier, never a second graphics API.

**Alternatives** (the full comparison is in `docs/research.md` § GPU API):

- **wgpu-native** implements the same header. It is not used, because two
  implementations would double the platform matrix for shader and validation
  behaviour, and the web build is Dawn's anyway. Code that keeps to the standard header
  keeps the option open.
- **SDL3's GPU API** (stable since SDL 3.2) targets Vulkan, Metal and D3D12, but it has
  no web backend and needs a shader cross-compiler in the build.
- **bgfx, sokol_gfx, Diligent, Filament**: each brings its own shader dialect and build,
  and each either has no WebGPU backend or limits it to the subset its other backends
  share.

**Where Dawn is built.** The prebuilt release covers Linux x64, Windows x64 and macOS
(x64 and arm64). On any other CI row, `GE_GPU=OFF` builds and tests everything except
`engine/gpu`, `engine/render` and the app, until a cached source build of Dawn is
worth its CI time.

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
| `texture-compression-bc` / `-astc` / `-etc2` | Compressed textures for authored assets | Uncompressed RGBA8 |

Only core WebGPU is a hard requirement. Compatibility mode (WebGPU over GLES or D3D11)
is not targeted, because its limits (few or no storage buffers in the vertex stage)
would rule out vertex pulling.

## The frame

The frame is a fixed, ordered **pass table**: one function in the game's app lists the
passes in order, and each pass declares the named targets it reads and writes. Dawn
schedules barriers from those uses. There is no general render graph; the table is the
graph, written out where an agent can read it. A pass has two halves:

```cpp
void prepare(Pass&, const FrameView&, Upload&);   // CPU: decide, fill staging; testable with no GPU
void encode(Pass&, gpu::Encoder&);                 // GPU: bind and dispatch or draw; makes no decisions
```

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
4. **Depth prepass** of the terrain and opaque instances, then a **depth pyramid**
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

**No pipeline is created after load.** Every pipeline the game can use is listed in a
manifest and created with `createRenderPipelineAsync` behind the loading card. The GPU
layer counts pipeline creations, and after load a creation is an error that names the
pipeline and says to add it to the manifest. A missing entry fails the lab tests
instead of causing a hitch in a run.

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

Snow marks are two **toroidal textures** that follow the camera: the trail map (where
the snow was cut and packed) and the deformation field (how high the snow stands).
A compute pass stamps each contact as a capsule into them, and wraps the addressing
instead of copying the texture as the window moves.

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

**4× MSAA with no temporal pass** is the fallback for the lowest tier, where a phone's
tile memory resolves MSAA almost for free.

## Post

One compute pass reads the resolved HDR frame and writes the swapchain image:

- exposure from the sky's brightness, not from a histogram, so it never pumps;
- bloom from a half-resolution mip chain (dual-filter down and up), cheap and stable;
- the colour grade as a 32³ 3D lookup table the game bakes from its look;
- tone mapping, a blue-noise dither against banding, and the output transfer function.

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
- CPU zones use Tracy behind `GE_TRACY=ON`.
- **Budgets.** On the reference desktop GPU, the top of the picture ladder at 1440p
  output fits in 8.3 ms of GPU (120 Hz). On an integrated or phone-class GPU in the
  browser, the middle of the ladder fits in 16.6 ms (60 Hz). The renderer's CPU half
  (every `prepare` and `encode`) stays under 1 ms on one core, whatever the instance
  counts. The `perf` skill says how a claim against these budgets is proven.

## Lab hooks

The renderer is built to be photographed by the engine's labs. The labs own the
contract (scenes, captions, sheets). The renderer's half is:

- **Offscreen frames.** A frame can render into an offscreen target of a given size,
  with no window and no surface, so a lab or a test runs headless.
- **Named targets.** Every target in the pass table has a name (`final`, `depth`,
  `motion`, `shadow.atlas`, `sky.view`, `snow.trail`, …). `capture(name)` reads one back
  to the CPU after the frame, and a debug view can show any of them on screen.
- **Repeatable pictures.** The renderer reads time only from the `FrameView`, never a
  clock. TAA's jitter is indexed by frame number. `reset_history()` drops every temporal
  buffer (TAA, clouds) at a cut, and a capture renders a fixed number of settle frames
  first. The same scene, settings and frame count give the same picture on the same
  driver.
- **Per-pass stats with every capture**: GPU ms, draws, instances and triangles, so a
  lab sheet can show what a picture costs beside the picture.
- **CI captures on lavapipe.** CI's Linux x64 row runs Dawn on Mesa's lavapipe and
  compares captures with committed golden images within a tolerance: a per-pixel delta
  and a cap on the share of pixels that exceed it. Pictures from different drivers are
  never compared bit for bit. Golden images are regenerated only on that row.

## Where the code goes

| Home | Holds |
| --- | --- |
| `engine/gpu/` | The `webgpu.h` wrapper: the device and its feature tiers, RAII handles, the pipeline manifest and creation counter, timestamp queries, staging uploads, readback. The only directory that includes `webgpu.h` |
| `engine/render/` | Mechanisms any game could use: the pass table and named targets, the frame's stats, GPU-driven instance families and their culling, the depth pyramid, the heightfield clipmap renderer, the horizon map, cascaded shadows and the shadow atlas, the sky tables and aerial perspective, the cloud volume, TAA-U, post, compute particles, skinning, and the shared WGSL library |
| `plugins/<genre>/app/` | A genre's looks, shared by every game of that genre: for a snowy mountain, the snow material, the trail and deformation maps and their stamping, and the procedural tree shapes |
| `games/fall_line/app/` | The game's own: its pass table, its quality ladder's values, its grade, and whatever draws only this game has |

A piece goes in `engine/render` only if it carries no genre's words. A heightfield, an
instance family and a cloud volume are generic. Snow, a piste and a ski are not.

## Dependencies

Each library is added in the milestone that first needs it, pinned to a tag.

| Library | Licence | For | Milestone |
| --- | --- | --- | --- |
| Dawn | BSD-3-Clause | WebGPU on desktop; emdawnwebgpu on the web; Tint for `make shaders-check` | M6 |
| SDL3 | zlib | The window and the surface (already the platform layer) | M6 |
| cgltf | MIT | Loading authored glTF meshes | M8 |
| Tracy | BSD-3-Clause | CPU and GPU zones, behind `GE_TRACY=ON` | M7 |
| Dear ImGui | MIT | The developer UI, behind `GE_DEV_UI` | M10 |

Considered and not taken now: meshoptimizer (added if an authored mesh needs LOD
simplification or vertex-cache ordering that the procedural builders cannot do
themselves), the FidelityFX SDK (its algorithms are a reference, not a dependency),
any render-graph or scene library, and any shader cross-compiler.

## Considered and not taken

| Technique | Why not, for this workload |
| --- | --- |
| Deferred shading | A handful of lights does not repay a G-buffer's bandwidth, which tile-based GPUs pay most for; forward also keeps MSAA and transparency simple |
| Visibility buffer | Its benefit over a depth prepass with an `equal` test is small for this scene's few materials, and it needs 64-bit atomics or a large render target |
| Virtual shadow maps | They need a page table, sparse allocation and feedback; the horizon map plus cached cascades plus the figures' atlas give the same sharpness where it is seen, at a fraction of the complexity |
| Meshlet (cluster) culling | Instances are many and small, so instance-level culling removes the work; clusters would pay off only for large unique meshes, which the game does not have |
| Screen-space GI or reflections | Snow under open sky is lit by the sun and the sky's irradiance table; the look does not call for them |
| Async compute | WebGPU has one queue |

Each of these can be revisited when a measurement on this workload shows the current
technique is the bottleneck. Record the measurement in the PR that does it.
