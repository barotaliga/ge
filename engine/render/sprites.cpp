// Sprites drawn: the frames' upload (the atlas), each draw turned by its facing rule, the cutouts
// staged first and the blended ones after them far to near, and the two pipelines.
//
// ge:summary Sprites: the atlas, the facing rules, the staged and sorted draws, the pass
// ge:keywords render sprites upload atlas shelf-pack facing billboard sort cutout blend
// ge:keywords sprite_basis pack_sprite_atlas
// ge:skill render
// ge:test tests/render_sprites_test.cpp
#include "render/sprites.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>

#include "core/libm/libm.hpp"
#include "render/sprites.wgsl.hpp"

namespace ge::render {

namespace {

using BU = wgpu::BufferUsage;

// sprites.wgsl's Sprite.
struct GpuSprite {
  float centre[4];
  float last[4];
  float right[4];
  float up[4];
  float extent[4];
  float colour[4];
  float light[4];
  float ambient[4];
  uint32_t info[4];
};
static_assert(sizeof(GpuSprite) == 144, "match sprites.wgsl's Sprite");

void normalise(double v[3]) {
  const double l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (l < 1e-12) return;
  for (int k = 0; k < 3; ++k) v[k] /= l;
}

// The world's right of a horizontal direction (the engine's right = up x forward).
bool right_of(const double f[3], double r[3]) {
  r[0] = f[2];
  r[1] = 0;
  r[2] = -f[0];
  const double l = std::sqrt(r[0] * r[0] + r[2] * r[2]);
  if (l < 1e-9) return false;
  r[0] /= l;
  r[2] /= l;
  return true;
}

void rotate(const float q[4], const double v[3], double out[3]) {
  // v + 2w (q x v) + 2 q x (q x v)
  const double x = q[0], y = q[1], z = q[2], w = q[3];
  const double t[3] = {2 * (y * v[2] - z * v[1]), 2 * (z * v[0] - x * v[2]),
                       2 * (x * v[1] - y * v[0])};
  out[0] = v[0] + w * t[0] + (y * t[2] - z * t[1]);
  out[1] = v[1] + w * t[1] + (z * t[0] - x * t[2]);
  out[2] = v[2] + w * t[2] + (x * t[1] - y * t[0]);
}

}  // namespace

bool pack_sprite_atlas(std::span<const SpriteFrame> frames, std::vector<SpriteRect>& rects) {
  rects.assign(frames.size(), SpriteRect{});
  std::vector<size_t> order(frames.size());
  std::iota(order.begin(), order.end(), size_t{0});
  std::stable_sort(order.begin(), order.end(),
                   [&](size_t a, size_t b) { return frames[a].height > frames[b].height; });
  uint32_t x = 0, y = 0, shelf = 0;
  for (size_t i : order) {
    const SpriteFrame& f = frames[i];
    const uint32_t w = f.width < 1 ? 1 : f.width, h = f.height < 1 ? 1 : f.height;
    if (w > kSpriteAtlasSize) return false;
    if (x + w > kSpriteAtlasSize) {
      x = 0;
      y += shelf + 1;
      shelf = 0;
    }
    if (y + h > kSpriteAtlasSize) return false;
    rects[i] = {x, y, w, h};
    x += w + 1;
    shelf = h > shelf ? h : shelf;
  }
  return true;
}

void sprite_basis(const SpriteDraw& d, const double eye[3], const double cam_right[3],
                  const double cam_up[3], const double cam_fwd[3], double right[3], double up[3]) {
  const double world_up[3] = {0, 1, 0};
  switch (d.facing) {
    case SpriteFacing::parallel:
      std::memcpy(right, cam_right, 3 * sizeof(double));
      std::memcpy(up, cam_up, 3 * sizeof(double));
      return;
    case SpriteFacing::upright: {
      const double to[3] = {d.position[0] - eye[0], 0, d.position[2] - eye[2]};
      if (!right_of(to, right)) right_of(cam_fwd, right);
      std::memcpy(up, world_up, sizeof world_up);
      return;
    }
    case SpriteFacing::parallel_upright:
      if (!right_of(cam_fwd, right)) std::memcpy(right, cam_right, 3 * sizeof(double));
      std::memcpy(up, world_up, sizeof world_up);
      return;
    case SpriteFacing::oriented: {
      const double x[3] = {1, 0, 0}, y[3] = {0, 1, 0};
      rotate(d.rotation, x, right);
      rotate(d.rotation, y, up);
      normalise(right);
      normalise(up);
      return;
    }
    case SpriteFacing::parallel_oriented: {
      // Right side down: the right swings toward -up, the up toward +right.
      const double c = core::libm::cos(d.roll), s = core::libm::sin(d.roll);
      for (int k = 0; k < 3; ++k) {
        right[k] = cam_right[k] * c - cam_up[k] * s;
        up[k] = cam_up[k] * c + cam_right[k] * s;
      }
      return;
    }
  }
}

bool create_sprites(gpu::Device& device, gpu::PipelineSet& pipelines, Sprites& sprites,
                    const wgpu::BindGroupLayout& frame, std::span<const SpriteFrame> frames) {
  if (frames.empty() || !pack_sprite_atlas(frames, sprites.rects)) return false;
  uint32_t atlas_h = 1;
  std::vector<uint32_t> table;
  table.reserve(frames.size() * 4);
  sprites.sizes.clear();
  for (size_t i = 0; i < frames.size(); ++i) {
    const SpriteRect& r = sprites.rects[i];
    table.insert(table.end(), {r.x, r.y, r.w, r.h});
    atlas_h = std::max(atlas_h, r.y + r.h);
    SpriteFrame size;
    size.width = r.w;
    size.height = r.h;
    size.origin[0] = frames[i].origin[0];
    size.origin[1] = frames[i].origin[1];
    sprites.sizes.push_back(size);
  }
  std::vector<uint8_t> atlas(size_t{kSpriteAtlasSize} * atlas_h * 4, 0);
  for (size_t i = 0; i < frames.size(); ++i) {
    const SpriteFrame& f = frames[i];
    const SpriteRect& r = sprites.rects[i];
    if (f.rgba.size() < size_t{f.width} * f.height * 4) continue;
    for (uint32_t y = 0; y < f.height; ++y) {
      std::memcpy(&atlas[(size_t{r.y + y} * kSpriteAtlasSize + r.x) * 4],
                  &f.rgba[size_t{y} * f.width * 4], size_t{f.width} * 4);
    }
  }
  sprites.frames =
      make_buffer(device, "sprites.frames", table.size() * sizeof(uint32_t), BU::Storage);
  device.queue.WriteBuffer(sprites.frames, 0, table.data(), table.size() * sizeof(uint32_t));
  sprites.draws =
      make_buffer(device, "sprites.draws", kMaxSprites * sizeof(GpuSprite), BU::Storage);
  sprites.atlas = make_texture(device, "sprites.atlas", kSpriteAtlasSize, atlas_h, 1,
                               wgpu::TextureFormat::RGBA8Unorm,
                               wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst);
  wgpu::TexelCopyTextureInfo dst{};
  dst.texture = sprites.atlas;
  wgpu::TexelCopyBufferLayout layout{};
  layout.bytesPerRow = kSpriteAtlasSize * 4;
  layout.rowsPerImage = atlas_h;
  wgpu::Extent3D size{kSpriteAtlasSize, atlas_h, 1};
  device.queue.WriteTexture(&dst, atlas.data(), atlas.size(), &layout, &size);
  sprites.staged.assign(kMaxSprites * sizeof(GpuSprite), 0);

  const auto vf = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
  sprites.layout =
      make_layout(device, "sprites", {Slot::storage_read, Slot::storage_read, Slot::texture}, vf);
  sprites.group = make_group(device, "sprites", sprites.layout,
                             {sprites.frames, sprites.draws, sprites.atlas.CreateView()});
  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::sprites, "sprites");
  const wgpu::PipelineLayout pl = make_pipeline_layout(device, "sprites", {frame, sprites.layout});

