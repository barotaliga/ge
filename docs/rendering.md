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
| Particles | Tens of thousands of spray, snow, rain and cloud particles | Overdraw of soft transparent quads; simulated on the GPU |
| Grass | Hundreds of thousands of blades near the eye in summer, none stored: grown from a cover of triangles every frame (`docs/grass.md`) | A compute scatter with culling and distance thinning into indirect draws; the vertex stage bends each blade in the wind |
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
5b. **Grass** (`render/grass.hpp`, `docs/grass.md` § Drawn): its blades grown by
   `grass.scatter` (after the frame's uniforms; any time before the draw) and drawn by `grass`
   after the opaque colour, testing and writing the depth, as blades bent in the wind.
6. **Sky and clouds:** the sky behind the depth buffer; the cloud volume over it (marched at a
   quarter of the pixels, a quarter of those each frame and the rest reprojected from the last,
   then upsampled by depth and resolved by TAA: § Sky).
7. **Water** (`render/water.hpp`, `docs/water.md` § Drawn): the opaque frame's colour and
   depth copied (`water.copy`), then every body drawn over it (`water`), reading the copies
   for what lies under it and what it mirrors, testing and writing the depth, so what follows
   sits in front of or behind it; both rows run empty with no water in view. With the eye
   under a body, `water.under` redraws the picture as seen through the water
   (`render/underwater.hpp`).
8. **Transparent:** particles and glows, softened against the depth buffer.
9. **Temporal resolve:** anti-aliasing and upscaling to the output size (§ Anti-aliasing).
10. **Post:** one compute pass for exposure, bloom, the colour grade, tone mapping, dither
   and the output encoding (§ Post).
11. **UI:** the HUD and cards, at output resolution.

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
`taa`, `bloom` (M7d3), `post`; M8e2 adds `skinning` straight after `sky.tables`, writing
`figures.posed`, which every pass that reads the figures' family reads. The late phase's depth lands before the colour pass, so it is still shaded
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
hides about 9,000; the forest is twenty meshes (five forms, four variants each) at three LODs
each (`plugins/mountain/app/trees.hpp`), 9 to 384 triangles a tree.

**Seeing through (M26g)** (`set_see_through`, `SeeThrough`). A follow camera in a wood sees
crowns, not its subject. A family may be told the point the camera frames; every upright
instance whose crown holds the eye or crosses the line from the eye to that point (stopped
0.6 m short of it, so a tree beside the subject and not before him stays whole) keeps only a
dithered share of its pixels in the depth prepass, easing from all of them 1 m off the line
to 15 % on it. An instance stands as its axis (its position up its rotated +y by its y
scale) and its crown (the larger of its x and z scales), which is what a unit-sized upright
mesh at its true size is. The share is computed per instance in the vertex stage (a branch
on a uniform when the family is not seen through), uses the LOD band's dither, so in a band
the two LODs together keep just that share, and moves smoothly with the line, so nothing
pops; under TAA the screen door reads as a ghost of the tree. The shadow maps draw it whole,
so its shadow stays on the snow. `see_through_keep` is the CPU twin; the `camera-world` lab
measures how much of the skier each outside lens sees in the densest wood of seed 38: 80 to
93 % of him, against 0 to 33 % with the trees drawn whole.

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
- **A base colour texture is baked into parts at load** (`assets/image.hpp`, its PNG reader
  palettes included). Each triangle takes the texel at its middle (NEAREST, repeating, through
  the texture's KHR_texture_transform), linear and times the base colour factor, as a material
  of its own colour (`<material>#rrggbb`, at most 256 a model), and a vertex two colours share
  is split. A low-poly kit's colour atlas (flat swatches, the common free kits' one 512 x 512
  palette PNG) then draws exactly in every pass that draws parts (the instances, the skinning,
  the shadows, the `models` lab) with no texture on the GPU and nothing more a frame. Not
  taken, for now: sampling the texture per pixel, which needs UVs in the packed vertex and a
  texture binding in every instanced pass; it waits on a model whose texture is a picture
  rather than an atlas (the loader notes one: a face whose corners read other colours than its
  middle).
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
  by `ge asset build`, fed the game's own numbers as a data set (`GE_ASSET_DATA`, compiled into
  the `assets` executable), so a skeleton or a ski's sidecut is stated once, in the code. The
  committed `.glb` is the engine's input, beside a STAMP of the hashes of everything it was
  built from and under a size BUDGET its manifest states (`engine/assets/manifest.hpp`); the
  `assets_check` test fails a model older than its sources or over its budget, and CI never
  runs Blender.

