// Morph meshes drawn: the upload of every model's frames, coordinates, triangles and skin (the
// atlas), the ramps, the draws staged a frame relative to the eye, and the pass.
//
// ge:summary Morph meshes: the upload, the skin atlas, the ramps, the staged draws and the pass
// ge:keywords render morph upload atlas shelf-pack ramps tint draws pipeline pose_morph
// ge:keywords morph_texel pack_morph_atlas ramp_from
// ge:skill render
// ge:test tests/render_morph_test.cpp
#include "render/morph.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>

#include "core/libm/libm.hpp"
#include "render/morph.wgsl.hpp"

namespace ge::render {

namespace {

using BU = wgpu::BufferUsage;

// morph.wgsl's Model.
struct GpuModel {
  uint32_t first_vertex, vertex_count, first_index, first_uv;
  uint32_t rect[4];
};
static_assert(sizeof(GpuModel) == 32, "match morph.wgsl's Model");

// morph.wgsl's Draw.
struct GpuDraw {
  float now_place[4];
  float now_rotation[4];
  float last_place[4];
  float last_rotation[4];
  uint32_t frames[4];
  float blends[4];
  uint32_t info[4];
  float ambient[4];
  float shade[4];
  float shade_dir[4];
};
static_assert(sizeof(GpuDraw) == 160, "match morph.wgsl's Draw");

float srgb_to_linear(uint8_t v) {
  const double c = v / 255.0;
  return static_cast<float>(c <= 0.04045 ? c / 12.92 : core::libm::pow((c + 0.055) / 1.055, 2.4));
}

uint32_t clamp_frame(const MorphModel& m, uint32_t f) {
  const uint32_t n = static_cast<uint32_t>(m.positions.size());
  return f < n ? f : n - 1;
}

}  // namespace

uint32_t ramp_from(TintRamps& ramps, const float colour[3]) {
  const uint32_t row = ramps.rows();
  for (uint32_t s = 0; s < kTintSteps; ++s) {
    // Step 0 half again as bright as the colour (toward white), step 15 near black.
    const float f = 1.5f * static_cast<float>(kTintSteps - 1 - s) / (kTintSteps - 1) + 0.06f;
    for (int k = 0; k < 3; ++k) {
      const float c = colour[k];
      const float v = f <= 1 ? c * f : c + (1 - c) * (f - 1) * 0.6f;
      ramps.rgb.push_back(v);
    }
  }
  return row;
}

bool pack_morph_atlas(std::span<const MorphModel> models, std::vector<AtlasRect>& rects) {
  rects.assign(models.size(), AtlasRect{});
  std::vector<size_t> order(models.size());
  std::iota(order.begin(), order.end(), size_t{0});
  std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
    return models[a].skin.height > models[b].skin.height;
  });
  uint32_t x = 0, y = 0, shelf = 0;
  for (size_t i : order) {
    const MorphSkin& s = models[i].skin;
    const uint32_t w = s.width < 1 ? 1 : s.width, h = s.height < 1 ? 1 : s.height;
    if (w > kMorphAtlasSize) return false;
    if (x + w > kMorphAtlasSize) {
      x = 0;
      y += shelf + 1;
      shelf = 0;
    }
    if (y + h > kMorphAtlasSize) return false;
    rects[i] = {x, y, w, h};
    x += w + 1;
    shelf = h > shelf ? h : shelf;
  }
  return true;
}

void pose_morph(const MorphModel& model, uint32_t a, uint32_t b, float t,
                std::vector<float>& positions, std::vector<float>& normals) {
  a = clamp_frame(model, a);
  b = clamp_frame(model, b);
  const size_t n = model.positions[a].size();
  positions.resize(n);
  normals.resize(n);
  const std::vector<float>&pa = model.positions[a], &pb = model.positions[b];
  const std::vector<float>&na = model.normals[a], &nb = model.normals[b];
  for (size_t i = 0; i < n; i += 3) {
    float len2 = 0;
    for (size_t k = 0; k < 3; ++k) {
      positions[i + k] = pa[i + k] + (pb[i + k] - pa[i + k]) * t;
      normals[i + k] = na[i + k] + (nb[i + k] - na[i + k]) * t;
      len2 += normals[i + k] * normals[i + k];
    }
    const float len = std::sqrt(len2);
    for (size_t k = 0; k < 3; ++k) normals[i + k] /= len > 1e-6f ? len : 1e-6f;
  }
}

void morph_texel(const MorphSkin& skin, const TintRamps& ramps, const uint32_t tint[2], uint32_t x,
                 uint32_t y, float rgb[3], float* glow) {
  const size_t i = static_cast<size_t>(y) * skin.width + x;
  const uint8_t mark = skin.marks.empty() ? kMarkPlain : skin.marks[i];
  const uint32_t region = mark >> 6;
  if (glow) *glow = 0;
  if (region != 0) {
    const uint32_t row = tint[region == 2 ? 1 : 0];
    const size_t at = (static_cast<size_t>(row) * kTintSteps + (mark & 15u)) * 3;
    for (int k = 0; k < 3; ++k) rgb[k] = ramps.rgb[at + static_cast<size_t>(k)];
    return;
  }
  for (int k = 0; k < 3; ++k) rgb[k] = srgb_to_linear(skin.rgba[i * 4 + static_cast<size_t>(k)]);
  if (glow && mark == kMarkGlow) *glow = 1;
}