  // Cutouts: opaque where kept, depth and motion written.
  {
    wgpu::ColorTargetState colours[2]{};
    colours[0].format = kHdrFormat;
    colours[1].format = kMotionFormat;
    wgpu::FragmentState fragment{};
    fragment.module = module;
    fragment.entryPoint = "fs_cutout";
    fragment.targetCount = 2;
    fragment.targets = colours;
    wgpu::DepthStencilState depth{};
    depth.format = kDepthFormat;
    depth.depthWriteEnabled = true;
    depth.depthCompare = wgpu::CompareFunction::Greater;  // reversed z
    wgpu::RenderPipelineDescriptor desc{};
    desc.label = "sprites.cutout";
    desc.layout = pl;
    desc.vertex.module = module;
    desc.vertex.entryPoint = "vs_main";
    desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    desc.primitive.cullMode = wgpu::CullMode::None;
    desc.depthStencil = &depth;
    desc.fragment = &fragment;
    sprites.p_cutout = pipelines.add_render(device, desc);
  }
  // Blended: premultiplied over the picture (an added sprite's alpha is 0); into the motion
  // target, the reactive channel's largest.
  {
    wgpu::BlendState over{};
    over.color = {wgpu::BlendOperation::Add, wgpu::BlendFactor::One,
                  wgpu::BlendFactor::OneMinusSrcAlpha};
    over.alpha = over.color;
    wgpu::BlendState most{};
    most.color = {wgpu::BlendOperation::Max, wgpu::BlendFactor::One, wgpu::BlendFactor::One};
    most.alpha = most.color;
    wgpu::ColorTargetState colours[2]{};
    colours[0].format = kHdrFormat;
    colours[0].blend = &over;
    colours[1].format = kMotionFormat;
    colours[1].blend = &most;
    colours[1].writeMask = wgpu::ColorWriteMask::Blue;
    wgpu::FragmentState fragment{};
    fragment.module = module;
    fragment.entryPoint = "fs_blend";
    fragment.targetCount = 2;
    fragment.targets = colours;
    wgpu::DepthStencilState depth{};
    depth.format = kDepthFormat;
    depth.depthWriteEnabled = false;
    depth.depthCompare = wgpu::CompareFunction::Greater;
    wgpu::RenderPipelineDescriptor desc{};
    desc.label = "sprites.blend";
    desc.layout = pl;
    desc.vertex.module = module;
    desc.vertex.entryPoint = "vs_main";
    desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    desc.primitive.cullMode = wgpu::CullMode::None;
    desc.depthStencil = &depth;
    desc.fragment = &fragment;
    sprites.p_blend = pipelines.add_render(device, desc);
  }
  return true;
}

