// The view-model pass: the held models' upload (frames, attributes, triangles, the flash's flags,
// the skins' atlas), a draw packed for the shader (the placement, the lens, the light, the view's
// basis), the pipelines and the two encodes.
//
// ge:summary The view-model pass: the upload, the packed draw, the pipelines and the encodes
// ge:keywords render viewmodel upload atlas pack_held_draw held_depth_floor held_candela lamp_of
// ge:keywords pipelines encode viewport depth-range
// ge:skill render
// ge:test tests/render_viewmodel_test.cpp
#include "render/viewmodel.hpp"

#include <algorithm>
#include <cstring>

#include "core/libm/libm.hpp"
#include "render/shadows.hpp"
#include "render/viewmodel.wgsl.hpp"

namespace ge::render {

namespace {

using BU = wgpu::BufferUsage;
namespace libm = core::libm;

// viewmodel.wgsl's HeldModel.
struct GpuModel {
  uint32_t at[4];    // first vertex, vertex count, first index, first attribute
  uint32_t rect[4];  // the skin's place in the atlas (w 0: none)
  uint32_t more[4];  // the first triangle's flag
};
static_assert(sizeof(GpuModel) == 48, "match viewmodel.wgsl's HeldModel");

void rows_of(const viewmodel::Placement& p, float out[12]) {
  for (int i = 0; i < 12; ++i) out[i] = static_cast<float>(p.m[i]);
}

}  // namespace

double held_depth_floor(double near_m, double guard_m) {
  if (!(guard_m > 0) || !(near_m > 0)) return 0.9;
  const double f = near_m / guard_m;
  return f < 0.9 ? f : 0.9;
}

float held_candela(float intensity, double white_lux) {
  return static_cast<float>(intensity * white_lux);
}

LocalLight lamp_of(const viewmodel::PointLight& light, double white_lux) {
  LocalLight l;
  for (int k = 0; k < 3; ++k) {
    l.position[k] = light.position[k];
    l.colour[k] = light.colour[k];
  }
  l.candela = held_candela(light.intensity, white_lux);
  l.range_m = light.range_m;
  l.radius_m = light.radius_m;
  return l;
}

GpuHeldDraw pack_held_draw(const HeldDraw& d, const FrameView& view, uint32_t width,
                           uint32_t height, double guard_m) {
  GpuHeldDraw g{};
  rows_of(d.now, g.now);
  rows_of(d.last, g.last);
  const double aspect = static_cast<double>(width) / static_cast<double>(height);
  const double gun = d.fov_y > 0 ? d.fov_y : d.now.fov_y > 0 ? d.now.fov_y : view.fov_y;
  const double tan_y = libm::tan(gun * 0.5);
  g.lens[0] = static_cast<float>(1.0 / (tan_y * aspect));
  g.lens[1] = static_cast<float>(1.0 / tan_y);
  g.lens[2] = static_cast<float>(kHeldNearM);
  g.lens[3] = static_cast<float>(held_depth_floor(view.near_m, guard_m));
  g.info[0] = static_cast<uint32_t>(std::max(d.frame_a, 0));
  g.info[1] = static_cast<uint32_t>(std::max(d.frame_b, 0));
  g.info[2] = d.model;
  g.info[3] = (d.lamps ? 1u : 0u) | (d.hide_flame ? 2u : 0u);
  g.mix[0] = d.blend;
  g.mix[1] = d.outdoor;
  g.mix[2] = static_cast<float>(d.white_lux);
  g.mix[3] = d.light.floor;
  const viewmodel::Lighting& l = d.light;
  for (int k = 0; k < 3; ++k) {
    g.ambient[k] = l.ambient[k];
    g.direct[k] = l.direct[k];
    g.toward[k] = l.toward[k];
  }
  g.ambient[3] = l.gloss;
  g.direct[3] = static_cast<float>(l.shininess);
  const int count = std::clamp(l.count, 0, viewmodel::kHeldLights);
  g.toward[3] = static_cast<float>(count);
  for (int i = 0; i < count; ++i) {
    const viewmodel::HeldLight& h = l.lights[i];
    float* o = g.lights[i];
    o[0] = h.at[0];
    o[1] = h.at[1];
    o[2] = h.at[2];
    o[3] = h.range_m;
    o[4] = h.rgb[0];
    o[5] = h.rgb[1];
    o[6] = h.rgb[2];
    o[7] = h.radius_m;
  }
  // The view's basis in the world, as the frame's camera has it: the view matrix's rows are the
  // right, the up and the backward.
  const CameraMatrices cam = camera_matrices(view, aspect);
  for (int i = 0; i < 3; ++i) {
    g.right[i] = cam.view.m[i * 4 + 0];
    g.up[i] = cam.view.m[i * 4 + 1];
    g.forward[i] = -cam.view.m[i * 4 + 2];
  }
  g.right[3] = static_cast<float>(libm::tan(view.fov_y * 0.5) / tan_y);
  return g;
}

bool create_viewmodel(gpu::Device& device, gpu::PipelineSet& pipelines, HeldModels& held,
                      const wgpu::BindGroupLayout& frame,
                      std::span<const viewmodel::HeldModel> models, const HeldSettings& settings) {
  if (models.empty()) return false;
  held.settings = settings;
  // The skins' places, packed as the morph meshes' are (their sizes alone).
  std::vector<MorphModel> sizes(models.size());
  for (size_t m = 0; m < models.size(); ++m) {
    const assets::Image& img = models[m].skin.image;
    if (!img.rgba.empty()) {
      sizes[m].skin.width = static_cast<uint32_t>(img.width);
      sizes[m].skin.height = static_cast<uint32_t>(img.height);
    }
  }
  std::vector<AtlasRect> rects;
  if (!pack_morph_atlas(sizes, rects)) return false;

  std::vector<GpuModel> table(models.size());
  std::vector<float> vertices, attrs;
  std::vector<uint32_t> indices, tri_flags;
  uint32_t atlas_h = 1;
  held.index_counts.clear();
  held.frame_counts.clear();
  for (size_t m = 0; m < models.size(); ++m) {
    const viewmodel::HeldModel& hm = models[m];
    const uint32_t n = hm.vertex_count();
    const bool skinned = !hm.skin.image.rgba.empty() && hm.uv.size() >= size_t{n} * 2;
    table[m].at[0] = static_cast<uint32_t>(vertices.size() / 8);
    table[m].at[1] = n;
    table[m].at[2] = static_cast<uint32_t>(indices.size());
    table[m].at[3] = static_cast<uint32_t>(attrs.size() / 4);
    if (skinned) {
      table[m].rect[0] = rects[m].x;
      table[m].rect[1] = rects[m].y;
      table[m].rect[2] = rects[m].w;
      table[m].rect[3] = rects[m].h;
      atlas_h = std::max(atlas_h, rects[m].y + rects[m].h);
    }
    table[m].more[0] = static_cast<uint32_t>(tri_flags.size());
    for (const viewmodel::HeldFrame& f : hm.frames) {
      for (uint32_t v = 0; v < n; ++v) {
        const float* p = &f.positions[size_t{v} * 3];
        const float* q = &f.normals[size_t{v} * 3];
        vertices.insert(vertices.end(), {p[0], p[1], p[2], 0, q[0], q[1], q[2], 0});
      }
    }
    for (uint32_t v = 0; v < n; ++v) {
      if (skinned) {
        attrs.insert(attrs.end(), {hm.uv[size_t{v} * 2], hm.uv[size_t{v} * 2 + 1], 0, 0});
      } else if (hm.colour.size() >= size_t{v} * 3 + 3) {
        const float* c = &hm.colour[size_t{v} * 3];
        attrs.insert(attrs.end(), {c[0], c[1], c[2], 0});
      } else {
        attrs.insert(attrs.end(), {0.6f, 0.6f, 0.6f, 0});  // the reference's grey
      }
    }
    indices.insert(indices.end(), hm.indices.begin(), hm.indices.end());
    const size_t tris = hm.indices.size() / 3;
    for (size_t t = 0; t < tris; ++t) tri_flags.push_back(t < hm.flame.size() && hm.flame[t]);
    held.index_counts.push_back(static_cast<uint32_t>(tris * 3));
    held.frame_counts.push_back(static_cast<uint32_t>(hm.frames.size()));
  }
  // The atlas: each skin's colour, its glow in alpha.
  std::vector<uint8_t> atlas(size_t{kMorphAtlasSize} * atlas_h * 4, 0);
  for (size_t m = 0; m < models.size(); ++m) {
    if (table[m].rect[2] == 0) continue;
    const viewmodel::Skin& s = models[m].skin;
    const uint32_t w = rects[m].w, h = rects[m].h;
    for (uint32_t y = 0; y < h; ++y) {
      for (uint32_t x = 0; x < w; ++x) {
        const size_t src = size_t{y} * w + x;
        const size_t dst = (size_t{rects[m].y + y} * kMorphAtlasSize + rects[m].x + x) * 4;
        std::memcpy(&atlas[dst], &s.image.rgba[src * 4], 3);
        atlas[dst + 3] = src < s.glow.size() && s.glow[src] != 0 ? 255 : 0;
      }
    }
  }

  auto upload = [&](const char* label, const void* data, size_t bytes) {
    wgpu::Buffer b = make_buffer(device, label, bytes < 16 ? 16 : bytes, BU::Storage);
    if (bytes > 0) device.queue.WriteBuffer(b, 0, data, bytes);
    return b;
  };
  held.models = upload("viewmodel.models", table.data(), table.size() * sizeof(GpuModel));
  held.vertices = upload("viewmodel.vertices", vertices.data(), vertices.size() * sizeof(float));
  held.attrs = upload("viewmodel.attrs", attrs.data(), attrs.size() * sizeof(float));
  held.indices = upload("viewmodel.indices", indices.data(), indices.size() * sizeof(uint32_t));
  held.flags = upload("viewmodel.flags", tri_flags.data(), tri_flags.size() * sizeof(uint32_t));
  held.draws =
      make_buffer(device, "viewmodel.draws", kMaxHeldDraws * sizeof(GpuHeldDraw), BU::Storage);
  held.atlas = make_texture(device, "viewmodel.atlas", kMorphAtlasSize, atlas_h, 1,
                            wgpu::TextureFormat::RGBA8Unorm,
                            wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst);
  wgpu::TexelCopyTextureInfo dst{};
  dst.texture = held.atlas;
  wgpu::TexelCopyBufferLayout layout{};
  layout.bytesPerRow = kMorphAtlasSize * 4;
  layout.rowsPerImage = atlas_h;
  wgpu::Extent3D size{kMorphAtlasSize, atlas_h, 1};
  device.queue.WriteTexture(&dst, atlas.data(), atlas.size(), &layout, &size);
  held.staged.assign(kMaxHeldDraws, GpuHeldDraw{});
  held.staged_models.assign(kMaxHeldDraws, 0);

  const auto vf = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
  held.layout =
      make_layout(device, "viewmodel",
                  {Slot::storage_read, Slot::storage_read, Slot::storage_read, Slot::storage_read,
                   Slot::storage_read, Slot::storage_read, Slot::texture},
                  vf);
  held.group = make_group(device, "viewmodel", held.layout,
                          {held.models, held.vertices, held.attrs, held.indices, held.flags,
                           held.draws, held.atlas.CreateView()});
  held.shadows = no_sun_shadows(device);
  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::viewmodel, "viewmodel");
  const wgpu::PipelineLayout pipeline_layout =
      make_pipeline_layout(device, "viewmodel", {frame, held.layout, sun_shadow_layout(device)});