bool create_morph(gpu::Device& device, gpu::PipelineSet& pipelines, Morph& morph,
                  const wgpu::BindGroupLayout& frame, std::span<const MorphModel> models,
                  const TintRamps& ramps) {
  std::vector<AtlasRect> rects;
  if (models.empty() || !pack_morph_atlas(models, rects)) return false;
  // Every model's frames, coordinates and triangles, one after another.
  std::vector<GpuModel> table(models.size());
  std::vector<float> vertices, uvs;
  std::vector<uint32_t> indices;
  uint32_t atlas_h = 1;
  morph.index_counts.clear();
  for (size_t m = 0; m < models.size(); ++m) {
    const MorphModel& mm = models[m];
    const uint32_t n = mm.vertex_count();
    table[m] = {static_cast<uint32_t>(vertices.size() / 8),
                n,
                static_cast<uint32_t>(indices.size()),
                static_cast<uint32_t>(uvs.size() / 2),
                {rects[m].x, rects[m].y, rects[m].w, rects[m].h}};
    for (size_t f = 0; f < mm.positions.size(); ++f) {
      for (uint32_t v = 0; v < n; ++v) {
        const float* p = &mm.positions[f][size_t{v} * 3];
        const float* q = &mm.normals[f][size_t{v} * 3];
        vertices.insert(vertices.end(), {p[0], p[1], p[2], 0, q[0], q[1], q[2], 0});
      }
    }
    for (uint32_t v = 0; v < n; ++v) {
      const bool has = mm.uv.size() >= size_t{n} * 2;
      uvs.push_back(has ? mm.uv[size_t{v} * 2] : 0);
      uvs.push_back(has ? mm.uv[size_t{v} * 2 + 1] : 0);
    }
    indices.insert(indices.end(), mm.indices.begin(), mm.indices.end());
    morph.index_counts.push_back(static_cast<uint32_t>(mm.indices.size()));
    atlas_h = std::max(atlas_h, rects[m].y + rects[m].h);
  }
  // The atlas: each skin's colour, its marks in alpha.
  std::vector<uint8_t> atlas(size_t{kMorphAtlasSize} * atlas_h * 4, 0);
  for (size_t m = 0; m < models.size(); ++m) {
    const MorphSkin& s = models[m].skin;
    for (uint32_t y = 0; y < s.height; ++y) {
      for (uint32_t x = 0; x < s.width; ++x) {
        const size_t src = size_t{y} * s.width + x;
        const size_t dst = (size_t{rects[m].y + y} * kMorphAtlasSize + rects[m].x + x) * 4;
        std::memcpy(&atlas[dst], &s.rgba[src * 4], 3);
        atlas[dst + 3] = s.marks.empty() ? kMarkPlain : s.marks[src];
      }
    }
  }
  std::vector<float> ramp4;
  for (size_t i = 0; i + 2 < ramps.rgb.size(); i += 3) {
    ramp4.insert(ramp4.end(), {ramps.rgb[i], ramps.rgb[i + 1], ramps.rgb[i + 2], 0});
  }
  if (ramp4.empty()) ramp4.assign(4 * size_t{kTintSteps}, 0.5f);

  auto upload = [&](const char* label, const void* data, size_t bytes) {
    wgpu::Buffer b = make_buffer(device, label, bytes < 16 ? 16 : bytes, BU::Storage);
    if (bytes > 0) device.queue.WriteBuffer(b, 0, data, bytes);
    return b;
  };
  morph.models = upload("morph.models", table.data(), table.size() * sizeof(GpuModel));
  morph.vertices = upload("morph.vertices", vertices.data(), vertices.size() * sizeof(float));
  morph.uvs = upload("morph.uvs", uvs.data(), uvs.size() * sizeof(float));
  morph.indices = upload("morph.indices", indices.data(), indices.size() * sizeof(uint32_t));
  morph.ramps = upload("morph.ramps", ramp4.data(), ramp4.size() * sizeof(float));
  morph.draws = make_buffer(device, "morph.draws", kMaxMorphDraws * sizeof(GpuDraw), BU::Storage);
  morph.atlas = make_texture(device, "morph.atlas", kMorphAtlasSize, atlas_h, 1,
                             wgpu::TextureFormat::RGBA8Unorm,
                             wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst);
  wgpu::TexelCopyTextureInfo dst{};
  dst.texture = morph.atlas;
  wgpu::TexelCopyBufferLayout layout{};
  layout.bytesPerRow = kMorphAtlasSize * 4;
  layout.rowsPerImage = atlas_h;
  wgpu::Extent3D size{kMorphAtlasSize, atlas_h, 1};
  device.queue.WriteTexture(&dst, atlas.data(), atlas.size(), &layout, &size);
  morph.staged.assign(kMaxMorphDraws * sizeof(GpuDraw), 0);
  morph.staged_models.assign(kMaxMorphDraws, 0);

  const auto vf = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
  morph.layout =
      make_layout(device, "morph",
                  {Slot::storage_read, Slot::storage_read, Slot::storage_read, Slot::storage_read,
                   Slot::storage_read, Slot::storage_read, Slot::texture},
                  vf);
  morph.group = make_group(device, "morph", morph.layout,
                           {morph.models, morph.vertices, morph.uvs, morph.indices, morph.draws,
                            morph.ramps, morph.atlas.CreateView()});
  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::morph, "morph");
  wgpu::ColorTargetState colours[2]{};
  colours[0].format = kHdrFormat;
  colours[1].format = kMotionFormat;
  wgpu::FragmentState fragment{};
  fragment.module = module;
  fragment.entryPoint = "fs_main";
  fragment.targetCount = 2;
  fragment.targets = colours;
  wgpu::DepthStencilState depth{};
  depth.format = kDepthFormat;
  depth.depthWriteEnabled = true;
  depth.depthCompare = wgpu::CompareFunction::Greater;  // reversed z
  wgpu::RenderPipelineDescriptor desc{};
  desc.label = "morph.draw";
  desc.layout = make_pipeline_layout(device, "morph.draw", {frame, morph.layout});
  desc.vertex.module = module;
  desc.vertex.entryPoint = "vs_main";
  desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  desc.primitive.frontFace = wgpu::FrontFace::CW;
  desc.primitive.cullMode = wgpu::CullMode::Back;
  desc.depthStencil = &depth;
  desc.fragment = &fragment;
  morph.p_draw = pipelines.add_render(device, desc);
  return true;
}

