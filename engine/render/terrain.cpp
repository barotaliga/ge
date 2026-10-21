// The clipmap's levels, the min/max pyramid, the terrain's textures, buffers and pipelines, and
// its cull and draw passes.
//
// ge:summary The clipmap's level placement, pyramid bake, resources and passes
// ge:keywords render terrain clipmap pyramid upload indirect cull draw
// ge:skill render
// ge:test tests/render_terrain_test.cpp
#include "render/terrain.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include "render/terrain.wgsl.hpp"
#include "render/terrain_cull.wgsl.hpp"
#include "render/terrain_normals.wgsl.hpp"

namespace ge::render {

namespace {

using TU = wgpu::TextureUsage;
using BU = wgpu::BufferUsage;

// How far down a covered coarse vertex sinks, in its level's spacings: below the finer surface
// on any slope up to 63 degrees.
constexpr float kSink = 2.0f;
// Where the morph starts, as a share of a level's half side.
constexpr float kMorphStart = 0.8f;

constexpr uint32_t kTilesPerLevel = kLevelTiles * kLevelTiles;
constexpr uint32_t kIndexCount = kTileQuads * kTileQuads * 6;

}  // namespace

void clipmap_levels(double x, double z, double base_spacing, int levels, ClipLevel* out) {
  const double half_quads = kLevelTiles * kTileQuads / 2.0;
  for (int i = 0; i < levels; ++i) {
    const double s = base_spacing * std::ldexp(1.0, i);
    const double cx = std::round(x / (2 * s)) * 2 * s;
    const double cz = std::round(z / (2 * s)) * 2 * s;
    out[i] = ClipLevel{cx - half_quads * s, cz - half_quads * s, s};
  }
}

std::vector<float> min_max_pyramid(const core::Heightfield& field,
                                   std::vector<std::pair<int, int>>& sizes) {
  sizes.clear();
  std::vector<float> out;
  int w = std::max(field.cols - 1, 1), h = std::max(field.rows - 1, 1);
  const auto at = [&](int c, int r) {
    c = std::clamp(c, 0, field.cols - 1);
    r = std::clamp(r, 0, field.rows - 1);
    return field
        .data[static_cast<size_t>(r) * static_cast<size_t>(field.cols) + static_cast<size_t>(c)];
  };
  sizes.emplace_back(w, h);
  size_t base = 0;
  for (int r = 0; r < h; ++r) {
    for (int c = 0; c < w; ++c) {
      const float a = at(c, r), b = at(c + 1, r), d = at(c, r + 1), e = at(c + 1, r + 1);
      out.push_back(std::min(std::min(a, b), std::min(d, e)));
      out.push_back(std::max(std::max(a, b), std::max(d, e)));
    }
  }
  while (w > 1 || h > 1) {
    const int pw = w, ph = h;
    // Halved as a texture's mips are (rounding down); the last texel of an odd row or column
    // also takes the one left over, so every texel of the mip before is in one of this mip's.
    w = std::max(w / 2, 1);
    h = std::max(h / 2, 1);
    const size_t next = out.size();
    for (int r = 0; r < h; ++r) {
      const int rows = r == h - 1 ? ph - r * 2 : 2;
      for (int c = 0; c < w; ++c) {
        const int cols = c == w - 1 ? pw - c * 2 : 2;
        float lo = 1e30f, hi = -1e30f;
        for (int dr = 0; dr < rows; ++dr) {
          for (int dc = 0; dc < cols; ++dc) {
            const int sc = c * 2 + dc, sr = r * 2 + dr;
            const size_t i =
                base +
                (static_cast<size_t>(sr) * static_cast<size_t>(pw) + static_cast<size_t>(sc)) * 2;
            lo = std::min(lo, out[i]);
            hi = std::max(hi, out[i + 1]);
          }
        }
        out.push_back(lo);
        out.push_back(hi);
      }
    }
    base = next;
    sizes.emplace_back(w, h);
  }
  return out;
}

void create_terrain(gpu::Device& device, gpu::PipelineSet& pipelines, Terrain& t,
                    const TerrainSource& source, const wgpu::BindGroupLayout& frame) {
  const core::Heightfield& f = *source.height;
  t.levels = std::clamp(source.levels, 1, kMaxLevels);
  t.base_spacing = source.base_spacing;
  t.cols = f.cols;
  t.rows = f.rows;
  t.origin_x = f.origin_x;
  t.origin_z = f.origin_z;
  t.cell = f.cell;
  const uint32_t w = static_cast<uint32_t>(f.cols), h = static_cast<uint32_t>(f.rows);

  auto upload = [&](const wgpu::Texture& tex, uint32_t mip, const void* data, uint32_t tw,
                    uint32_t th, uint32_t texel_bytes) {
    wgpu::TexelCopyTextureInfo dst{};
    dst.texture = tex;
    dst.mipLevel = mip;
    wgpu::TexelCopyBufferLayout layout{};
    layout.bytesPerRow = tw * texel_bytes;
    layout.rowsPerImage = th;
    const wgpu::Extent3D size{tw, th, 1};
    device.queue.WriteTexture(&dst, data, size_t{tw} * th * texel_bytes, &layout, &size);
  };

  t.height = make_texture(device, "terrain.height", w, h, 1, wgpu::TextureFormat::R32Float,
                          TU::TextureBinding | TU::CopyDst);
  upload(t.height, 0, f.data.data(), w, h, 4);
  t.albedo = make_texture(device, "terrain.albedo", w, h, 1, wgpu::TextureFormat::RGBA8UnormSrgb,
                          TU::TextureBinding | TU::CopyDst);
  upload(t.albedo, 0, source.albedo->data(), w, h, 4);
  t.normals = make_texture(device, "terrain.normals", w, h, 1, wgpu::TextureFormat::RGBA16Float,
                           TU::TextureBinding | TU::StorageBinding);

  std::vector<std::pair<int, int>> sizes;
  const std::vector<float> pyramid = min_max_pyramid(f, sizes);
  t.pyramid_mips = static_cast<int>(sizes.size());
  t.pyramid =
      make_texture(device, "terrain.pyramid", static_cast<uint32_t>(sizes[0].first),
                   static_cast<uint32_t>(sizes[0].second), 1, wgpu::TextureFormat::RG32Float,
                   TU::TextureBinding | TU::CopyDst, static_cast<uint32_t>(sizes.size()));
  size_t at = 0;
  for (size_t m = 0; m < sizes.size(); ++m) {
    const auto [mw, mh] = sizes[m];
    upload(t.pyramid, static_cast<uint32_t>(m), &pyramid[at], static_cast<uint32_t>(mw),
           static_cast<uint32_t>(mh), 8);
    at += static_cast<size_t>(mw) * static_cast<size_t>(mh) * 2;
  }

  // One tile's triangles over (kTileQuads + 1)^2 pulled vertices, every quad split the same way
  // (so a morphed fine quad shares its coarse neighbour's diagonal).
  std::vector<uint16_t> idx;
  idx.reserve(kIndexCount);
  const uint16_t n = kTileQuads + 1;
  for (uint16_t r = 0; r < kTileQuads; ++r) {
    for (uint16_t c = 0; c < kTileQuads; ++c) {
      const uint16_t a = static_cast<uint16_t>(r * n + c), b = static_cast<uint16_t>(a + 1);
      const uint16_t d = static_cast<uint16_t>(a + n), e = static_cast<uint16_t>(d + 1);
      for (uint16_t v : {a, d, e, a, e, b}) idx.push_back(v);
    }
  }
  t.indices = make_buffer(device, "terrain.indices", idx.size() * 2, BU::Index);
  device.queue.WriteBuffer(t.indices, 0, idx.data(), idx.size() * 2);
  t.params = make_buffer(device, "terrain.params", sizeof(Terrain::Params), BU::Uniform);
  t.tiles =
      make_buffer(device, "terrain.tiles", uint64_t{kMaxLevels} * kTilesPerLevel * 4, BU::Storage);
  t.indirect =
      make_buffer(device, "terrain.indirect", 20, BU::Storage | BU::Indirect | BU::CopySrc);

  const auto vf = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
  t.layout = make_layout(
      device, "terrain.draw",
      {Slot::uniform, Slot::texture_raw, Slot::texture, Slot::texture, Slot::storage_read}, vf);
  t.cull_layout = make_layout(device, "terrain.cull",
                              {Slot::uniform, Slot::texture_raw, Slot::storage, Slot::storage},
                              wgpu::ShaderStage::Compute);
  t.normals_layout =
      make_layout(device, "terrain.normals", {Slot::uniform, Slot::texture_raw, Slot::store_2d},
                  wgpu::ShaderStage::Compute);
  t.group = make_group(
      device, "terrain.draw", t.layout,
      {t.params, t.height.CreateView(), t.normals.CreateView(), t.albedo.CreateView(), t.tiles});
  t.cull_group = make_group(device, "terrain.cull", t.cull_layout,
                            {t.params, t.pyramid.CreateView(), t.tiles, t.indirect});
  t.normals_group = make_group(device, "terrain.normals", t.normals_layout,
                               {t.params, t.height.CreateView(), t.normals.CreateView()});

  const wgpu::ShaderModule cull = gpu::create_shader(device, wgsl::terrain_cull, "terrain_cull");
  wgpu::ComputePipelineDescriptor cd{};
  cd.label = "terrain.cull";
  cd.layout = make_pipeline_layout(device, "terrain.cull", {frame, t.cull_layout});
  cd.compute.module = cull;
  cd.compute.entryPoint = "cull";
  t.p_cull = pipelines.add_compute(device, cd);

  const wgpu::ShaderModule normals =
      gpu::create_shader(device, wgsl::terrain_normals, "terrain_normals");
  wgpu::ComputePipelineDescriptor nd{};
  nd.label = "terrain.normals";
  nd.layout = make_pipeline_layout(device, "terrain.normals", {t.normals_layout});
  nd.compute.module = normals;
  nd.compute.entryPoint = "bake";
  t.p_normals = pipelines.add_compute(device, nd);

  const wgpu::ShaderModule draw = gpu::create_shader(device, wgsl::terrain, "terrain");
  wgpu::ConstantEntry tile{};
  tile.key = "kTile";
  tile.value = kTileQuads;
  wgpu::ColorTargetState colour{};
  colour.format = kHdrFormat;
  wgpu::FragmentState fragment{};
  fragment.module = draw;
  fragment.entryPoint = "fs_main";
  fragment.targetCount = 1;
  fragment.targets = &colour;
  wgpu::DepthStencilState depth{};
  depth.format = kDepthFormat;
  depth.depthWriteEnabled = true;
  depth.depthCompare = wgpu::CompareFunction::Greater;
  wgpu::RenderPipelineDescriptor desc{};
  desc.label = "terrain.draw";
  desc.layout = make_pipeline_layout(device, "terrain.draw", {frame, t.layout});
  desc.vertex.module = draw;
  desc.vertex.entryPoint = "vs_main";
  desc.vertex.constantCount = 1;
  desc.vertex.constants = &tile;
  desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  desc.primitive.cullMode = wgpu::CullMode::None;
  desc.depthStencil = &depth;
  desc.fragment = &fragment;
  t.p_draw = pipelines.add_render(device, desc);

  // The grid's half of the params never moves; prepare fills the levels.
  t.staged.info[1] = static_cast<float>(kTileQuads);
  t.staged.info[2] = kSink;
  t.staged.info[3] = kMorphStart;
  t.staged.field[0] = static_cast<float>(f.origin_x);
  t.staged.field[1] = static_cast<float>(f.origin_z);
  t.staged.field[2] = static_cast<float>(f.cell);
  t.staged.field_size[0] = w;
  t.staged.field_size[1] = h;
  t.staged.field_size[2] = static_cast<uint32_t>(t.pyramid_mips);
}

void prepare_terrain(gpu::Device& device, Terrain& t, const FrameView& view) {
  ClipLevel levels[kMaxLevels];
  clipmap_levels(view.eye[0], view.eye[2], t.base_spacing, t.levels, levels);
  for (int i = 0; i < t.levels; ++i) {
    t.staged.levels[i][0] = static_cast<float>(levels[i].origin_x);
    t.staged.levels[i][1] = static_cast<float>(levels[i].origin_z);
    t.staged.levels[i][2] = static_cast<float>(levels[i].spacing);
    t.staged.levels[i][3] = 0;
  }
  t.staged.info[0] = static_cast<float>(t.levels);
  device.queue.WriteBuffer(t.params, 0, &t.staged, sizeof t.staged);
  const uint32_t args[5] = {kIndexCount, 0, 0, 0, 0};
  device.queue.WriteBuffer(t.indirect, 0, args, sizeof args);
}

void encode_terrain_cull(Terrain& t, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                         wgpu::CommandEncoder& encoder, const wgpu::BindGroup& frame) {
  wgpu::ComputePassEncoder pass = rec.compute(encoder, "terrain.cull");
  PassStat& stat = rec.current();
  if (!t.normals_built) {
    pass.SetPipeline(pipelines.compute(t.p_normals));
    pass.SetBindGroup(0, t.normals_group);
    pass.DispatchWorkgroups((static_cast<uint32_t>(t.cols) + 7) / 8,
                            (static_cast<uint32_t>(t.rows) + 7) / 8);
    ++stat.dispatches;
    t.normals_built = true;
  }
  pass.SetPipeline(pipelines.compute(t.p_cull));
  pass.SetBindGroup(0, frame);
  pass.SetBindGroup(1, t.cull_group);
  pass.DispatchWorkgroups(static_cast<uint32_t>(t.levels));
  ++stat.dispatches;
  pass.End();
}

void encode_terrain(const Terrain& t, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                    wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                    const wgpu::BindGroup& frame) {
  wgpu::RenderPassColorAttachment colour{};
  colour.view = targets.hdr_view;
  colour.loadOp = wgpu::LoadOp::Clear;
  colour.storeOp = wgpu::StoreOp::Store;
  colour.clearValue = {0, 0, 0, 1};
  wgpu::RenderPassDepthStencilAttachment depth{};
  depth.view = targets.depth_view;
  depth.depthLoadOp = wgpu::LoadOp::Clear;
  depth.depthStoreOp = wgpu::StoreOp::Store;
  depth.depthClearValue = 0.0f;  // reversed-z: 0 is infinitely far
  wgpu::RenderPassDescriptor desc{};
  desc.colorAttachmentCount = 1;
  desc.colorAttachments = &colour;
  desc.depthStencilAttachment = &depth;
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "terrain");
  pass.SetPipeline(pipelines.render(t.p_draw));
  pass.SetBindGroup(0, frame);
  pass.SetBindGroup(1, t.group);
  pass.SetIndexBuffer(t.indices, wgpu::IndexFormat::Uint16);
  pass.DrawIndexedIndirect(t.indirect, 0);
  ++rec.current().draws;
  pass.End();
}

}  // namespace ge::render