  auto build = [&](const char* label, const char* entry, std::span<wgpu::ColorTargetState> out) {
    wgpu::FragmentState fragment{};
    fragment.module = module;
    fragment.entryPoint = entry;
    fragment.targetCount = out.size();
    fragment.targets = out.data();
    wgpu::DepthStencilState depth{};
    depth.format = kDepthFormat;
    depth.depthWriteEnabled = true;
    depth.depthCompare = wgpu::CompareFunction::Greater;  // reversed z
    wgpu::RenderPipelineDescriptor desc{};
    desc.label = label;
    desc.layout = pipeline_layout;
    desc.vertex.module = module;
    desc.vertex.entryPoint = "vs_main";
    desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    desc.primitive.frontFace = wgpu::FrontFace::CW;
    desc.primitive.cullMode = wgpu::CullMode::Back;
    desc.depthStencil = &depth;
    desc.fragment = &fragment;
    return pipelines.add_render(device, desc);
  };
  wgpu::ColorTargetState hdr[2]{};
  hdr[0].format = kHdrFormat;
  hdr[1].format = kMotionFormat;
  held.p_draw = build("viewmodel.draw", "fs_main", hdr);
  held.has_resolved = settings.resolved;
  if (settings.resolved) {
    wgpu::ColorTargetState bytes[1]{};
    bytes[0].format = wgpu::TextureFormat::RGBA8Unorm;
    held.p_resolved = build("viewmodel.resolved", "fs_resolved", bytes);
  }
  return true;
}