**As built (M8e1).** The loader, posing, the CPU reference and the `skinning` pass, held to
each other within one 16-bit step on the software adapter (`tests/render_skinning_test.cpp`),
and the `models` lab. **M8g** added `ge asset` and the first built model, the skier on the
reference skis (`plugins/skier/assets/skier/`), and **M8e2** its poser (`plugins/skier/app/rig_pose.hpp`: the rig posed from a `SkierState`, then `rig_locals` for the model, ready for the palette and the `skinning` pass) and the model drawn in Fall Line's frame:
the committed `skier.glb` is compiled into the game as bytes (`cmake/EmbedBinary.cmake`), the
figures' family holds one skinned mesh a racer packed into the poser's fixed reach
(`pose_reach`, held by `tests/skier_rig_pose_test.cpp` over the ride scenarios), each racer's
poser keeps its lags between frames (`FrameScene::dt`; `settle` for a still or a jump), his
palette is written before the frame is encoded and the `skinning` pass runs straight after the
sky's tables, so every view that reads the family (the culls, the prepass, the colour pass, the
figures' shadow atlas) reads the posed mesh. Each skinned vertex carries its own **motion**:
the family keeps a skinned LOD's last frame's positions after its vertices (two words a
vertex, `lod_last`; the bind's to start), the `skinning` pass copies the position it replaces
there before writing the new one, and the colour pass reads it (`Lod::motion`) for the
vertex's last place, so a limb that swings between frames is not smeared by TAA; a cut, a still
and a slot left undrawn settle the skin (`settle_skin`), so the next pose moves nothing. The
`taa` lab's `--scenario=turns` chases the skier through his turns: the ghosting against the
converged still falls from 0.45 to 0.35 levels (0.95% to 0.73% of the pixels off) with it.

**The crowd: palette families (M8e2).** Hundreds of figures of one mesh, each in a pose of its
own, are not a skin each: a **palette family** (`create_family`'s `palette_joints`) holds every
instance's palette in a storage buffer of its own, twice (this frame's half and the last
frame's, swapped by each `set_palettes`, both written after a cut). Its LODs (`palette_source`)
keep the bind pose packed into the reach box and, after the family's vertices, each vertex's
four joints as bytes and four weights as 16-bit unorms (three words a vertex). The draw's
vertex stage, shared by the prepass, the colour pass and every caster, poses each vertex by the
linear blend of its instance's palette rows, and by the last frame's rows for its motion, so no
pass runs and nothing is stored a figure: a palette costs 48 bytes a joint a frame to upload.
`pose_palette` is the CPU twin, within 1e-4 m of `skin_mesh`; `tests/render_palette_test.cpp`
draws two limbs in poses of their own as one palette family and through the `skinning` pass and
finds the same depth on every pixel either covers.

## The view model

A first-person game draws the weapon its player holds in front of the eye (`engine/viewmodel`,
row M48). Three questions decide how it looks, and each has one answer in one file:

- **What it is** (`held.hpp`): a `HeldModel`, morph frames (a position and a normal a vertex a
  frame) over shared triangles, coloured by a skin read at its UVs (its glowing texels lit by
  themselves) or by a colour a vertex. Any format is converted into it once, at load: a glTF
  posed into frames (`held_from_model`), a game's own morph format by its reader. `fit_mount`
  turns a model authored in any frame so its muzzle is +z and its up +y without mirroring it,
  scales it and puts the grip at the origin.
- **Where it is** (`place.hpp`): an affine from the held frame into the view's, a display frame
  at a time from the look, the eye's own bob and the moments (a shot, a landing, a weapon put
  away): the rest offset low and right, the bob pushed forward, the sway that lags a turn on a
  second-order follower (so a gun has weight), the kick on a stiff spring, the put-away at a
  steady rate. Presentation only; comfort (`cl_motion`) scales every motion the player did not
  command. A classic row and a modern one are the starting points.
- **How it is lit** (`light.hpp`): by the place the player stands in, never by a fixed lamp. A
  game samples its baked light at the player (the lightmap under the feet, a light grid's cell,
  the sky and the sun) as a probe of an ambient and a direct colour, and hands its local lights
  (torches, lamps, a muzzle flash) on the same fall-off law as the renderer's local lights. The
  strongest eight at the grip are kept, a floor keeps a gun in the dark a silhouette, and the
  surface is a wrapped Lambert with a Blinn highlight.

**The draw** is two passes into one target: the world, then the depth cleared and the gun drawn
through its own lens, so it is never cut by a wall it is pushed into and a wide view does not
stretch it. `raster.hpp` is the CPU reference every lab draws with (`viewmodel`, on the stage's
corridor and its six named lights); the GPU pass (M48c) draws the same light per pixel over the
clustered local lights and is held to the reference.

**The muzzle** is where a game stages what a shot makes (the flash, its smoke, its light). A held
model names its tip as the centre of a few of its vertices (`muzzle_vertices`, the barrel's front
on the rest frame by `find_muzzle`, or the game's own), so `muzzle_at` follows every morph frame
the barrel does, and `muzzle_dir` is the way the shot leaves. The gun is drawn through its own
lens, so its tip is carried into the world by `held_to_world` / `held_dir_to_world`: placed into
the view, then moved across at its depth by tan(view / 2) / tan(gun / 2), so an effect drawn in
the world's pass sits on the same screen ray as the barrel. A model that draws its own flash (a
classic weapon's glowing triangles on its firing frames) marks them in `flame`, and a game whose
engine draws the flash hides them through `DrawLit::hidden`. The effect is drawn after the gun,
so a flame can overlap the barrel.

## Morph meshes

A **morph mesh** (`engine/render/morph.hpp`, `morph.wgsl`) is a model animated by whole frames:
every vertex has a position and a normal in every frame, as vertex-animated model formats store
them. Shooter's Edge draws its players, monsters and items this way (M18i). The pass is generic.
A game hands it `MorphModel`s (frames, triangles, texture coordinates, a skin) once, and each
frame a list of `MorphDraw`s.

- **Upload once.** Every frame of every model goes into one storage buffer, two `vec4` a vertex.
  The triangles and the coordinates go into two more. The skins are shelf-packed into one
  2048-wide atlas, tallest first, a texel apart (`pack_morph_atlas`). A frame writes only its
  draws: 160 bytes each, at most `kMaxMorphDraws`.
- **Frames blended on the GPU.** The vertex stage pulls each vertex by index and mixes the draw's
  two frames by its share. It does the same for the frame before, so a swinging limb carries its
  own motion into the motion target and TAA keeps it sharp. A model with frames at 10 a second
  is drawn between them at any rate.
- **Team colours without a skin per team.** A skin texel's alpha is its **mark**:
  - plain, an sRGB colour;
  - **glow**, which lights itself (eyes, lamps, a muzzle);
  - **tinted**, in region A or B with a step from 0 to 15.

  A tinted texel takes the instance's ramp for its region at its step. The step keeps the
  painter's shading and the ramp gives the colour. The ramps are a table of 16-colour rows the
  game fills, from its palette's colour rows or from any colour with `ramp_from`.
- **Sharp or smooth, per draw.** Sharp reads the texel the point falls in, which gives the
  classic look. Smooth blends the four nearest texels, each resolved to its colour first, so a
  tinted texel never bleeds the wrong ramp into its neighbour.
- **The game's own light.** `outdoor` is the share of the frame's sun and sky. `ambient` and a
  `shade` from one direction are what a game samples where the model stands: an indoor map's
  light at its feet. Glowing texels add `glow` times the scene's white, whatever the light.
- The draws go into the HDR and motion targets after the opaque world. Depth is tested and
  written (reversed z) and back faces are culled.

The CPU twins (`pose_morph`, `morph_texel`) are the shader's arithmetic for
`tests/render_morph_test.cpp`. The `gpu-morph` lab draws a runner in four teams, sharp beside
smooth, its frames and in-betweens, at dusk and in a game's own light, and times 64 of them.
**Not yet:** the local lights of the lighting pass, shadows cast by the figures, and the classic
model reader (it lands with its plugin).

## Sprites

A **sprite** (`engine/render/sprites.hpp`, `sprites.wgsl`) is a flat picture stood in the world:
one frame of a set of images, turned by a rule, cut out or blended. Shooter's Edge draws its
classic effects and pickups this way (M18i). The pass is generic. A game hands it its
`SpriteFrame`s once, and each frame a list of `SpriteDraw`s.

- **One atlas.** Every frame is shelf-packed into one 2048-wide atlas, tallest first, a texel
  apart (`pack_sprite_atlas`). A frame has an ORIGIN, the texel that stands at the sprite's
  position (a flame's foot, a ball's middle). An animation is a run of frames, and which one
  shows is the game's choice.
- **Five facing rules** (`SpriteFacing`), all worked out on the CPU (`sprite_basis`):
  - `parallel` lies in the screen's plane;
  - `upright` stands on the world's up, turned to face the eye's point;
  - `parallel_upright` stands on the world's up, turned with the camera's heading;
  - `oriented` is fixed by the draw's own rotation;
  - `parallel_oriented` lies in the screen's plane, rolled by the draw's `roll`.

  The shader only builds the quad from the right, up and extent staged for it.
- **Three blends** (`SpriteBlend`):
  - `cutout` is alpha-tested at half coverage, writes depth and its own motion for TAA, and is
    drawn first, in one draw.
  - `over` (premultiplied) and `add` are drawn after the cutouts, sorted far to near, in one
    draw through one blend state. An added sprite's alpha out is 0, so the same blend adds it.
    They write no depth, and they set the motion target's reactive channel as particles do.
- **Light.** `glow` times the scene's white (fullbright), plus `outdoor` of the sky and sun on
  the face turned to the eye, plus a game's `ambient`. The haze applies. An added sprite only
  fades with distance, because the air adds nothing to light that is itself added.
- **Sharp or smooth**, per draw. Smooth is bilinear over premultiplied texels, so a cutout's
  edge never darkens.
- The pass runs after the sky and the clouds, where the particles are drawn: the blended
  sprites lie over whatever is behind them, the sky included. At most `kMaxSprites` (4096) are
  drawn a frame. The sort allocates nothing.

`tests/render_sprites_test.cpp` holds the five rules and the atlas. The `gpu-sprites` lab draws
every kind on a yard by day, from the eye's height and from above, sharp beside smooth, a
fireball's frames and the yard at dusk, and times 2000 sprites.

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

**Forward shading with clustered local lights.** The sun or moon, the sky's light (read
from the sky's irradiance table), and the local lights (§ Local lights) a game sets, each pixel
reading only the lamps its cluster lists. Forward shading keeps the lowest bandwidth, works with
MSAA and transparency, and suits tile-based GPUs; the clusters make a town's hundreds of lamps
cost a pixel only the few that reach it. Every surface answers all three through one physically
based model (§ Materials).

### Local lights

**As built (M39a)** (`render/lights.hpp`, `wgsl/lights.wgsl`). A game keeps its own list of
`LocalLight`s and hands it to `prepare_lights` each frame; nothing about a light is stored
between frames.

- **Units.** A light's `candela` is its luminous intensity; a point `d` m off gets candela / d²
  lux, the units the sun's 128 000 lx is in, and the shader pre-exposes it as it does the sun. A
  torch is 10 to 40 cd, a bulb about 70, a street lamp 1000 to 3000, a muzzle flash 10⁴ to 10⁵
  for a few milliseconds. Every light ends at its `range_m` by the window (1 − (d/r)⁴)², and the
  inverse square stops at the emitter's `radius_m`. A spot is whole inside `inner_deg` of its
  axis and dark past `outer_deg`, smoothstepped between.
- **Flicker** is `flicker_level(light, t)`, a pure function of the light and the frame's time
  computed on the CPU (so the shader reads one colour a light, and a replay or a lab flickers
  the same): a flame (two octaves of smooth noise and a slow breath, its own seed), a buzz (a
  failing tube's short dark gaps), a strobe, a pulse (up in `attack_s`, down by e every
  `decay_s`: a muzzle flash, a spark, an explosion; dropped once under a thousandth), and steps,
  a pattern of letters 'a' (dark) to 'z' played at `rate_hz`, 'm' its full level.
- **The clusters.** The view is cut into 16 × 9 tiles and 24 depth slices (0 to 2 m, then
  logarithmic to 2 km, the last on to infinity). `bin_lights` (CPU) keeps the lit lights whose
  sphere reaches the frustum, the nearest 256, walks each one's slices and the tiles its
  sphere's box covers and tests the sphere against each cluster's box in view space. One storage
  buffer holds the head, the lights, each cluster's (first, count) and the lists; it is group 0's
  binding 10 (`frame_group`'s `lights`, visible to fragment and compute stages but not the
  vertex stage, whose storage-buffer count is 8 on the smallest devices), and a frame with no
  lights binds the sky's empty list. A pixel finds its cluster from its position and its
  reversed-z depth (distance = near / z), so no extra target is read. The binning is checked
  both ways: `tests/render_lights_test.cpp` against points inside every light's sphere, and the
  `gpu-lights` lab off the depth read back, every pixel a lamp reaches finding that lamp.
- **Readers.** `local_lights(surface, p, n, v, frag)` sums every listed lamp through the
  surface's model; the instances, the plain terrain and the snow call it. `light_inscatter(p,
  dir, frag, g)` is the air's (a Henyey-Greenstein phase), for a fog volume's march.
- **The eye under lamps.** The exposure is read off the sky's light; a lit street at night
  needs the lamps' light in it too, or the eye is at the dark sky's gain and every lamp burns
  out. `FrameView::lamp_lux` is that light (lx on a white face); `lamp_lux_at` gives it at a
  point, and a game picks where its eye adapts (the `gpu-lights` lab takes the geometric mean
  of a few points of what it looks at). Metering it off the picture is the eye adaptation row's.

**Not yet**: local lights' shadows (a shadow atlas of spot and cube faces, cached for lamps that
stand still: M39c), and the lamps on particles and water.

### Materials

**As built (M39b)** (`render/surface.hpp`, `wgsl/surface.wgsl`). One physically based model for
every material: a Lambert body weighed by (1 − F) and by how little metal it is, and a GGX lobe
with the height-correlated Smith visibility and Schlick's Fresnel. A dielectric reflects 0.08 ×
`spec` head on (0.5 is 4 %), a metal its albedo. The sky in a surface is its radiance along the
reflected ray, blurred toward the sky's irradiance as it roughens, times the split sum's
analytic environment term (no table to bake). A `Surface` is albedo, roughness, metal, spec, an
emissive luminance (cd/m², tinted by the albedo) and how porous the rain finds it; the presets
name stone, metal, wood, slime, a wet floor and a lamp's glowing face. An instance family takes
its materials as surfaces (two rows each); the rgb form makes every material matte (`spec` 0:
no lobe at all, not even at a grazing angle), so every picture made before the model is
unchanged. The `gpu-lights` lab's sphere sheet shows roughness across a dielectric, a metal and
the presets under the noon sun and under one lamp at night.

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
no depth pyramid, the figures' and the crowd's one a tile (sixteen), each culled against its own
tile's box, so a figure is drawn only into the tiles it can shade, each tile under its own
viewport and scissor. The tiles go to the racers first, then to the amateurs nearest the eye
(`games/fall_line/app/scene.hpp`'s `shadow_centres`). A receiver (the terrain and every instance) offsets its point 1.5 texels
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

### Baked light (`engine/bake`)

A static world's light is worked out once on the CPU and drawn from a lightmap atlas
(M40). The baker reads triangles, each with a material and the planar **chart** its luxels
live on, the lights and the sky's brightness, and writes two things a luxel: the
**irradiance** (direct light with soft shadows, then gather passes of bounced light coloured
by what it bounced off, the glow of emissive faces and the sky through sky faces) and the
light's **mean direction**, weighted by luminance, whose length says how much of the light
comes from one way. A renderer shades a face's detailed normal with it
(`directional_scale`: a half-Lambert lobe renormalised so the flat normal gets exactly the
baked irradiance), so relief and normal maps catch the room's real lamps at almost no cost.
Luxels inside walls (their rays mostly meet the backs of faces) and luxels off a face are
filled from their chart's neighbours, so bilinear reads never fetch black. The bake is the
same bits on any number of workers. The `bake` lab compares an old-style bake, the rebake
and its directional reading on a test hall.

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

**Sky types (M8f7)** (`render/sky_style.hpp`, `FrameView::style`): a world that is not the Earth
changes its AIR (scales on the Rayleigh scattering per channel, the dust's scattering,
absorption per channel, asymmetry and scale height, and the ozone), which `medium_at` reads, so
the tables are rebuilt in it and everything lit by the sky follows (the sky, the sun's colour,
the sky's light, the haze, the cloud's light). Over the sky the pass draws what an air cannot:
the sun's disc's size and tint, a second sun with a halo, a planet lit as a ball (its phase
fixed by its offset from the sun, banded, a ring whose near half crosses it, dimming half the
sky behind it), a nebula on the star sphere and the stars by day (both against the snow's
white, not physical), and a glow from under the horizon. Ten named types (`sky_types`) a game
picks by a word, knobs over them (`SkyKnobs`). The Earth's style is every scale 1 and every
extra 0, so its picture is the same to the bit.

**The weather in the picture (M8f1).** A frame is seen under a `Weather` (`render/view.hpp`,
in the frame block as `weather` and `deck`), and `wgsl/frame.wgsl` is the one place it acts, so
the ground, the trees, the figures, the particles, the flakes and the sky all see the same day:

- the cloud takes its share off the sun's direct light (`sun_light`), so the sun's shadows, the
  snow's glare and the particles' forward lobe fade with it; the glitter, the sun's disc in a
  crystal, goes faster (the pass squared);
- a DECK covers its share of the dome (`sky_luminance`): its grey at the horizon is the clear
  day's light on a level face over pi (so it follows the hour and the night) times the deck's
  tint and light, darker overhead, a pale patch where the sun is behind it; the sun's disc is
  lost behind it;
- the sky's light on a surface becomes the deck's (`sky_irradiance`: pi times its grey, the
  overhead share weighed as a level face sees it), with no direction but up: THE FLAT LIGHT;
- the air under a deck scatters the deck's grey, not the sun's (`haze`), and a FOG or a FALL
  closes every surface into its VEIL at its visual range V (`exp(-3.912 d / V)`: 2 % of a black
  target's contrast left at V). Droplets and flakes are far larger than light's wavelengths and
  scatter every colour alike, so the veil is grey: a white face's light under the sun and sky,
  dimmed to 0.85 (`veil_luminance`), never the clear air's blue. The sky is seen through a slab of
  it 800 m deep over the eye (`veil_sky`), so through a thick fall the blue overhead is lost too,
  and toward the horizon all of it;
- the exposure keys a white face under a deck to 0.6 rather than 0.78, so a grey day's snow is a
  light grey, not a white-out.

A clear `Weather` (the default) changes no bit of the picture. What the mountain's eight skies
are in these numbers is `plugins/mountain/app/weather_look.hpp`, and Fall Line's frame reads the
map's sky at the view's time (`FrameOptions::weather`). The `weather` lab draws all eight: from
seed 38's piste the clear day's picture spreads 37 levels of luma, an overcast's 15, a snow's 3
and a storm's 2. The cloud volume itself is M8f3 (below).

**The cloud in the picture (M8f3).** A frame is seen under up to four cloud layers
(`render/view.hpp`'s `Clouds`, in the frame block as `cloud_info` and `clouds`), drawn by
`render/clouds.hpp`:

- THE SHAPE is one tileable noise, a 64³ value-noise fbm made on the CPU (the same bytes on
  every machine; the frame's group 0 binds it with a repeating sampler beside the sky's tables).
  A layer reads it flat at its cell's scale for its COLUMN, the third axis its age, and stands
  where the noise passes the cut its cover sets: `cloud_cut` is the noise's own quantile, so the
  cover is a true share of the sky (the `weather` lab measures fair's dealt 0.29 as 0.28). Within
  its slab a HEAP grows from a flat base to a top the fuller the column the taller, a SHEET
  thickens about its middle, and two more reads at a quarter and an eighth of the cell eat its
  edge, harder toward the top.
- THE WIND carries each layer along its heading (the drift, kept within the noise's tile in
  double) and its shape changes as it goes (the age).
- THE LIGHT on a point of cloud is the key light through the air to its height, through the
  layers above (their shadow at its middle) and through its own cloud toward the light (four
  reads at growing steps): Beer's law for the light that has not scattered, and for the light
  scattered on (forward, so it dies at the reduced extinction σ(1 − g), g ≈ 0.85) one wide lobe
  standing for its many orders, so a thick heap is as bright as snow in the light; a powder term
  darkens a thin edge seen with the light behind the eye; the sky's light from above and the
  snow's from below. Far cloud is hazed at its mean distance and melts into the sky from 25 to
  55 km.
- THE SHADOW on everything lit (`cloud_shadow` in `wgsl/frame.wgsl`, read by `sun_light(y, xz)`)
  is each layer's column where the beam toward the key light crosses its middle, taking
  exp(−column × the layer's shadow depth) off the light: each place its own share of the sun.
- THE PASSES ("clouds.march" and "clouds", after the sky) march each ray, nearest layer first,
  up to what the depth target holds (28 steps through a heap, 10 through a sheet, a new offset
  every frame for TAA to resolve), and lay the cloud over the HDR target premultiplied. By
  default the march runs at A QUARTER OF THE PIXELS (`Clouds::quarter`): a full-screen triangle
  over a half-size target marches each two by two's ray to the FARTHEST of its four depths, so
  the cloud behind a ridge reaches the ridge's edge, and keeps the distance it marched to beside
  it (R32F). Then each pixel of the frame takes the four half-size pixels round it, each weighted
  by its share of the bilinear footprint and by exp(−40 × how far its distance misses the
  pixel's own, as a share of the nearer); a pixel that none of the four matches within 10 %
  (the near side of an edge) marches its own ray. Marching every pixel (the quality ladder's
  top) draws "clouds" alone and leaves "clouds.march" empty; with no layer both are empty. The
  ladder also takes a share of the steps (`Clouds::steps`, never under four) or draws no layer
  at all (§ The quality ladder). The `weather` lab holds the quarter to every pixel's march:
  under 1 % of the picture may differ by more than 8 levels of luma (`<sky>.quarter-misses`).
  On lavapipe at 1280 × 720, from seed 38's start looking up into a flurry's sky, every pixel's
  march costs 72 ms and the quarter's 36 (28 at half the steps): a CPU raster's numbers, where
  a full-screen pass costs about 13 ms whatever it does, so a GPU's saving is nearer the
  pixels' four to one.
- THE MARCH SPREAD OVER FRAMES (`Clouds::spread`, on by default, and only with the last frame's
  camera: `cloud_info.z`): each frame one of every two by two of the half-size pixels marches,
  in turn over four frames, and the other three take the last frame's half-size picture. The
  marching pixels have a pass of their own ("clouds.fresh", over a target a quarter of the
  half-size one), because a GPU runs a group of neighbouring pixels as long as its slowest: one
  marching pixel in every two by two of one pass makes every group march, and the first build,
  which did that, timed no cheaper than the whole march. The half-size pass ("clouds.march")
  then takes the fresh pixel where it is its turn, and elsewhere reprojects: its ray's point at
  the distance its cloud lies at (first guessed at the nearest layer's middle, then read back
  from the last frame and reprojected again) is carried through the last frame's camera, and
  the last frame's picture there is read bilinear over the taps that were written the frame
  before (a stamp, 1..4096, beside each) and marched to the same scene within 10 %; a pixel
  those taps hold under half of (off the screen, newly seen past a ridge) marches after all.
  The two half-size sets (RGBA16F cloud, RGBA32F distances and stamp) swap each frame. The
  `weather` lab holds the spread to every pixel's march as it holds the quarter, with the view
  swinging 30 degrees a second (`--turn=30`): at 1280 × 720 looking up into the sky, 0.2 % of
  the fair sky's picture and 0.27 % of the flurry's differ by more than 8 levels (0.01 % and
  0.27 % with the march whole every frame), the edges of the heaps and a pixel's dither. On
  lavapipe the half-size march falls from 12 to 16 ms to 3.4 to 4.8 for the fresh pass and 4.8
  to 5.6 for the half-size one, and the cloud's three passes from 22 to 26 ms to 17 to 19 (the
  full-size upsample, about 8, is the same either way); on a GPU, whose pass has no such floor,
  the march is nearer a quarter of its cost. In one ladder pricing run the clouds' high stop
  fell from 63 % of every pixel's march to 42 %.

What the mountain's broken skies stand is `plugins/mountain/app/cloud_field.hpp`: FAIR heaps at
1.5 to 2 km over the valley floor and a few ice streaks at 8 km, FLURRIES' taller heaps under a
rippled sheet at 4 km, a HIGH sky's streaks over a veil; every band dealt off the map's seed,
the layers carried at a quarter of the wind aloft (the map's mean wind grown and turned with
height) and living their kind's life. Where they are drawn a broken sky's mean cloud
(`weather_look`'s pass and cover) is given back, since the layers cast their own shadows; a
lid keeps its deck, and stands a sheet under it (OVERCAST a stratus, SNOW and a STORM a
nimbostratus) so the deck has a texture.

**Cloud kinds (M8f8)** (`render/cloud_kinds.hpp`): sixteen named kinds, each a layer's bands,
its form and its light, matched by eye against photographs of real skies side by side (the
photographs are kept out of the tree): inversion, fractus (the scud of M8f9), stratus, stratocumulus, cumulus, congestus,
cumulonimbus, nimbostratus, altocumulus, altostratus, lenticular, cirrocumulus, veil,
cirrostratus, cirrus and wisps. A kind's bands are its base over the valley floor, its depth, its cover and
its cell (m); its stretch along the wind, its dome (0 a sheet, 1 a heap), its optical depth,
its shadow, its life and its speed; four FORM knobs, each 0..1, carried in a fourth row of the
layer's frame block (`CloudLayer::anvil`, `erode`, `fibre`, `patch`); its LIGHT in a fifth
(`drop_um`, `drop_spread`, `halo`, `deck`); and its THICKNESS in a sixth (`thin`,
`thick_cells`, `thick_bias`), below:

- THE BILLOWS. The noise texture is two channels (RG8Unorm 64³): the column's value fbm, and a
  cellular (Worley) fbm at 4, 8 and 16 cells a tile. Two reads of it at ten times the cell's
  scale take a heap's edge away in rounded lobes, harder toward the top, by the kind's `erode`
  (a lenticular's 0.06 leaves it smooth as a lens, a cumulus's 0.6 cauliflowers it).
- THE STRANDS (`fibre`): the detail read stretched along the wind and squeezed across it, and
  the value noise's wisps in place of the billows, then the density cut into strands, so a
  cirrus is fibres and hooks rather than a smear.
- THE PATCHES (`patch`): a quarter-scale read of the noise masks the column, so a cirrocumulus
  or an altocumulus comes as fields of cells with clear sky between, never a uniform sheet.
- THE ANVIL (`anvil`): from 55 to 80 % of its depth up a heap's cut is lowered to 0.72 of
  itself, so a cumulonimbus spreads its top to several times its tower's width, thinning toward
  the rim, and a coarse read of the billows lowers the anvil's base cell by cell into hanging
  pouches (mammatus).
- THE COLUMN ramps from the cut to full over at most 0.3 of the noise (`kColumnRamp`), so a
  full sheet is evenly thick and does not break into the noise's blotches.
- THE LIGHT IN A SHEET: where a thin slab is marched along a long path the steps grow (up to
  three times) and bunch toward its ends; the light that has scattered many times in a sheet
  is diffusion's 1/(1 + σ′τ) along the path up through the slab (one more read halfway to its
  top) rather than the heap's forward exp(−σ′τ), and the sky's light from above dims through
  the same path, so a deck is grey under and bright on top. A thick sheet under a lid is
  blended toward the deck's own grey (half of the way), so the ground's flat light and the sheet
  overhead agree and the sheet keeps its texture.
- THE HALO: an ice layer lights a ring at 22° round the sun (and the moon, when it is the key
  light), its red inside its blue (a Gaussian of 0.9° at 21.7°, 22.0° and 22.4° for red, green
  and blue), as strong as its kind's `halo`: the veil's 1, cirrus's 0.8, cirrostratus's 0.4.
- THE GLOW ROUND THE DISC (`diffraction` in `clouds.wgsl`): half of what a particle much
  larger than the light's wavelength takes out of a beam is diffracted round its edge, so every
  layer in front of the sun or the moon adds the Airy pattern (k a)²/4π (2 J1(k a θ)/(k a θ))² of
  its particles, for red, green and blue, lit by the key light through the layer as its single
  scattering is: the aureole a few degrees wide and, where the particles are of one size, the
  corona's coloured rings round it. Each kind names its particles' radius and their spread (five
  sizes, a share either side, which wash the outer rings out): altocumulus's 8 µm droplets
  spread 0.1 and a lenticular's 6 µm spread 0.06 ring the moon in colour; altostratus's mixed
  14 µm spread 0.45 give the sun the soft glare of ground glass and no rings; the ice kinds'
  30–40 µm crystals a tight bright glare. The sun's glow dazzles as it is; the moon's is seen
  beside its disc as the disc is (`as_seen`, now in `wgsl/frame.wgsl`): the disc's brightness as
  seen times the glow's ratio to the disc's to the power 0.35 (`kGlowSeen`), never brighter than
  it is, so its rings show at night.
- THE DECK'S TOP (`deck`): the light that scatters on in a deck comes in through its top, which
  the sun lights at a slant, so in a layer whose kind is a deck (stratus, nimbostratus,
  altostratus, an inversion, and 0.6 of stratocumulus) that light is the sun's height's sine of
  the beam's: an altostratus under a low sun is grey with the sun a pale disc in it, not white
  over the whole sky. A heap's sides and a thin layer's edges face the sun at any height, so a
  high veil keeps its pink at sunset.
- THE THICKNESS (`cloud_thickness` in `wgsl/frame.wgsl`, mirrored by `render/clouds.hpp`): a
  layer's extinction is not even across it. A read of the noise a patch wide (`thick_cells`
  cells, set off from the column's read), a share of the column itself (so it thins toward its
  gaps) and the sky's lean (`thick_bias`, dealt per map in ±0.4, or `sv_clouddensity` in Fall Line)
  give each column a share of its kind's full optical depth between its thinnest patches
  (`thin`) and 1. The kinds that vary are the sheets: an altostratus from 1.6 (the sun as
  through frosted glass) to 32 (hidden), a stratus from 3.4 (a watery disc) to 28, a
  stratocumulus from 2.7 to 18, cirrus from 0.4 to 4; a storm's tower is never less than half
  its depth. The cloud's shadow on the ground (`cloud_cast`) is the column times its thickness,
  so a thin patch's shadow is faint.
- THROUGH IT, THE DISC: the sky draws the sun's and the moon's discs behind the cloud, so a
  disc shows by the ray's own transmittance, exp(−τ), wherever on the ray its light was taken:
  bright through a thin patch, gone behind a core. Two things used to leak it: the march stopping
  at a hundredth of the light (the rest of the path is now taken as thick as the ray's so far)
  and the half-size picture's half floats, which hold one minus a transmittance near 1 only in
  steps of 1/2048 (they now hold the transmittance itself). A sun tens of thousands of times the
  white showed through both as a disc behind any deck.
- NO CLOUD ENDS IN A CUT. Round the sun and the moon the glow is seen as the disc is, far over
  the picture's white, so a difference of a thousandth in what a cloud lets through shows. Three
  things cut a cloud's edge into a line a pixel wide there, and each is now continuous:
  - the march's stop: a ray stopped at a hundredth of the light and took the rest of its path
    as thick, while the next ray, past a thin wisp into clear air, marched on and kept its
    hundredth. Within 0.1 rad of the key light the march goes on to a millionth (back to the
    hundredth by 0.3 rad, so only the rays round the disc pay for it);
  - the layers behind an opaque one were dropped once a hundredth was left; they now fade out
    with what is let through, from a hundredth to a thousandth;
  - a layer under a ten-thousandth of cover was skipped, its glow with it; only one under a
    ten-millionth is now.
  And a dense layer's edge is FEATHERED (`feather`): its density's share, climbing from nothing
  to full across the edge, is taken to the power ln τ / ln(1/0.3) (τ the layer's whole depth),
  so the edge reaches an optical depth of one 0.3 of the way in rather than a thirtieth of the
  way in on a scud of 30. A layer under 3.3 deep keeps its edge as it was.
- THE GLOW'S ORDERS: the glow is lit by the light the ray's steps scatter times the ray's own
  transmittance (τ e^−τ: brightest through a patch about one deep, gone behind a thick core).
  Through a thicker cloud the light is bent again and again: the orders past the first (Poisson
  in their count, each the diffracted half of an extinction) spread the glow ever wider, a
  Gaussian of the first's core (0.257 wavelengths over the radius) widened as the root of the
  order, so the disc melts into a bright smudge where the sun stands and, thicker still, into
  the deck's grey: frosted glass, then a bright patch, then nothing.
- THE SHAFTS (`shafts` in `wgsl/frame.wgsl`): six points along the view ray, out to 20 km,
  read the cloud's shadow; the air scatters that share of the sun (70 % of its in-scatter is
  shadowed at most), on the world through the haze and on the sky strongest toward the sun, so
  the gaps between heaps throw beams.
- THE HEIGHT AND THE WIND (M8f9, `mountain/app/cloud_field.hpp`): every layer stands at its
  true height over the valley floor. The march walks the eye's rays through each slab, so a
  cirrus 8 km up needs no world under it. Each layer drifts on the wind aloft at its own
  height: the map's wind grown on a log law and turned with height. A low layer crosses the
  sky the faster for being near. THE EVENING'S LOW-LEVEL JET: as dusk falls (`dusk_of`: 0 with
  the sun 6 degrees up, 1 with it 6 down) the air a few hundred metres up decouples from the
  cooling ground, and its wind speeds up to 1.7 times the day's at 400 m (`jet_at`, a Gaussian
  450 m wide). From twilight a fair, flurrying, grey or snowing sky in a wind of 2 m/s or more
  also gets ragged SCUD (`fractus`) racing under it at 200 to 450 m. It is dealt on a salt of
  its own, so it moves none of the stack's layers, and more of it comes the further dusk has
  fallen. Being so near and in the jet, it crosses the sky five times faster than the heaps
  over it and blows across a lit moon. `Aloft` carries the picked height and speed (`base`, the
  lowest layer's base, every layer over it in proportion; `speed`, a share of the true wind in
  place of the true wind's whole; the layers drifted at a quarter of it before M8f9, and a low
  cloud hung still where a real one races), and Fall Line's `sv_cloudheight` and `sv_cloudspeed` set them.
  The drift is the wind times the clock, not integrated, so with the sky's clock running
  (`sv_skyrate`) a jet winding up moves the layers on a little as it does.
- CLOUDS SKIED THROUGH (M8f13). A layer is a slab of the world's air at its height over the
  valley floor, so a deck lower than the mountain lies across its slopes: the skier skis down
  onto its top, through it and out of its base, and the march, walking each ray from the eye
  through the slab to the scene's depth, puts the cloud round him and between him and the
  ground. `Aloft::thickness` (Fall Line's `sv_cloudthickness`, m) is how tall the lowest layer
  stands from its base to its top. Its cloud keeps its kind's density a metre, so a taller layer
  is the darker under it and hides more of the sun (its shadow's optical depth grows with it).
  Every layer, dealt or picked, starts at the top of the one under it, so a layer's top is
  where clear sky or the next layer begins; two layers never share a slab of the air. The
  `world` lab stamps every shot with the eye's height and each layer's base and top over the
  floor, so a shot says whether it was taken over, in or under a layer.

Fall Line picks a stack with `sv_clouds` (kinds joined by '+', such as `cumulus+cirrus`, up to
four; `map`, the default, keeps the map's own), each kind's bands dealt off the map's seed by
`cloud_layers_of`; a stack whose lowest kind is a thick, full, low sheet (`lid_under`; a middle
altostratus, its thin patches the sun shows through, is none) turns the day grey under it as a
lid would. `sv_clouddensity` (0 thin .. 1 dense, -1 the map's) leans every layer between its kind's
thin patches and its cores. The `world` lab's `clouds` view stands at the finish in the
valley and looks up the sky with the sun behind and to one side, the way a photograph of a sky
is taken.

**The night in the picture (M8f2).** A frame's `Night` (`render/view.hpp`, filled by
`render/night.hpp`'s `set_night` once the game has placed the sun) carries the moon and the dark
sky; `plugins/mountain/app/night_look.hpp` places the map's moon on its date (a day of the year
read in 2026, longitude 0) by `core/moon.hpp` (M8f6):

- ONE UNIT OF LIGHT is the sun's illuminance outside the air (128 000 lx), and a night is a
  millionth of a day, under a half-float's smallest step. The frame stores its light through a
  PRE-EXPOSURE (`pre_exposure`: the estimated light on a white face brought to about a quarter
  of a unit, in `world.y`), 1 whenever the sun is up so a day's bits never move; the exposure
  divides it back out, and the eye's adaptation and a star's light read through it;
- THE MOON'S PLACE (M8f6, `core/moon.hpp`) is a truncated lunar theory (sixty terms in
  longitude and distance, sixty in latitude), the time's step from the universal hour, the
  nutation, the apparent sidereal time, the parallax of a place on the surface and the
  refraction: within 0.007 degrees of a reference ephemeris from 1990 to 2049. It gives the
  phase, the libration and the axis's tilt (the IAU's pole and prime meridian precessed to the
  date) and the Earth's shadow (its axis and the umbra's and penumbra's radii at the moon);
- the moon's light follows the phase law (a half moon a tenth of a full one) and the square of
  its distance; its disc is drawn at its true size by Lommel-Seeliger's law, scaled to that
  light (`surface_surge`), over THE REAL FACE (`render/moon.hpp`: a public-domain orbiter
  mosaic's albedo, 1024 x 512 with its mips, read at the level the disc's size asks), turned by
  the face's pole and prime meridian, with an earthlit dark part; a faint part beside a bright
  one is drawn as the eye sees it (the earthshine's ratio to the lit limb to the power 0.35,
  never above the earthshine alone), so a day moon is a pale disc in the blue and a twilight
  crescent shows its ashen light;
- THE DISCS DRAWN LARGER (`FrameView::sun_size`, `moon_size`; Fall Line's `r_sunsize` and
  `r_moonsize`): each disc's angular radius times its factor, as through a longer lens. Only the
  disc grows: the sun shares the same light over its larger face and the moon keeps its face's
  brightness, and neither lights the world more. A cloud's glow round the key light is a point's
  pattern spread over the disc, so it is read off the drawn limb (`clouds.wgsl`'s
  `disc_swell`): its core fills a larger face and its corona's rings stand out from the larger
  edge. The Earth's shadow on an enlarged moon is still laid at the real disc's scale;
- AN ECLIPSE: the Earth's shadow on the disc, the penumbra by the share of the sun's disc it
  leaves and the umbra the air's refracted red (a thousandth of the sun's light at its edge, a
  ten-thousandth on its axis), its contrast against the disc's brightest point to the power
  0.5; the moonlight on the snow is dimmed by the disc's mean (`eclipse_of`, on the CPU);
- OTHER WORLDS' MOONS (M8f14; `render/moons.hpp`, `FrameView::moons`, `render/night.hpp`'s
  `set_moons`, `render/wgsl/moons.wgsl`; Fall Line's `sv_moons`): ten kinds taken from real moons
  of other planets, each as large as it looks from its own planet (Io a little larger than the
  Moon, Phobos a third of it, Deimos a point drawn a pixel and a half wide, its light shared
  over it). Up to four go round the sky near the celestial equator, each at its own period
  (Phobos faster than the sky turns, so it rises in the west; Triton backwards). Each is locked
  face-on and rocks by its libration, and is lit by the real sun, so two moons show two phases.
  Each is drawn as an ellipsoid hit in closed form, or, for a dented kind, marched inside that
  bound. Its face is the real moon's map (`render/moon_maps.hpp`: the public-domain spacecraft
  mosaics, 1024 x 512, Io, Ganymede and Triton in colour; `render/moon_maps/SOURCES.md` names
  each), in its body frame at the mip level its size asks; its light is
  Lommel-Seeliger's blended toward Lambert's, as seen, and Titan has a haze shell. A set without
  the Earth's moon hides that disc and keys the night on the others' summed light from the
  brightest one's direction. Each sky type names its own set (`SkyType::moons`). Post's night
  vision leaves each one's bounding disc to the cones (`on_moon`);
- THE KEY LIGHT, the one direct light that casts shadows, is the sun until three degrees under
  the horizon, then the moon when it is up (`FrameView::sun_*` become the moon's; the real sun
  stays in `night_sun`). The sky is TWO tables at once: the sun's own sky view and a second at
  the moon's height lit by its light (`moon_view_lut`, built only at night when the moon turns
  or the eye moves), summed in `sky_luminance` and the sky's light, so the dusk's glow fades
  under the moon's sky and the handover moves the picture by a thousandth. The aerial volume is
  built for the brighter of the two skies and the other is laid over as a veil; while the sun
  still keys, the moon's direct light is spread from above (`world.z`);
- the dark sky's own glow (2e-4 cd/m2 at the zenith, brighter toward the horizon) lights the
  snow without a moon; the stars are the REAL ones (M8f5, `render/stars.hpp`: a bright-star
  catalogue's 9,096, each at its place, magnitude and B-V colour, baked into the engine as a
  generated table and cut into a cube map of 6 × 160 × 160 cells of up to three stars, the sky
  pass reading the pixel's cell and the eight round it; 19 stars of tight clusters are dropped,
  none brighter than the fifth magnitude), drawn down to the limiting magnitude the sky's
  brightness leaves (none in the day, thousands on a moonless night, few under a full moon),
  turned by the sidereal angle about the pole at the map's latitude, so the constellations stand
  where the map's place and hour put them (the catalogue's epoch's places: the precession since,
  under half a degree, is not applied), with the galaxy's band;
- THE EYE: under 14 lx on white the exposure adapts only partly (dark scenes stay dark), and
  the rods' share desaturates the picture toward a blue tint (`night_vision`, in post before
  the exposure). The share is the scene's or the pixel's own, whichever is less, so a bright
  star keeps its tint; the moon's disc, drawn far dimmer than it is but always bright enough
  for the cones (an eclipse's umbra near 1 cd/m2), keeps its own colour: the sky pass gives the
  sky in front of it to the rods and post leaves the disc's pixels alone (`on_moon`), so a full
  moon is a warm white and an eclipsed one copper red over a blue-grey night.

The `night` lab draws it on seed 38's vista: the snow's luma falls from 194 at three degrees
under to 67 at fourteen, a full moon's snow is 71, a quarter's 28 and a new moon's 10, the
handover moves the snow by 0.1 %, a moonless sky draws 319 star pixels, and the sky tables'
twilight matches `twilight_lux` within 1 % down to twelve degrees under.

## Fog in an area

The sky's haze is the air's, the same over the whole map; the weather's fog closes everything at
one visual range. **Fog in an area** is the third kind: a mist lying in a wood, the dust of a
hall, the glowing haze over a pool of lava, placed by the game where it is (`render/fog.hpp`,
M38a). The froxels reach a few hundred metres; the sea of fog in the valleys seen from a peak is
the next section's.

- **The volumes** (`FogVolume`): a box or an ellipsoid turned about the vertical; its extinction
  at its floor, thinning upward by a falloff; soft over an edge inside its faces; carved into
  wisps by two octaves of the cloud's tiled noise drifting on the wind; scattering an albedo's
  share of what it takes out on a Henyey-Greenstein lobe; glowing (cd/m2 a metre of extinction).
  One that **hugs the ground** measures its height up from the terrain under each point, so a
  mist lies as deep on a slope as on a flat. `prepare_fog` keeps the 32 nearest in reach.
- **The froxels**: 160 x 90 columns over the screen, 64 slices deep to the range (160 m by
  default), a slice's far edge at range x (slice / slices)^2: centimetres deep at the eye,
  metres at the far end. A bank kilometres off takes a range of kilometres (`FogSettings`).
- **`fog.inject`** (compute, a thread a froxel): the medium at a point jittered in depth inside
  the froxel (a new depth each frame on the golden ratio), lit by the key light (`sun_light`:
  the air's transmittance, the cloud's shadow) past the casters' maps (the shafts through a wood)
  and the mountains (`terrain_sun`: 16 steps toward the sun over the ground's heights, growing
  from 6 m to about 11 km, soft by the distance; near the snow the horizon maps' answer too: the
  horizon a point on the ground sees says little of what one 300 m over it sees, so a bank in a
  shaded valley lies in the shade with its floor), dimmed by the fog's own medium toward the light (a 63 m march: the floor of a bank in
  its top's shade). The orders of scattering past the first come out evenly, held to what a deep
  bank's lit top sends back (0.8 of a white face's light, so a sunlit bank is as bright as the
  snow beside it), counted only as far as the fog round the point is thick (a dust scatters once)
  and falling as one over the reduced depth into it from above. Then the sky's light through the
  fog over the point, and THE GROUND'S LIGHT from below: the sun and the sky sent back by the
  terrain's albedo under the point (the frame's even ground with no terrain), through the fog
  over and under the point. Over snow that is nearly the light from above again, and without it
  a mist over a sunlit field came out at under half the snow's brightness. Last, its glow. The
  result is blended with the
  last frame's light at the same world point (reprojected through the last camera), nine
  tenths of it, so the jitter averages to a smooth medium.
- **`fog.integrate`** (compute, a thread a column): front to back, each slice's light over its
  depth dimmed by the slice itself (energy-conserving), the transmittance multiplied on, both
  written at the slice's far edge.
- **The read**: `frame.wgsl`'s `local_fog`, called last in `haze` (after the air, the weather's
  veil and the reach's mist, since this air is the nearest), by the sky pass and, over a
  premultiplied colour, by the cloud's composite (`local_fog_over`). It is group 0's binding 11
  and `frame.fog` (the range, the slices). With no volume in reach the range is 0, nothing reads
  the volume, and the picture is the bits it was without fog (`render_fog_test`).

On the software adapter at the default 160 x 90 x 64 froxels the inject costs 40 to 210 ms (the
march toward the light in every froxel the medium fills) and the integrate 15 to 25 ms (the `fog` lab's `gpu-ms` rows); a real GPU's figures and a ladder row
(the froxels' size) wait for M38f. Not yet: the lamps in the fog (M38e, on M39's light list).

## The valley fog

A still morning's inversion fills the valleys with fog up to a level top: from a peak a white sea
with the ridges standing out of it as islands, from under its top a grey gloom. It lies over
kilometres and its edge on a slope must stay sharp at any range, which no froxel volume of a few
hundred metres can give, so it is a layer of its own, drawn per pixel (`render/valley_fog.hpp`,
`FrameView::valley`, M38h).

- **The medium**: an even extinction under the top, thinning linearly to none over an edge above
  it. The integral of that share from a height up, G(y), is 0 over the edge, a square across it
  and a line under it, so a ray's optical depth between the eye and a surface is the extinction
  times |G(0) - G(end)| over the ray's slope: closed form, no march, no texture
  (`valley_depth`, held against a numeric march by `render_valley_fog_test`).
- **The top** rises and falls on two octaves of the cloud's tiled noise, read where the ray meets
  it, drifting on the fog's own wind; the eye's offset in the noise is folded into its tile in
  doubles on the CPU (`valley_rows`), so it stays exact at any place and hour.
- **The light**: a white face's under this sun and sky, the fog's albedo of it (0.85): the sun on
  the top's billows (the first octave's slope over two texels, with a wrapped cosine, since the
  light a fog sends back is scattered many times inside it; laid level from three tiles off to
  ten, where the waves are under a pixel) past the cloud's shadow, and the sky over them; dimmed
  under the top by the eye's depth in it, to 0.3 of the top's at the deepest.
- **The read**: `valley_fog` (wgsl/valley_fog.wgsl) first in `haze`, before the air's haze, so a
  far sea is veiled by the air in front of it; by the sky pass, so the sea reaches the horizon
  where no ground is drawn; and over the cloud's premultiplied colour (`valley_fog_over`). A
  ray that meets no fog returns its colour as it came, and with no fog (`density` 0) nothing
  reads it: the bits are the ones drawn without it.

The `valley-fog` lab draws a seed's sea from the summit, from the air, at dawn and from under it.
Not yet: the mountains' shade on the top (a sea at dawn is lit to its far shore under every
ridge's shadow) and Fall Line's own seas, dealt by the map's weather and hour (M38f).

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

- exposure from the sky's brightness, not from a histogram, so it never pumps outdoors; the
  eye's adaptation (M43) opens it where the world is darker than the sky lights it;
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

**As built (M35a, M35b): the lens and the film** (`render/post.hpp`, `render/lens.hpp`,
`render/grade.hpp`; the `post` lab). The effects a modern game ships, each a switch and a
strength, and each chosen to cost nothing when it is off and almost nothing when it is on:

- **Folded into the post pass, no pass of their own:** contrast-adaptive sharpening (the four
  neighbours' headroom scales the negative lobe, so a flat area sharpens most and an edge already
  at full contrast barely: it undoes TAA-U's softness without halos), the lens's lateral colour
  (two extra bilinear taps), the vignette and the film grain (arithmetic), and the lens dirt (a
  small procedural picture made once at load, lit only by the glow past white and the lens pass,
  so it shows round the sun and never over the snow). Each is an `override` switch, so a game
  that leaves one at 0 compiles it out; the strengths are a 32-byte uniform (`set_post_look`).
- **Baked into the grade's table, free:** contrast (an S-curve about middle grey, the ends held),
  saturation and a white balance (temperature, tint) in linear light keeping the luminance, and
  gamma (`apply_grade_knobs` after the game's own grade).
- **One quarter-size compute pass, `lens`, after the bloom:** the sun streaming through what
  stands in front of it (a 32-step radial march toward the sun's place over the open sky round it,
  read off the depth target, started at the first step inside the sun's radius so a far texel
  costs nothing) and the sun's flare (six analytic ghosts and a ring on the line through the
  middle, scaled by how much of the sun is open sky). It reads only the post pass's input and the
  depth, so it needs no bloom; the post pass adds it in one tap. The sky's own shafts (the cloud's
  shadow across the air, § Sky) stay the sky pass's; these are the screen's half, from trunks,
  crowns, ridges and figures, and the two stack.

**As built (M35c): motion blur** (`render/motion_blur.hpp`, the gather in `post.wgsl`; the
`post` lab's `moving` and `motionblur` variants). The picture is smeared along the motion target
the opaque passes already write (the camera's and each thing's own motion), for the shutter's
share of a frame (`PostLook::motion_blur`; 0.5 is a film camera's 180-degree shutter), each streak
at most 4 % of the picture's height:

- **Two small dispatches before post, `motion_blur.tiles`:** the fastest motion of each 16 x 16
  tile (and the slowest's length), one lane a tile reading its texels in turn, then the fastest
  and slowest over each tile's 3 x 3 neighbours, so a fast thing's blur reaches into the still
  tiles round it. A first cut reduced each tile in shared memory, a workgroup a tile; its eight
  barriers cost 30.7 ms at 1280 x 720 on the software rasterizer, and one lane a tile 3.7 ms.
- **The gather, folded into post:** a pixel whose neighbourhood moves under half a pixel costs one
  load. Where the slowest motion round it is at least half the fastest (the camera turning over
  open snow: nothing to keep sharp), the fast path averages 3 to 6 bilinear taps along the streak
  (one every four pixels) and reads nothing else. Elsewhere each tap also reads its motion and
  depth and is weighted by whether its own streak reaches the pixel and which of the two stands in
  front (a scatter-as-gather reconstruction), so a figure the camera follows stays sharp against
  the streaming slope and the slope does not smear over it. The taps' offsets are a fixed
  screen-space noise, so their gaps break up instead of banding.
- **Cost** at 1280 x 720 on the software rasterizer, the eye moving at 21 m/s and turning: the
  tiles 3.7 ms and the post pass 16.1 → 27.7 ms; the first cut with eight taps and no fast path
  was +26 ms on the post pass at 960 x 540. It is the dearest of the lens and film, so Fall Line
  keeps it off (`r_motionblur 0`) and offers 0.5.

**As built (M35e): ambient occlusion** (`render/ao.hpp`, `sky_seen` in `wgsl/frame.wgsl`; the
`post` lab's `ao` variant and the `world` lab's `--cfg="r_ao …"`). How much of the sky each
surface sees, worked out from the depth alone and taken off the sky's light only (the sun keeps its
shadow maps), so the snow darkens at a trunk's foot, under the crowns' skirts and round a skier's
boots, and a figure stands on the snow instead of floating over it:

- **One half-size compute pass, `ao`, after the depth prepass:** each texel stands for one of the
  four render pixels under it (a different one each frame), rebuilds its point and its normal from
  the depth, and searches 2 slices through the view ray, 4 steps out each way, within a radius in
  metres (`r_ao_radius`, 1.5): the highest thing on each side is that side's horizon and the
  cosine-weighted arc between them is the sky the surface sees (a ground-truth horizon search). The
  slices turn and the steps shift by a noise of the texel and the frame that any 3 x 3 texels
  hold evenly, and the pass's second dispatch takes each texel's mean over the 3 x 3 round it whose
  depth is like its own. Without it TAA-U alone left the shade under a moving skier speckled: the
  history it would average is the one it rejects where a figure moves. A texel whose radius spans under two pixels (the
  far ground) stops after its first load; the few hundredths of occlusion open ground reads off
  the depth's own steps are dropped, so a flat slope is as lit as with the pass off.
- **Read in the lit passes** (`sky_seen`, the frame group's binding 10): the four half-size texels
  round the pixel, each weighted by how near it is and how like the pixel's own its depth is, so
  one surface's shade does not bleed onto the one behind it. With the pass off the frame group
  binds the sky's blank and `sky_seen` is one dimension query.
- **Cost** at 1280 x 720 on the software rasterizer: the pass 22.6 ms (the search about 14.5, the
  smoothing 8) against the post pass's 16, about 4 % of the frame; one slice instead of two saved
  only a tenth of the search, so two it is. Fall
  Line keeps it on at 0.6 (`r_ao`): the snow's bounce fills the creases in life, so the full
  strength reads too dark.

**As built (M35d): depth of field** (`render/dof.hpp`, `defocused` in `post.wgsl`; the `post`
lab's `dof` variant and the `world` lab's `--cfg="r_dof …"`). The picture blurred away from a
focus the game names, for a replay's lens or a photograph rather than for play:

- **The blur circle** (`wgsl/dof_coc.wgsl`, one copy for both passes) grows with
  |1 - focus / distance|, as a lens's does: the far field tends to the widest circle
  (`DofSettings::strength` x 1.5 % of the picture's height), the near field reaches it a little in
  front of the focus, and `sharp` (0.15) of the way either side of the focus stays sharp.
- **One half-size compute pass, `dof`, after the lens pass:** 16 taps of the post pass's input
  over a disc as wide as the widest circle, turned per texel, each counted as far as its own
  circle reaches the texel; a tap behind the texel counts no further than the texel's own circle,
  so the blurred background never takes a sharp subject's edge. A texel in focus skips the gather.
- **The mix, folded into post:** each pixel reads its own circle off the full-size depth and
  takes the gather in by it (fully past a pixel and a half), so the subject's edge is as sharp as
  its pixels. The near field's spread over the focus, which a scatter would draw, is left out: a
  blurred near thing keeps a hard edge against a sharp one behind it.
- **The focus** is live (`set_dof`, `set_post_focus`): Fall Line's `r_dof_focus`, or with 0 how far
  the camera's subject is (`camera/subject.hpp`'s `framed_point`), every frame.
- **Cost** at 1280 x 720 on the software rasterizer, into the sun focused 30 m out at full
  strength: the pass 26 ms and the post pass 15.5 → 18.6 ms. Fall Line keeps it off
  (`r_dof 0`): in play the whole picture is the road ahead.

Considered and not taken: a screen-space "god rays" occlusion pre-pass (the depth target already
says what is sky), image-based ghosts off the bloom chain (its Karis-weighted first level flattens
the sun on purpose, so the ghosts came out dim and blotchy; the sun's own analytic flare is sharper
and cheaper), a starburst (reads as a filter, not a lens) and anamorphic streaks (a film look the
game does not want). Fall Line keeps the sharpening (0.3), the vignette (0.25) and the rays (0.5)
on and the flare, fringes, grain and dirt off: those read as an effect rather than as the picture.

**As built (M43)** (`render/eye.hpp`). **The eye's adaptation** leaves the sky's exposure as
the eye AT REST, so the snow under the sky still never pumps, and opens the eye past it only
where the picture is darker than the sky lights it: a hall, a cave, the woods at dusk. A compute
pass (`eye`, after bloom) meters the post pass's input on a 128 × 72 grid of bilinear taps at
the sky's exposure into a 64-bin log2 histogram (a quarter stop a bin, the middle of the picture
weighed four times the corners), then one workgroup averages the bins between 40 % and 90 % of
the weight, takes the stops that average sits under the key (0.18) beyond a band of one stop,
scales them by the strength (0.85) and clamps them to 0..4 stops, and steps the eye toward
them with a half-life of 2.5 s opening and 0.35 s closing (the eye closes to light fast and
opens to the dark slowly). The eye is kept on the GPU in the fifth row of the sky's light buffer
(`SkyLight.eye`), which every pass already binds, so the post pass, TAA-U's weights, the
clouds' white and the moon as seen read the same exposure with no new binding and no readback.
The opened stops dim the white the eye adapts to (`exposure_at`), so the night's partial
adaptation under 14 lx still holds in the dark. A zeroed buffer is the eye at rest: a frame that
never runs the pass keeps its bits. The `gpu-eye` lab walks out of a 40 m stone corridor into a
noon and back into the dark.

Linear light is used throughout, and the conversion happens here, once.

## The 2D pass

Menus, the HUD and the console are drawn by the 2D layer (`engine/ui`, `docs/architecture.md`):
an immediate-mode draw list of rounded boxes and text runs, which a CPU raster pictures headless.
The GPU draws the same list in one pass after post, blended over the display image.

**As built (M22b)** (`render/ui_pass.hpp`, `ui_pass.wgsl`):

- **One instanced draw.** Each box is one quad, and its rounded rectangle's signed distance,
  paint (solid, or a linear or radial gradient of up to eight stops), image and border are
  worked out in the fragment shader. Each glyph is one quad that reads a glyph atlas. Every
  instance is in one storage buffer and every paint in another. The draw splits only where a
  box's image changes, because each image has its own small bind group. A menu page is one draw.
- **The glyph atlas** is a 1024² `R8Unorm` texture packed in shelves. A glyph's mask is copied
  in the first time a list draws it, and kept under the font's cached mask. A full atlas is
  emptied and refilled from the list being prepared.
- **Images** are uploaded the first time a box names them. A texture the GPU already has, such
  as the live frame under a menu, is bound to a key with `ui_texture`.
- **The CPU raster's pixels.** The pass uses the same signed distance and coverage at the
  pixel's centre, the same clip rule, and text set by the same `ui::place_glyphs` from the same
  masks. It samples images bilinearly at the same texel centres. It blends straight
  sRGB-encoded colours, folding a fill and its border into one premultiplied colour, so the
  output must be a UNORM target: an `*Srgb` one would blend in linear light. Gradients get half
  a level of dither against banding.
- **Measured.** The `menu-gpu` lab holds every page of the `menu` lab to the raster: on lavapipe,
  none is more than 3 levels off at any pixel. Preparing a 1280 × 720 page costs about 1.7 ms of
  CPU, and the software adapter draws it in 19 ms. The real GPU's cost waits for the Mac.
- **In Fall Line's frame (M21d)** it is the pass table's last row, `ui` (reads and writes
  `final`), drawn when the frame's scene hands it a list (`FrameScene::overlay`): the window
  hands it the console's every frame. With no list, the row runs empty and keeps its timing row.

**Shapes (M22e).** A HUD's instruments need more than boxes, so the draw list has five kinds of
command, every one a quad in the same instanced draw with its own signed distance:

- **Boxes turn and nine-slice.** A box turns by `rotation` (radians, clockwise) about `pivot`
  (box-local pixels, the centre by default): the quad is the turned box's bounds, and the
  fragment shader moves the pixel back into the box's own frame before the rounded rectangle,
  the paint and the image are worked out, so a needle is a box turned about its foot and a map is
  an image turned under a heading. A nine-sliced box maps each band of the box (`slice_to`) to its
  band of the image (`slice`): the corners keep their pixels at any size, the edges and the
  middle stretch.
- **Lines** are capsules of a width between two points; a polyline is a line a segment.
- **Arcs** are bands of a width round a centre, from one angle to another (0 at twelve o'clock,
  clockwise), with flat or round ends: the distance is to the ring inside the sweep and to the
  ends outside it. A gradient over an arc is in the arc's box, so a dial's band grades from one
  end to the other.
- **Convex polygons** of up to 16 points, in either winding: the distance is the largest of the
  edges' distances, the edges' outward normals worked out once on the CPU and read from a third
  storage buffer.

Every angle's sine and cosine is worked out once a command on the CPU with the engine's libm,
so the raster and the GPU read the same numbers. On lavapipe the `menu-gpu` lab's instrument
panel is within 8 levels of the raster everywhere but one pixel a picture, at the needle's
point, where an acute vertex's coverage swings on the last bit of a float.

## Particles and figures

- **Particles are simulated in compute** in fixed pools: emission appends to a ring, the
  simulation integrates and kills, and a compaction step writes the indirect draw count.
  They are drawn as camera-facing quads, softened against the depth buffer. Sorting is
  avoided by blend choice where possible; where a pool needs it, a bitonic sort in
  compute orders it.

**As built (M8d1).** `engine/render/particles.hpp` is one pool of 16384 slots of 64 bytes, a
ring: the game hands each frame's births (`emit_particles`) and `prepare_particles` writes
them into the next slots, so a full pool lets its oldest go first. There is no compaction or
indirect count yet: `particles.sim` (one thread a slot) skips a dead slot, and `particles`
draws one quad a slot with a dead one folded away in the vertex stage. The flight is gravity
times the particle's `fall` (1 a grain, about a tenth a fine that drifts), an exponential drag
toward the still air, and the ground off the terrain's heights: a grain coming down on the
snow is gone, a fine one settles and fades, and one under the snow and rising flies on (a ski
sinks into soft snow, and what it throws starts there). `step_particle` is the flight's CPU
twin, and `tests/render_particles_test.cpp` holds the pass to it. A particle is drawn as a
streak along its motion over `streak` s (a camera's shutter sees spray as strokes), thinned by
its length, faded in and out over its life, softened over `soft_m` against the depth, lit as
the instances are (the sun past the horizon and the shadow maps, the sky) with a forward lobe
toward the sun, hazed, blended premultiplied and unsorted, and it writes the motion target's
reactive channel so TAA does not smear it. What fills the pool is the game's: Fall Line
throws each racer's spray (`plugins/skier/app/spray.hpp`) over the time between two frames'
`time_s`. On the software adapter the pass pair costs about 0.6 + 0.13 ms (the `profile`
lab), the flight of all 16384 slots the larger half.

**As built (M8d2).** The drag pulls a particle toward the AIR, not still air: the frame hands the
pool the map's wind at the view's time (`plugins/mountain/air/air.hpp`), so a powder cloud (the
spray's big thin puffs, hanging three to five seconds) drifts off downwind. THE FALLING SNOW is
`engine/render/precip.hpp` (named `snowfall` until M36 brought the rain into it): 24576 flakes
in a 36 m cube centred on the eye, no births and no deaths. `precip.sim` moves each at the wind
plus its own fall (0.6 to 1.4 m/s by its index) and a flutter on its phase, and wraps its offset
from the eye back into the cube, so the eye always stands in a full fall however fast it moves.
`precip` draws the first `fall` share of
them (the air's fall at the view's time: a flurry a scatter, a storm the whole box) as streaks
over a 1/60 s shutter, at least a pixel and a half wide (a far flake widened and thinned as much,
so it keeps its light and does not shimmer), thinned by the root of the streak's length, faded
toward the cube's faces and at the lens, lit by the sun past the horizon and the sky, hazed and
softened against the depth. When nothing falls the pass is an empty compute pass under its name
(an empty render pass still loads and stores its targets). The flakes are lit as the day is: under
a clear sky that is a sunny fall; under a lid they are lit as its flat grey (M8f1, above).

**Precipitation (M36).** What falls, and how hard, is a world's to say through
`engine/wind/precip.hpp` (no GPU, golden): ONE DIAL, a fall's intensity 0..1 and its rain share
0 (dry snow) .. 1 (rain), gives the liquid-equivalent rate (rain 0.64 * 232.6^i mm/h, so 2.5 at
0.25 and 50 at 0.8; snow 0.04 * 250^i, 10 mm/h of water at the top), the TIER (rain by its rate:
light, moderate, heavy; snow by what it hides: 1 km, 500 m; either a storm when heavy in a wind
of 15 m/s or more, and rain past 50 mm/h), the visual range (rain 11.5 R^-0.63 km, snow
1.2 R^-0.9 km), the pace (a drop's terminal speed by its median size off the exponential size
spectrum, a flake's 1 m/s, sleet between), the PHASE by height (snow above a freezing level,
melting through the 300 m under it) and the WET GROUND (a 0.3 mm film and puddles to 3 mm that
drain and dry by the wind and the sun). The picture reads the same numbers:

- `engine/render/precip.hpp` turns the first `rain` share of the particles (by a rank of their
  index) into DROPS, so a sleet is flakes and drops at once and a rising share turns them one by
  one. A drop falls at 0.75 to 1.25 of the rain's pace and is drawn as a thin clear line over
  1/30 s (the eye smears a drop), at least a pixel and a half wide and never thinned below a
  faint trace, taking the sky's light with the sun caught along it. Every particle rides THE
  EDDIES, a slow field of swirls some metres across at `swirl` m/s (the wind's turbulence
  intensity times its speed, over a floor), so a storm's snow swirls in sheets; a drop shrugs off
  most of it.
- THE SPLASHES are the same draw's next instances: up to 6144 crowns at a full rain, each living
  0.16 s at a spot drawn afresh each life out of the screen and set on the surface the depth
  shows (its normal off the depth's neighbours), kept where that faces up and within 24 m, at an
  even density over the ground (a far spot kept by the share of ground its pixel covers). Off the
  depth, so a splash lands on any game's ground, roof or deck; a world with roofs binds THE
  SHELTER (below) to keep them off the floors under them.
- THE WET GROUND rides in the frame's `wet` row (`wet.xy`) (`Weather::wet`, `puddles`): every world material
  calls `wet_albedo` (a porous surface goes to half its albedo soaked, the water keeping the light
  in its pores) and adds `wet_sheen` (Schlick's Fresnel off water, the sky along the reflected ray
  and the sun's glint; on the flats the puddles lie level and mirror sharply). On Fall Line's snow
  that reads as slush: the glitter goes, the glare dulls, the flats pool.
- Fall Line keeps one wet ground a frame, the eye's, stepped by the frames' clock from the steady
  state its rain and drying would leave. Its rain comes from the map's SNOW LINE
  (`Weather::snowline`, raised only by `sv_snowline`: R19 deals none, so every dealt map is snow
  to the valley and no digest moves). The cvars: `r_precip`, `r_precip_density`, `r_splashes`,
  `r_wet`; the labs `precip` (eight skies from a flurry to a rainstorm) and `precip-air` (the
  numbers as charts).
- THE SNOW'S COVER and THE LIGHTNING are the dial's too: `step_cover` lays ten millimetres of
  snow a millimetre of snow's water and takes it with the rain (4 mm an hour a mm of rain) and
  the sun (2 mm an hour in full sun), the cover's share 1 - e^(-depth / 6 mm); `flash_at(t)`
  lights a heavy fall 1 stroke a minute and a storm up to 6 (fewer the more it is snow), each a
  main stroke and two returns, a pure function of the clock (half-second slots hashed, so a
  replay flashes at the same moments). The frame carries both in `wet.zw`: the snow on what faces
  up, the flash over the open sky and the dome.
- THE SHELTER (`PrecipShelter`, `bind_precip_shelter`): a depth map looking ALONG THE FALL (down
  and downwind, kept within 75 degrees of the vertical), the surface nearest the sky in each
  texel. A particle deeper than it is faded out and a splash whose spot is deeper is not set, so
  the rain falls in a courtyard and through a window as far as the wind slants it, and not in the
  room behind the wall. Without one everything is open. The edge is a soft shadow's
  (`PrecipShelter::spread`, the tangent of the fall's scatter): a blocker search finds how far
  below the opening a point lies, the penumbra widens with that depth, and eight comparison taps
  rotated by a hash of the position break it into spots, so a fall scatters out from where the
  slant puts it instead of landing as the window's square. A particle is kept by its open share
  against a hash of its own, so the edge thins rather than fades.

**The weather on a BSP map (M36e).** `plugins/brush/app/bsp_weather.hpp` turns twelve cvars
(`sv_weather`, `sv_precip`, `sv_rain`, `sv_wind`, `sv_winddir`, `sv_gust` the server's;
`r_precip`, `r_precip_density`, `r_splashes`, `r_wet`, `r_snowcover`, `r_lightning` the player's)
into a frame's weather: the wind off `engine/wind`'s field with its gusts and veer, the fall's
way, the wet ground and the snow's cover for the whole map stepped by the clock, the flash, and
a storm's deck over the sky. The v2 renderer draws a third orthographic map of its `orthos`
array along the fall (the sky faces do not block it), binds it as the fall's shelter, and reads
it in every face's material (`weathered` in `bsp.wgsl`): what faces the fall and the map reaches
is wet, a flat's low noise pools into puddles that the drops ring while it rains, what faces up
gathers the snow in drifts (a thin cover in the hollows first, the floor showing between), and a
flash lights what is open overhead and a little round the rooms. The open sky and the windows
decide all of it, through a soft edge whose spread is the eddies over the fall's pace (snow,
slow, scatters wider than rain), with the wet and the snow struck in noise spots across it: lab `gpu-bsp-weather` (the weather fixture, a hall with windows onto a yard and
a store behind it) shows eight weathers in four views and checks, with no GPU, that a ray back up
the fall leaves by the sky from the yard and the window's patch and is stopped from the store.

Every shader's whole-line comments are left out of what is embedded (`cmake/WgslEmbed.cmake`), so
a material that includes `frame.wgsl` stays under the 60 KB literal cap however well it is
explained.
- **Skinning** uses bone palettes in a storage buffer. A figure drawn in more than one
  view (the player in the main view, the sun's cascade and the shadow atlas) is skinned
  once by a compute pass into a vertex buffer that every view reads. Crowd members,
  drawn at lower detail and in larger numbers, are skinned in the vertex shader from
  their instance's palette.

**As built (M7d1, then M8e2).** Fall Line draws every racer as the skier model
(`plugins/skier/assets/skier/`), one skinned mesh a racer in one instance family
(`games/fall_line/app/scene.hpp`), posed every frame from his `SkierState` by
`plugins/skier/app/rig_pose.hpp` and placed at his figure frame (`skier/app/figure.hpp`'s
`figure_frame`), a jacket colour a racer; a cut writes the instances fresh and settles every
poser, so no motion is carried across it. The **placeholder figure** (`figure.hpp`'s 17 rigid
pieces, each a unit box, cylinder or ball posed from the `SkierState`) is no longer drawn; it
stays as the `anim` lab's comparison. The free ride's **crowd** (`sim/crowd.hpp`) is the same
model as one palette family (`scene.hpp`'s `crowd_meshes`, `crowd_instance`): an instance an
amateur at his id, each posed by his own poser from the amateur read as a skier (`crowd_skier`:
his place, heading, lean, crouch, down), sized by his body and in one of ten jackets; one on a
lift's carrier or up a lift unseen is drawn at a scale of nought, which the cull drops. The
amateurs nearest the eye cast into the figures' atlas, a tile each after the racers'; each tile
culls the crowd by its own box, so the atlas costs the figures near the tiles, not the crowd
times the tiles (on the software adapter, 2 ms of `shadows.figures` with one racer, 12 ms with
fifteen amateurs; drawing every figure into every tile had cost 385 ms). The crowd draws the
model at three **levels of detail** (`scene.hpp`'s `skier_lods`: the meshes `skier`,
`skier.lod1` and `skier.lod2` the builder makes off the same rig, 1808, 698 and 344 triangles)
by its radius on screen (`kSkierLodPx`: LOD 0 down to 40 px, LOD 1 to 16 px), the heights the
`model-views` lab's `lods` sheet shows each level keeping its silhouette at; the far crowd's
vertex stage, the most of its cost, fell from about 22 ms to 9 ms of `instances` on the software
adapter (crowd-look, three runs each, the medians). An atlas tile's casters pick their LOD at a
quarter of the tile's texels a metre (`ShadowSettings::figure_lod_share`), since a shadow needs
the silhouette, not the folds. The `crowd-look` lab
films a lift's queue, a busy run, a group and one of them down at `--hour` (11: the sun on the
runs), each with the crowd and without.

### Beams

A held electric beam (a weapon's lightning gun, a machine's arc, a spell) is a channel of
glowing segments from its source to whatever it hits, redrawn many times a second, lighting the
room round it and burning where it lands (`engine/render/beam.hpp`, `beams.hpp`, `beams.wgsl`).

- **The channel is struck, not animated.** A real arc's path does not slide; it is ionised
  afresh and the eye sees one bright path, then another. `build_beam` draws a walk from the
  source to the hit in `step_m` steps, each point kicked sideways (`wander`) and pulled back to
  the line (`pull`), pinned at both ends, from a hash of the shot's seed and the strike number
  (`strike_hz` a second). Inside one strike two frames see the same channel; the next strike is
  a new one. A pull of 0.4 keeps a 15 m beam within a few centimetres of straight (a thick,
  nearly straight bar); a pull of 0.2 with a large wander is lightning's stepped
  zigzag. `filaments` strands wind round the axis on a spinning helix (`braid_m`, `twist`,
  `spin_hz`), and forks (`fork_chance`, `fork_m`) branch off, dimmer and thinner to their tips,
  never in the first 1.5 m off the source, where a fork by the eye would fill the view.
- **The light flickers by strike.** `beam_level` swells in over `attack_s`, gives each strike
  its own level (`flicker`), flashes in a strike's first `flash_s` (`flash`: a return stroke's
  peak over its glow) and fades over `release_s` when let go. Inside a strike the shader ripples
  the glow along the beam (`shimmer`).
- **The look is a core and a sheath in physical units.** The core (`core_m`, `core_cd`, cd/m²)
  is a Gaussian at least a pixel and a half wide (its light kept as it widens, so a far beam is
  a fainter line, never a gap) and at most a hundredth of the screen's height (its brightness
  kept, so the root by the eye is a thick line, not a white blot), and is whitened as it
  brightens; the sheath (`glow_m`,
  `glow_cd`) is the ionised air's glow round it. Both are emissive at the frame's pre-exposure,
  added over the lit picture, depth-tested and softened against the depth behind so the beam
  sinks into the wall it hits. A segment's ribbon runs a ramp past each end and the ramps of two
  segments cross-fade, so the glow is even along a bent channel with no bead at a joint.
- **It lights the world through the local lights.** `beam_lights` hands render/lights a row of
  steady point lights along the beam (one each `light_every_m`, the look's `light_cd` shared
  among them, each as soft as their spacing), one at its root by the source (`root_cd`: the
  muzzle's glow on the gun and the hand) and one at the hit, at this frame's level, so the
  walls, the floor and the gun flicker with the strikes. The lamps' light counts in the frame's
  pre-exposure (`night.cpp`'s `pre_exposure` adds `FrameView::lamp_lux`): a dark room exposed for
  its torches alone stores the beam's white past a half float's 65504, the target holds
  infinity and the tone curve turns it black. The pass also clamps what it writes below that.
- **It burns where it lands.** `beam_burns` says when a held beam leaves a scorch: at once on a
  new spot, again each `scorch_every_s` while it dwells, and, when the hit jumps further than
  half a scorch in a frame, at every half scorch along the jump (the caller asks again until it
  says no), so a fast sweep still leaves an unbroken trail. The game presses each through its decal
  system (engine/detail's `place_decal`, a scorch) and hands the pass the clipped meshes; the
  pass lays the soot over the picture and a fresh burn's ember (`ember_level`: `ember_cd`
  cooling over `ember_s`) in it, white-yellow to a dull red, hottest in the heart and the cracks
  of the atlas's mask, in one blend (the ember added, the picture kept by the soot's alpha).
- **Presets.** `arc` (one thick, nearly straight channel in a violet-blue sheath, struck 30
  times a second), `bolt` (a jagged forking channel, struck 14 times a second with a bright
  flash each), `braid` (three thin strands wound round each other, cyan, crackling at 40 a
  second). Every knob is named (`beam_knobs`, `set_beam_knob`) for a console or a lab's `--set`.

The `gpu-beam` lab fires each preset from a held gun down a stone corridor at night and sweeps it
over three walls, with the bloom mixed in as a game's frame mixes it and the eye adapting to the
room's light (quick to a brighter room, slow to a darker one), so the corridor falls back to its
torches when the beam is let go; `--frames=N` writes each sweep's frames
(`frame-<look>-NNN.png`) for an animation.

## Explosions

A blast is common in the games this engine serves (a rocket, a grenade, a barrel), so it is built
to cost little and to read as fire and smoke rather than as a flat sprite.

**As built (M47a).** What a blast IS at each moment of its life is `engine/render/explosion.hpp`,
pure functions of the `Blast` (where and when it went off, its look, its scale, its seed, whether
it went off against a surface) and the clock, so a replay, a lab and two clients see the same
fireball. **One code draws every blast; what tells a rocket from a grenade, or one game's blast from
another's, is its LOOK** (`BlastLook`): 26 knobs, each named, explained and ranged in
`blast_knobs()` and set by name with `set_knob` (clamped). They cover the fireball (its radius, how
fast it swells, how much it grows as smoke, how fast it rises), the fire (how much burns, for how
long, how hot it starts, what it cools toward and how fast, how bright its light), the smoke (how
thick, how fast it builds, how long it lasts, how pale it is and its tint: soot, dust, steam), the
shape (how deep the billows cut, their size, the fray at the edge, how fast it rolls), what it throws
(sparks: how many, how fast, how long, how hot; debris: how many, how many still glowing) and what it
leaves (the scorch's size, the light's reach). The engine's presets are starting points with stable
ids a content table names them by: `explosion` (the common blast, 1.4 m), `explosion.rocket` (hot,
quick and bright, a spray of fast sparks, thin dark smoke), `explosion.grenade` (a short dull flash,
a fat cloud of pale brown dust, many chunks of debris, hanging longer), `explosion.small` and
`explosion.large`; a game copies one, turns its knobs and points its blasts at its own, and the
`gpu-explosion` lab's `--look` and `--set=knob=value,...` show any of them. The shader reads the
shape and the smoke's colour per blast, so many looks in one frame cost nothing more. The fireball swells to its radius in about a fifth of a second, grows on
slowly as smoke, rises as hot air does, and cools from about 2300 K toward a dull red while its
burning share falls and its smoke climbs and thins away over 3 to 5 s. Its colour and brightness are
a blackbody's (`engine/render/blackbody.hpp`: Planck's law against the CIE 1931 colour matching
functions, the absolute luminance a perfect radiator has), so the fire sits in the sun's units: blown
white for a few hundredths of a second, orange, then red, and far brighter against a dusk the eye
has adapted to than against a sunlit yard. The flash a game hands its local lights
(`flash_at`) is the glowing ball's own luminous intensity (its disc times its core's blackbody
luminance times what still burns, about 1.5 million cd at its peak), in the core's colour. What it
throws (`blast_births`, dealt off its seed at the moment it goes off) is white-hot sparks that cool
to red as they fall and dark debris, some still glowing; off a surface they fly into the half space
it faces. The scorch it leaves (`scorch_of`) is its kind's size times its scale, turned and picked
off the seed, for a game to press with `engine/detail`'s scorch decal (none for a look whose
scorch is 0).

`engine/render/explosions.hpp` draws them. Each live blast is ONE QUAD round its ball's outline on
the screen (the whole screen when the eye is inside it), and the pixel's ray is cut by the ball and
by the scene's depth, so a wall or the floor clips it exactly, and marched in 10 steps from a jittered
start that the TAA resolve averages away. Each step reads the cloud's tileable noise twice (the
billows and a finer fray, `render/clouds.hpp`'s texture already bound in group 0, so the pass has no
texture of its own) and carves the ball's edge into rolling billows. Gas that is deep inside and
still burning emits its temperature's blackbody light; the rest is smoke of its look's albedo and tint lit by the sun
on its sunward side, the sky and the fire inside it; emission and absorption are integrated front to
back and the march stops once the ray is opaque. At most 32 are drawn a frame, the nearest, back to
front, premultiplied, each writing the reactive channel so TAA does not smear it. The sparks are the
particle pool's (§ Particles and figures): a particle with a `kelvin` glows as a blackbody cooling
toward 700 K by e every `cool_s`.

**The cost.** The pass runs only on the pixels a ball covers, two fetches a step. On the software
adapter at 640 x 360 (`gpu-explosion`, the pass's own timestamps), a frame of the yard costs about
42 ms; one blast 10 m ahead (3% of the screen) costs the pass 0.5 to 0.7 ms, 32 at once 0.8 ms (most of them small
and far), and one filling the whole screen 0.9 ms, since dense smoke ends the march early (about 2% of the frame at most). A real
GPU's times need the Mac. The CPU stages 32 blasts in about 0.1 ms in the debug build.

**Not yet:** the flash lighting the scene (it needs the local lights, M39a) and the scorch pressed
(it needs the decals, M42b), and particles colliding with anything but a heightfield: rows M47b and
M47c.

## Muzzle fire

A gun's flash in the classic content is a few triangles in the weapon's model, drawn on the frame
it fires. The engine draws it as fire instead, with the explosions' code (§ Explosions), so the
fire is lit by its heat, the smoke is lit by the sun and the cost is the same pass's.

**As built (M47d).** `engine/render/muzzle.hpp` says what a shot's fire is at each moment: pure
functions of the `Shot` (its look, its seed, when it went off), the muzzle's pose (`MuzzlePose`:
the barrel's tip, the way it points, the gun's up) and the clock. **One code for every weapon, and
a LOOK of knobs for each** (`MuzzleLook`, 19 knobs named, explained and ranged in `muzzle_knobs()`,
set by name with `set_muzzle_knob`, clamped). Those knobs set the flame's length and width, life,
heat, the heat it cools to, glow, flash, prongs and their depth, and billows. The smoke is a whole
`BlastLook` of its own, set as `smoke.<knob>`, and `smoke_speed` sets how fast it leaves. The
sparks' knobs set their count, speed, life and heat, and the cone's spread. The light's reach is a
knob too. There are four presets by id: `muzzle.shotgun` (a wide five-petalled star for 60 ms, a
fat grey-white puff, ten sparks), `muzzle.nailgun` (a small 30 ms jet and a wisp),
`muzzle.grenade` (a cooler, dull orange thump and a brown cloud) and `muzzle.rocket` (a long
white-hot jet for 120 ms, fast sparks, white smoke thrown at 6 m/s). A game copies one and turns
it for each of its guns.

**The flame follows the gun.** A game hands the pass each live shot with the muzzle's pose THIS
frame (`MuzzleFire`), so the flame rides the firing animation's kick and the player's turn. The
flame (`flame_at`) is the explosions' ball stretched along the barrel: `stretch` is its length over
its width, and its base sits at the muzzle. It swells to its full length in a third of its life
while it cools and its burning share falls. In the shader the ray is squeezed along the axis, so
the bound is an ellipsoid round the flame and the march spends its steps on fire, not air; the
noise read in squeezed space streaks the billows along the barrel. Prongs are petals round the
axis (`cos(prongs·phi)`), cut deeper away from it, and turned per shot off its seed. The flame is
hotter at its base. `glow` scales its emission below a blackbody's, because a flame's thin gas
radiates far less than a solid radiator at its temperature. The smoke and the sparks stay in the
world. `smoke_of` is a `Blast` born where the muzzle was when the shot went off, carried by its
`v` and slowed by the air (e^(-t/0.3 s), `kBlastDragS`). `throw_shot` throws the sparks into the
particle pool in a cone down the barrel. `muzzle_flash_at` is the flame's light for the local
lights: its glowing area times its core's blackbody luminance times what burns times `flash`.

**The cost.** Flames are staged in the same list as the blasts, so at most 32 are drawn in all. On
the software adapter at 480 x 270 (`gpu-muzzle`), a shotgun's flame at its peak costs the pass about
1.2 to 2.6 ms, a nailgun's stream (seven flames and wisps in view) about 2.6 ms, and a rocket's jet
about 1.2 ms. The CPU stages a stream in about 13 us.

**On a held weapon.** `engine/viewmodel`'s `muzzle_to_world` gives a `MuzzlePose` straight off
the held model: the barrel's tip on the frames the firing animation blends (`muzzle_at`), the way
the shot leaves and the gun's up, carried into the world through the gun's own lens
(`held_to_world`), so the flame drawn in the world's pass sits on the barrel drawn in front of the
eye. The model's own flash triangles are hidden with `DrawLit::hidden = model.flame`.

**Not yet:** the classic pack's weapons each given their look and fired in a lab (M48b), and the
flash as a local light (M47b).

## Impacts

A round that hits something throws a burst off it, and what is thrown says what the surface is.
Concrete and stone give a grey cloud of powder blown out of the hole, with flakes thrown wide; a
steel round may strike a few sparks there. A steel plate gives almost no dust but a bright spray
of sparks carried on along the round's glance. Wood gives a tan puff and pale splinters and never
sparks. Soil kicks a dark plume nearly straight up, and clods fall back out of it. Sand fans out in
pale fines that settle fast. Glazed tile breaks into white shards. Water throws a narrow column of
beads and a little mist. A soft lead pellet strikes far fewer sparks than a hardened spike.

**As built (M47e).** `engine/render/impact.hpp` gives each surface's answer as pure functions of
the hit (`Impact`: the point, the surface's normal, the round's way, the look, the round, a seed,
the clock). **One code for every surface and a LOOK of knobs for each** (`ImpactLook`, 19 knobs
in `impact_knobs()`, set by name and clamped with `set_impact_knob`). The DUST is a whole
`BlastLook`, set as `dust.<knob>`, with `dust_speed` setting how fast it leaves. `ricochet` sets
the burst's axis between the surface's normal and the round's glance (its way mirrored in the
surface), always lifted out of the surface. Then come SPARKS (count, speed, life, heat, cone),
CHIPS (count, speed, size, life, shade, cone), GRIT (fines that hang and drift: count, speed,
shade) and DROPS (count, speed, cone). The presets are named `impact.<surface>`, using the words
engine/detail's surface classes use: stone, metal, wood, dirt, grass, sand, tile, liquid, and
unknown (stone's). `impact_look_for(word)` gives the look and returns none for the sky. The ROUND
(`ImpactRound`) sizes the burst like a blast's `scale` and says how much of the look's sparks it
strikes: `round.nail` gives all of them, and `round.pellet` gives 0.6 of the size and a third of
the sparks. A shotgun's shot is a handful of pellet hits, each with its own seed.

**Drawn by the passes already there.** `dust_of` is the puff as a `Blast` on the surface, thrown
out along the axis. The explosions pass marches it as smoke of the surface's albedo and tint.
`throw_impact` puts the sparks, chips, grit and drops into the particle pool. Each kind flies its
own way (`spark_particle`):
- A chip falls on its arc in the surface's shade (`Spark::albedo`).
- Grit is slowed by the air at once and drifts down at a seventh of gravity, half clear.
- A drop is a bright, half-clear streak.
- A spark glows and cools as a blast's does.

The MARK the hit leaves is the decals' (engine/detail's `shot_hit`, pressed at the same point and
normal), so the hole and the burst never depend on each other. A game classes the surface it hit
(engine/detail's `classify_surface`) and asks `impact_look_for(surface_name(class))`.

**The cost.** The dust is staged in the same list as the blasts, so the 32 the pass draws are shared. A hit's throw is 4 to 24 things into the pool. On the software adapter at 320 x 180 (`gpu-impact`), a shotgun's spread of nine puffs filling a third of the view costs the pass about 3 ms (0.13 ms with nothing in view); the CPU throws the spread in about 0.12 ms (a debug build).

**Not yet:** hits in a real level, where the shooter's code traces the round, classes the face it
hit by its texture and hands the impact here and the hole to the decals (M18, M42f); a light for
the metal's sparks; chips tinted by the surface's own texture rather than its shade.

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

The picture's settings are one table of ROWS in the game (Fall Line's:
`games/fall_line/app/ladder.hpp`), each a ladder of STOPS, cheapest first. A stop maps to the
frame's parameters (`FrameOptions`: the render scale, the shadow maps' size, the trail maps'
texels, the flakes, the cloud march's share of its steps, whether it marches a quarter of the
pixels and whether that march is spread over frames, the picture's reach), never to a code path
of its own. Each row is its
own bill, because a machine can be short of pixels and rich in passes, or the other way round.

- **The price table.** Every stop carries a COST, the ms it adds to a frame over its row's
  cheapest on the table's reference machine, and a WORTH, what it adds to the look on one
  0–100 scale for every row (a judgement, argued beside the row in `ladder.cpp`). The `ladder`
  lab's `--price` fills the costs: the top picture, every row lowered alone to each of its stops,
  and the floor (every row at its cheapest), each timed under a flurry (the dearest sky: heaps
  under a sheet, and the fall) from seed 38's start looking 15 degrees up, so the sky fills the
  top of the frame and every row draws something (`--view=chase` times the camera behind him,
  where the clouds' march is a sliver of the frame). On a software adapter each picture is timed
  in rounds, keeping its least median.
- **The fit** (`render/ladder.hpp`, generic). `fit_ladder` takes the picture of most worth whose
  cost fits the budget over the floor: a knapsack with a choice in each row, solved exactly in
  hundredths of a millisecond. A greedy walk would hide a dear stop that matters behind a cheap
  one that does not, and could hand a slower machine more of the look than a faster one. A row
  marked to go first (Fall Line's RESOLUTION, since a softer picture loses nothing in it) steps
  down before any other row loses a stop.
- **One machine is the reference scaled.** `machine_scale` reads a machine as the reference made
  faster or slower: its timed frame at the picture it drew, over what the table says that
  picture costs. The automatic preset (`auto_picture`) fits, applies, times again and fits again,
  so the second reading is of the neighbourhood the answer lies in.
- **The presets** (low, medium, high, top) are fixed picks a player can choose by name; the
  automatic preset is the fit at the display's budget.

The table's costs today are the software adapter's (lavapipe at 1280 × 720), whose ratios are a
CPU raster's: the reference machine's prices (the Mac, § Where it is developed) replace them.
- **The reach and its mist.** `FrameView::reach_m` is how far a picture draws. The ground's
  clipmap draws only the levels that cover it (`clipmap_levels_to`: 1, 2 or 4 km round the eye
  where all eight reach 8 km), the instances' cull drops every one past it (counted with the
  frustum's), and the haze closes every surface but the cloud into the sky behind its pixel from
  half the reach to all of it (`haze` in `wgsl/frame.wgsl`; the cloud's own colour goes through
  `haze_air`, the air alone, since it is sky). While there is a reach the sky under the horizon is
  drawn as the horizon's, so the land past it is lost in a bright haze, never seen as an edge or a
  dark sea beyond it. Fall Line's DISTANCE row is priced where it is seen, from over the summit
  on a clear day (`--view=vista --lift=0 --fall=0`): there the woods past 2 km are most of the
  frame's cost on the software adapter.

The table's costs today are the software adapter's (lavapipe at 1280 × 720), whose ratios are a
CPU raster's: the reference machine's prices (the Mac, § Where it is developed) replace them.
What the ladder does not have yet: the cloud's march spread over frames with reprojection, and
changing a stop without making the frame again (the reach already can: it is a field of the
view, not of the frame).

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
- **The wire probe.** With `TerrainSource::wire_probe` and `create_instance_pipelines`'
  `wire_probe`, the terrain and every instance family also get a line-list pipeline over the
  same vertex stage and line index buffers made once: `encode_terrain_wire` and
  `encode_instances_wire` draw the last frame's culled tiles and lists as their triangles'
  edges into a `kWireFormat` target, their instance counts copied from the cull's indirect
  arguments, tested against the frame's depth (read only) with each fragment's depth pushed a
  hair nearer (`frag_depth`: line topologies take no depth bias), so the ground hides what is
  behind it. The ground is banded by distance in doublings from 32 m, so the clipmap's rings
  show; the instances are one colour. It is what `ge shot --gpu --mode=wireframe` draws.
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
| `engine/render/` | Mechanisms any game could use: the pass table and named targets, the frame's stats, GPU-driven instance families and their culling, the depth pyramid, the heightfield clipmap renderer, the horizon map, cascaded shadows and the shadow atlas, the sky tables and aerial perspective, the cloud volume, TAA-U, post, the 2D pass, compute particles, skinning, and the shared WGSL library |
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

Data baked into the engine as generated tables:

| Data | Licence | For | Milestone |
| --- | --- | --- | --- |
| The lunar orbiter's wide-angle camera colour mosaic, as NASA's Scientific Visualization Studio publishes it in its CGI Moon Kit (<https://svs.gsfc.nasa.gov/4720>) | Public domain; credit NASA's Scientific Visualization Studio | The moon's albedo (`engine/render/moon_data.cpp`) | M8f6 |

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