void prepare_sprites(gpu::Device& device, Sprites& sprites, std::span<const SpriteDraw> draws,
                     const FrameView& view) {
  sprites.cutouts = 0;
  sprites.blended = 0;
  sprites.dropped = 0;
  const CameraMatrices m = camera_matrices(view, 1.0);
  double cam_right[3], cam_up[3], cam_fwd[3];
  for (int i = 0; i < 3; ++i) {
    cam_right[i] = m.view.m[i * 4 + 0];
    cam_up[i] = m.view.m[i * 4 + 1];
    cam_fwd[i] = -m.view.m[i * 4 + 2];
  }
  auto stage = [&](const SpriteDraw& d, uint32_t slot) {
    const SpriteFrame& f = sprites.sizes[d.frame];
    double right[3], up[3];
    sprite_basis(d, view.eye, cam_right, cam_up, cam_fwd, right, up);
    GpuSprite g{};
    for (int k = 0; k < 3; ++k) {
      g.centre[k] = static_cast<float>(d.position[k] - view.eye[k]);
      g.last[k] = static_cast<float>(d.last[k] - view.eye[k]);
      g.right[k] = static_cast<float>(right[k]);
      g.up[k] = static_cast<float>(up[k]);
      g.ambient[k] = d.ambient[k];
    }
    g.extent[0] = -f.origin[0] * d.scale;
    g.extent[1] = (static_cast<float>(f.width) - f.origin[0]) * d.scale;
    g.extent[2] = -(static_cast<float>(f.height) - f.origin[1]) * d.scale;
    g.extent[3] = f.origin[1] * d.scale;
    std::memcpy(g.colour, d.colour, sizeof g.colour);
    g.light[0] = d.glow;
    g.light[1] = d.outdoor;
    g.info[0] = d.frame;
    g.info[1] = static_cast<uint32_t>(d.blend);
    g.info[2] = d.smooth ? 1 : 0;
    std::memcpy(&sprites.staged[size_t{slot} * sizeof(GpuSprite)], &g, sizeof g);
  };
  // The cutouts first, then the blended far to near.
  sprites.order.clear();
  sprites.depth.clear();
  uint32_t n = 0;
  for (size_t i = 0; i < draws.size(); ++i) {
    const SpriteDraw& d = draws[i];
    if (d.frame >= sprites.sizes.size()) continue;
    if (n == kMaxSprites) {
      ++sprites.dropped;
      continue;
    }
    ++n;
    if (d.blend == SpriteBlend::cutout) {
      stage(d, sprites.cutouts++);
    } else {
      double dist = 0;
      for (int k = 0; k < 3; ++k) dist += (d.position[k] - view.eye[k]) * cam_fwd[k];
      sprites.order.push_back(static_cast<uint32_t>(i));
      sprites.depth.push_back(static_cast<float>(dist));
    }
  }
  std::vector<uint32_t>& idx = sprites.sorted;
  idx.resize(sprites.order.size());
  std::iota(idx.begin(), idx.end(), 0u);
  // Far to near, ties in the game's order (std::sort allocates nothing; stable_sort may).
  std::sort(idx.begin(), idx.end(), [&](uint32_t a, uint32_t b) {
    const float da = sprites.depth[a], db = sprites.depth[b];
    return da > db || (da == db && a < b);
  });
  for (uint32_t k : idx) stage(draws[sprites.order[k]], sprites.cutouts + sprites.blended++);
  const uint32_t total = sprites.cutouts + sprites.blended;
  if (total > 0) {
    device.queue.WriteBuffer(sprites.draws, 0, sprites.staged.data(),
                             size_t{total} * sizeof(GpuSprite));
  }
}

void encode_sprites(const Sprites& sprites, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
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
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "sprites");
  if (sprites.cutouts + sprites.blended > 0) {
    pass.SetBindGroup(0, frame);
    pass.SetBindGroup(1, sprites.group);
    if (sprites.cutouts > 0) {
      pass.SetPipeline(pipelines.render(sprites.p_cutout));
      pass.Draw(6, sprites.cutouts, 0, 0);
      ++rec.current().draws;
    }
    if (sprites.blended > 0) {
      pass.SetPipeline(pipelines.render(sprites.p_blend));
      pass.Draw(6, sprites.blended, 0, sprites.cutouts);
      ++rec.current().draws;
    }
  }
  pass.End();
}

}  // namespace ge::render