void bind_viewmodel_shadows(HeldModels& held, const wgpu::BindGroup& shadows) {
  held.shadows = shadows;
}

void prepare_viewmodel(gpu::Device& device, HeldModels& held, std::span<const HeldDraw> draws,
                       const FrameView& view, uint32_t width, uint32_t height) {
  held.drawn = 0;
  held.dropped = 0;
  held.depth_floor = static_cast<float>(held_depth_floor(view.near_m, held.settings.guard_m));
  for (const HeldDraw& d : draws) {
    if (d.model >= held.index_counts.size()) continue;
    if (held.drawn == kMaxHeldDraws) {
      ++held.dropped;
      continue;
    }
    HeldDraw clamped = d;
    const int last = static_cast<int>(held.frame_counts[d.model]) - 1;
    clamped.frame_a = std::clamp(d.frame_a, 0, last);
    clamped.frame_b = std::clamp(d.frame_b, 0, last);
    held.staged[held.drawn] = pack_held_draw(clamped, view, width, height, held.settings.guard_m);
    held.staged_models[held.drawn] = d.model;
    ++held.drawn;
  }
  if (held.drawn > 0) {
    device.queue.WriteBuffer(held.draws, 0, held.staged.data(), held.drawn * sizeof(GpuHeldDraw));
  }
}

