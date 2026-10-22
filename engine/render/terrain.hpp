// THE HEIGHTFIELD RENDERER (docs/rendering.md § Terrain): any game hands it a baked grid of
// heights and an albedo over the same grid, and it draws the ground to the horizon as a
// GEOMETRY CLIPMAP.
//
// - LEVELS: `kLevels` square levels round the eye, each `kTileQuads` x 4 quads a side; level 0's
//   quad is `base_spacing` m and each level doubles it. A level's square is centred on the eye
//   snapped to twice its spacing (`clipmap_levels`), so a finer level always lies inside the
//   next and on its grid.
// - ONE MESH: a tile of kTileQuads x kTileQuads quads, its vertices pulled from the vertex index
//   (no vertex buffer); a level is 4 x 4 tiles and every visible tile is an instance of one
//   indexed draw.
// - CULLED ON THE GPU: a compute pass tests every tile's box (its height range from a min/max
//   pyramid baked at load) against the frustum and appends the visible ones to the instance list
//   behind the draw's indirect arguments. The CPU never walks the tiles.
// - SEAMS WITHOUT `discard`: toward its outer edge a level's odd vertices morph onto its coarser
//   neighbour's grid (by the vertex's distance from the eye, so nothing pops as the eye moves),
//   and a coarse vertex the finer level covers sinks under it.
// - NORMALS from the height texture, baked into a filterable texture at load by compute, so the
//   vertex stays small and every pixel is lit by its own normal.
//
// The pass draws into the frame's HDR target and depth (cleared here: it is the first opaque
// pass until the depth prepass lands, M7c). Shaders: terrain.wgsl (draw), terrain_cull.wgsl
// and terrain_normals.wgsl (compute).
//
// ge:summary The heightfield renderer: a GPU-culled geometry clipmap with morphed seams
// ge:keywords render terrain heightfield clipmap geometry-clipmap tile morph seam sink lod
// ge:keywords gpu-culling indirect-draw min-max-pyramid normals albedo vertex-pulling
// ge:keywords clipmap_levels create_terrain prepare_terrain encode_terrain_cull draw_terrain
// ge:skill render
// ge:test tests/render_terrain_test.cpp
#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "core/heightfield.hpp"
#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"
#include "render/horizon.hpp"

namespace ge::render {

inline constexpr int kTileQuads = 32;  // a tile's quads a side (terrain.wgsl's override)
inline constexpr int kLevelTiles = 4;  // a level's tiles a side
inline constexpr int kMaxLevels = 10;

struct ClipLevel {
  double origin_x, origin_z;  // the level's square's min corner, world m
  double spacing;             // m a quad
};

// The levels round the eye at (x, z): level i's spacing is base * 2^i, its square
// kLevelTiles * kTileQuads quads a side centred on the eye snapped to 2 * spacing.
void clipmap_levels(double x, double z, double base_spacing, int levels, ClipLevel* out);

// What a game hands the renderer: the heights, and the albedo (sRGB-encoded RGBA8, one texel a
// sample, rows along z) over the same grid.
struct TerrainSource {
  const core::Heightfield* height = nullptr;
  const std::vector<uint8_t>* albedo = nullptr;
  double base_spacing = 1.0;    // level 0's quad, m
  int levels = 7;               // ≤ kMaxLevels
  HorizonSettings horizon;      // the ground's own shadow
  bool overdraw_probe = false;  // labs: build the pipelines encode_terrain_overdraw draws with
};

struct Terrain {
  int levels = 0;
  double base_spacing = 1.0;
  int cols = 0, rows = 0;
  double origin_x = 0, origin_z = 0, cell = 1;
  int pyramid_mips = 0;
  wgpu::Texture height, normals, albedo, pyramid;
  wgpu::Buffer params, tiles, indirect, indices;
  wgpu::BindGroupLayout layout, cull_layout, normals_layout;
  wgpu::BindGroup group, cull_group, normals_group;
  uint32_t p_depth = 0, p_draw = 0, p_cull = 0, p_normals = 0;
  uint32_t p_count_equal = 0, p_count_greater = 0;  // the overdraw probe's, when asked for
  bool normals_built = false;
  Horizon horizon;
  // terrain.wgsl's `TerrainParams`, filled by prepare.
  struct Params {
    float levels[kMaxLevels][4];  // origin x, origin z, spacing, 0
    float info[4];                // level count, tile quads, sink (spacings), morph start
    float field[4];               // origin x, origin z, cell, 0
    uint32_t field_size[4];       // cols, rows, pyramid mips, 0
    float horizon[4];             // the horizon maps' blend (Horizon::blend)
    float horizon_grid[4];        // the maps' origin x, z, cell, 0
    uint32_t horizon_size[4];     // the maps' width, height, 0, 0
  } staged{};
};

// Upload the source's grids, bake the min/max pyramid and add the pipelines to the manifest.
void create_terrain(gpu::Device& device, gpu::PipelineSet& pipelines, Terrain& terrain,
                    const TerrainSource& source, const wgpu::BindGroupLayout& frame);
// The levels round this frame's eye and the horizon maps the sun needs, staged and uploaded; the
// indirect count reset.
void prepare_terrain(gpu::Device& device, Terrain& terrain, const FrameView& view);
// The normals' bake (the first frame) and the cull ("terrain.cull" compute pass).
void encode_terrain_cull(Terrain& terrain, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                         wgpu::CommandEncoder& encoder, const wgpu::BindGroup& frame);
// The horizon maps' bakes that are due ("terrain.horizon").
void encode_terrain_horizon(Terrain& terrain, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                            wgpu::CommandEncoder& encoder);
// The depth prepass ("terrain.depth"): clears the depth and writes every visible tile's.
void encode_terrain_depth(const Terrain& terrain, const gpu::PipelineSet& pipelines,
                          FrameRecorder& rec, wgpu::CommandEncoder& encoder,
                          const FrameTargets& targets, const wgpu::BindGroup& frame);
// The colour pass ("terrain"): clears the HDR and motion targets and shades the pixels whose
// depth is `equal` to the prepass's, each once; the depth is read only.
void encode_terrain(const Terrain& terrain, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                    wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                    const wgpu::BindGroup& frame);
// The overdraw probe (labs; needs TerrainSource::overdraw_probe): every fragment that passes the
// depth test adds one to `count` (an R16Float view at the render size). After the prepass it
// tests `equal` against its depth (one a covered pixel); otherwise it clears the depth and
// tests `greater` as a pass with no prepass would (the overdraw that saves).
void encode_terrain_overdraw(const Terrain& terrain, const gpu::PipelineSet& pipelines,
                             wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                             const wgpu::BindGroup& frame, const wgpu::TextureView& count,
                             bool after_prepass);

// The CPU's min/max pyramid of a heightfield: mip 0's texel i spans samples i and i + 1 (a
// cell), each mip after halves the last (rounding down, as a texture's mips do; the last texel
// of a row or column takes what is left over). Interleaved (min, max) floats, mip after mip;
// `sizes` gets each mip's width and height.
std::vector<float> min_max_pyramid(const core::Heightfield& field,
                                   std::vector<std::pair<int, int>>& sizes);

}  // namespace ge::render