void prepare_morph(gpu::Device& device, Morph& morph, std::span<const MorphDraw> draws,
                   const FrameView& view) {
  morph.drawn = 0;
  morph.dropped = 0;
  for (const MorphDraw& d : draws) {
    if (d.model >= morph.index_counts.size()) continue;
    if (morph.drawn == kMaxMorphDraws) {
      ++morph.dropped;
      continue;
    }
    GpuDraw g{};
    auto place = [&](const MorphPose& p, float out[4], float rot[4]) {
      for (int k = 0; k < 3; ++k) out[k] = static_cast<float>(p.position[k] - view.eye[k]);
      out[3] = p.scale;
      std::memcpy(rot, p.rotation, sizeof p.rotation);
    };
    place(d.now, g.now_place, g.now_rotation);
    place(d.last, g.last_place, g.last_rotation);
    g.frames[0] = d.now.frame_a;
    g.frames[1] = d.now.frame_b;
    g.frames[2] = d.last.frame_a;
    g.frames[3] = d.last.frame_b;
    g.blends[0] = d.now.blend;
    g.blends[1] = d.last.blend;
    g.blends[2] = d.glow;
    g.blends[3] = d.outdoor;
    g.info[0] = d.model;
    g.info[1] = d.tint[0];
    g.info[2] = d.tint[1];
    g.info[3] = d.smooth ? 1 : 0;
    for (int k = 0; k < 3; ++k) {
      g.ambient[k] = d.ambient[k];
      g.shade[k] = d.shade[k];
      g.shade_dir[k] = d.shade_dir[k];
    }
    std::memcpy(&morph.staged[morph.drawn * sizeof(GpuDraw)], &g, sizeof g);
    morph.staged_models[morph.drawn] = d.model;
    ++morph.drawn;
  }
  if (morph.drawn > 0) {
    device.queue.WriteBuffer(morph.draws, 0, morph.staged.data(), morph.drawn * sizeof(GpuDraw));
  }
}

void encode_morph(const Morph& morph, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                  wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                  const wgpu::BindGroup& frame) {
  wgpu::RenderPassColorAttachment colours[2]{};
  colours[0].view = targets.hdr_view;
  colours[1].view = targets.motion_view;
  for (auto& c : colours) {
    c.loadOp = wgpu::LoadOp::Load;
    c.storeOp = wgpu::StoreOp::Store;
  }
  wgpu::RenderPassDepthStencilAttachment depth{};
  depth.view = targets.depth_view;
  depth.depthLoadOp = wgpu::LoadOp::Load;
  depth.depthStoreOp = wgpu::StoreOp::Store;
  wgpu::RenderPassDescriptor desc{};
  desc.colorAttachmentCount = 2;
  desc.colorAttachments = colours;
  desc.depthStencilAttachment = &depth;
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "morph");
  if (morph.drawn > 0) {
    pass.SetPipeline(pipelines.render(morph.p_draw));
    pass.SetBindGroup(0, frame);
    pass.SetBindGroup(1, morph.group);
    for (uint32_t k = 0; k < morph.drawn; ++k) {
      pass.Draw(morph.index_counts[morph.staged_models[k]], 1, 0, k);
      ++rec.current().draws;
    }
  }
  pass.End();
}

}  // namespace ge::render