namespace {

void draw_all(const HeldModels& held, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
              wgpu::RenderPassEncoder& pass, uint32_t pipeline, const wgpu::BindGroup& frame) {
  pass.SetPipeline(pipelines.render(pipeline));
  pass.SetBindGroup(0, frame);
  pass.SetBindGroup(1, held.group);
  pass.SetBindGroup(2, held.shadows);
  for (uint32_t k = 0; k < held.drawn; ++k) {
    pass.Draw(held.index_counts[held.staged_models[k]], 1, 0, k);
    ++rec.current().draws;
  }
}

}  // namespace

void encode_viewmodel(const HeldModels& held, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
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
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "viewmodel");
  if (held.drawn > 0) {
    // The gun's own depth range: in front of the world past the guard.
    pass.SetViewport(0, 0, static_cast<float>(targets.width), static_cast<float>(targets.height),
                     held.depth_floor, 1);
    draw_all(held, pipelines, rec, pass, held.p_draw, frame);
  }
  pass.End();
}

void encode_viewmodel_resolved(const HeldModels& held, const gpu::PipelineSet& pipelines,
                               FrameRecorder& rec, wgpu::CommandEncoder& encoder,
                               const wgpu::TextureView& colour, const wgpu::TextureView& depth,
                               uint32_t width, uint32_t height, const wgpu::BindGroup& frame) {
  wgpu::RenderPassColorAttachment c{};
  c.view = colour;
  c.loadOp = wgpu::LoadOp::Clear;
  c.storeOp = wgpu::StoreOp::Store;
  c.clearValue = {0, 0, 0, 0};
  wgpu::RenderPassDepthStencilAttachment z{};
  z.view = depth;
  z.depthLoadOp = wgpu::LoadOp::Clear;
  z.depthStoreOp = wgpu::StoreOp::Store;
  z.depthClearValue = 0;
  wgpu::RenderPassDescriptor desc{};
  desc.colorAttachmentCount = 1;
  desc.colorAttachments = &c;
  desc.depthStencilAttachment = &z;
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "viewmodel.resolved");
  if (held.has_resolved && held.drawn > 0) {
    pass.SetViewport(0, 0, static_cast<float>(width), static_cast<float>(height), held.depth_floor,
                     1);
    draw_all(held, pipelines, rec, pass, held.p_resolved, frame);
  }
  pass.End();
}

}  // namespace ge::render
