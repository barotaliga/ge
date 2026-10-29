// The beams' pass: its buffers, atlas and pipelines, the staging of channels and burns, the draw.
//
// ge:summary The beams' pass: buffers, atlas, pipelines, staging, the burns and beams drawn
// ge:keywords render beams pass staging pipelines atlas burn ribbon additive
// ge:skill render
// ge:test tests/render_beam_test.cpp
#include "render/beams.hpp"

#include <algorithm>
#include <cmath>

#include "render/beams.wgsl.hpp"

namespace ge::render {

namespace {

using BU = wgpu::BufferUsage;

// A soft round mark, for a pass given no atlas: full in its middle, gone at its rim.
std::vector<uint8_t> round_mark(uint32_t n) {
  std::vector<uint8_t> px(size_t{n} * n * 4, 0);
  for (uint32_t y = 0; y < n; ++y) {
    for (uint32_t x = 0; x < n; ++x) {
      const double dx = (x + 0.5) / n * 2 - 1, dy = (y + 0.5) / n * 2 - 1;
      const double r = std::sqrt(dx * dx + dy * dy);
      const double c = std::clamp((1 - r) * 2.5, 0.0, 1.0);
      uint8_t* p = &px[(size_t{y} * n + x) * 4];
      p[0] = p[3] = static_cast<uint8_t>(std::lround(c * 255));
      p[1] = 128;
    }
  }
  return px;
}

}  // namespace

void create_beams(gpu::Device& device, gpu::PipelineSet& pipelines, Beams& beams,
                  const wgpu::BindGroupLayout& frame, const BeamSettings& settings) {
  beams.settings = settings;
  beams.settings.max_segments = std::max(settings.max_segments, 1u);
  beams.settings.max_burn_verts = std::max(settings.max_burn_verts, 3u);
  beams.seg_staging.assign(beams.settings.max_segments, GpuBeamSeg{});
  beams.burn_staging.assign(beams.settings.max_burn_verts, GpuBurnVert{});
  beams.params = make_buffer(device, "beams.params", sizeof(GpuBeamParams), BU::Uniform);
  beams.segs = make_buffer(device, "beams.segs",
                           uint64_t{beams.settings.max_segments} * sizeof(GpuBeamSeg), BU::Storage);
  beams.burns =
      make_buffer(device, "beams.burns",
                  uint64_t{beams.settings.max_burn_verts} * sizeof(GpuBurnVert), BU::Storage);

  // The atlas: the game's, or a round mark.
  std::vector<uint8_t> fallback;
  const std::vector<uint8_t>* px = settings.atlas;
  uint32_t aw = settings.atlas_width, ah = settings.atlas_height;
  if (px == nullptr || px->size() < size_t{aw} * ah * 4 || aw == 0 || ah == 0) {
    aw = ah = 64;
    fallback = round_mark(aw);
    px = &fallback;
  }
  beams.atlas = make_texture(device, "beams.atlas", aw, ah, 1, wgpu::TextureFormat::RGBA8Unorm,
                             wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst);
  wgpu::TexelCopyTextureInfo dst{};
  dst.texture = beams.atlas;
  wgpu::TexelCopyBufferLayout copy{};
  copy.bytesPerRow = aw * 4;
  copy.rowsPerImage = ah;
  const wgpu::Extent3D size{aw, ah, 1};
  device.queue.WriteTexture(&dst, px->data(), size_t{aw} * ah * 4, &copy, &size);
  beams.atlas_view = beams.atlas.CreateView();

  const auto vf = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
  beams.layout = make_layout(
      device, "beams",
      {Slot::uniform, Slot::storage_read, Slot::storage_read, Slot::depth, Slot::texture}, vf);
  const wgpu::PipelineLayout pl = make_pipeline_layout(device, "beams", {frame, beams.layout});
  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::beams, "beams");

  wgpu::DepthStencilState depth{};
  depth.format = kDepthFormat;
  depth.depthWriteEnabled = false;
  depth.depthCompare = wgpu::CompareFunction::GreaterEqual;  // reversed-z: nearer is greater

  // THE BURNS: the ember added, the picture under it kept by the soot's alpha.
  wgpu::BlendState burn{};
  burn.color = {wgpu::BlendOperation::Add, wgpu::BlendFactor::One, wgpu::BlendFactor::SrcAlpha};
  burn.alpha = {wgpu::BlendOperation::Add, wgpu::BlendFactor::Zero, wgpu::BlendFactor::One};
  wgpu::ColorTargetState burn_targets[2]{};
  burn_targets[0].format = kHdrFormat;
  burn_targets[0].blend = &burn;
  burn_targets[1].format = kMotionFormat;
  burn_targets[1].writeMask = wgpu::ColorWriteMask::None;
  wgpu::FragmentState burn_fs{};
  burn_fs.module = module;
  burn_fs.entryPoint = "burn_fs";
  burn_fs.targetCount = 2;
  burn_fs.targets = burn_targets;
  wgpu::RenderPipelineDescriptor bd{};
  bd.label = "beams.burns";
  bd.layout = pl;
  bd.vertex.module = module;
  bd.vertex.entryPoint = "burn_vs";
  bd.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  bd.depthStencil = &depth;
  bd.fragment = &burn_fs;
  beams.p_burn = pipelines.add_render(device, bd);

  // THE BEAMS: added; the reactive mask's largest.
  wgpu::BlendState add{};
  add.color = {wgpu::BlendOperation::Add, wgpu::BlendFactor::One, wgpu::BlendFactor::One};
  add.alpha = {wgpu::BlendOperation::Add, wgpu::BlendFactor::Zero, wgpu::BlendFactor::One};
  wgpu::BlendState most{};
  most.color = {wgpu::BlendOperation::Max, wgpu::BlendFactor::One, wgpu::BlendFactor::One};
  most.alpha = most.color;
  wgpu::ColorTargetState beam_targets[2]{};
  beam_targets[0].format = kHdrFormat;
  beam_targets[0].blend = &add;
  beam_targets[1].format = kMotionFormat;
  beam_targets[1].blend = &most;
  beam_targets[1].writeMask = wgpu::ColorWriteMask::Blue;
  wgpu::FragmentState beam_fs{};
  beam_fs.module = module;
  beam_fs.entryPoint = "beam_fs";
  beam_fs.targetCount = 2;
  beam_fs.targets = beam_targets;
  wgpu::DepthStencilState beam_depth = depth;
  beam_depth.depthCompare = wgpu::CompareFunction::Greater;
  wgpu::RenderPipelineDescriptor rd{};
  rd.label = "beams.ribbons";
  rd.layout = pl;
  rd.vertex.module = module;
  rd.vertex.entryPoint = "beam_vs";
  rd.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  rd.depthStencil = &beam_depth;
  rd.fragment = &beam_fs;
  beams.p_beam = pipelines.add_render(device, rd);
}

void bind_beam_targets(gpu::Device& device, Beams& beams, const FrameTargets& targets) {
  beams.group =
      make_group(device, "beams", beams.layout,
                 {beams.params, beams.segs, beams.burns, targets.depth_view, beams.atlas_view});
}

void stage_beams(Beams& beams, std::span<const BeamDraw> draws, std::span<const BurnVertex> burns,
                 const FrameView& view, float hot_ember_cd) {
  GpuBeamParams& p = beams.staged;
  p = GpuBeamParams{};
  beams.dropped = 0;
  uint32_t nseg = 0, nbeam = 0;
  for (const BeamDraw& d : draws) {
    if (d.shape == nullptr || d.look == nullptr || d.shape->count == 0) continue;
    if (nbeam >= kMaxBeamDraws) {
      beams.dropped += static_cast<uint32_t>(d.shape->count);
      continue;
    }
    const BeamLook& l = *d.look;
    const float level = d.shape->level;
    GpuBeamLook& g = p.looks[nbeam];
    for (int k = 0; k < 3; ++k) {
      g.core[k] = l.core_rgb[k];
      g.glow[k] = l.glow_rgb[k];
    }
    g.core[3] = l.core_cd * level;
    g.glow[3] = l.glow_cd * level;
    g.shape[0] = l.core_m;
    g.shape[1] = l.glow_m;
    g.shape[2] = l.shimmer;
    g.shape[3] = static_cast<float>(d.seed % 997u) / 997.0f;
    g.more[0] = std::min(l.glow_m * 0.5f, std::max(0.02f, l.step_m * 0.45f));
    g.more[1] = 0.12f;
    float rel[3];
    for (int k = 0; k < 3; ++k) rel[k] = static_cast<float>(d.shape->origin[k] - view.eye[k]);
    for (int i = 0; i < d.shape->count; ++i) {
      if (nseg >= beams.settings.max_segments) {
        ++beams.dropped;
        continue;
      }
      const BeamSegment& s = d.shape->seg[i];
      GpuBeamSeg& o = beams.seg_staging[nseg++];
      for (int k = 0; k < 3; ++k) {
        o.a[k] = rel[k] + s.a[k];
        o.b[k] = rel[k] + s.b[k];
      }
      o.a[3] = s.width_a;
      o.b[3] = s.width_b;
      o.c[0] = s.glow;
      o.c[1] = s.s_a;
      o.c[2] = s.s_b;
      o.c[3] = static_cast<float>(nbeam);
    }
    ++nbeam;
  }
  const uint32_t nburn = std::min<uint32_t>(static_cast<uint32_t>(burns.size() / 3 * 3),
                                            beams.settings.max_burn_verts / 3 * 3);
  beams.dropped += static_cast<uint32_t>(burns.size()) - nburn;
  for (uint32_t i = 0; i < nburn; ++i) {
    const BurnVertex& b = burns[i];
    GpuBurnVert& o = beams.burn_staging[i];
    for (int k = 0; k < 3; ++k) o.at[k] = static_cast<float>(b.at[k] - view.eye[k]);
    o.at[3] = b.fade;
    o.uv[0] = b.u;
    o.uv[1] = b.v;
    o.uv[2] = b.soot;
    o.uv[3] = b.ember;
  }
  beams.seg_count = nseg;
  beams.burn_count = nburn;
  p.info[0] = static_cast<float>(nbeam);
  p.info[1] = static_cast<float>(nseg);
  p.info[2] = static_cast<float>(nburn);
  // The shader's clock: the shimmer's phase, wrapped so a float keeps its fraction.
  p.info[3] = static_cast<float>(std::fmod(view.time_s, 1000.0));
  p.more[0] = static_cast<float>(view.near_m);
  p.more[1] = hot_ember_cd;
}

void prepare_beams(gpu::Device& device, Beams& beams, std::span<const BeamDraw> draws,
                   std::span<const BurnVertex> burns, const FrameView& view, float hot_ember_cd) {
  stage_beams(beams, draws, burns, view, hot_ember_cd);
  device.queue.WriteBuffer(beams.params, 0, &beams.staged, sizeof beams.staged);
  if (beams.seg_count > 0) {
    device.queue.WriteBuffer(beams.segs, 0, beams.seg_staging.data(),
                             uint64_t{beams.seg_count} * sizeof(GpuBeamSeg));
  }
  if (beams.burn_count > 0) {
    device.queue.WriteBuffer(beams.burns, 0, beams.burn_staging.data(),
                             uint64_t{beams.burn_count} * sizeof(GpuBurnVert));
  }
}

void encode_beams(const Beams& beams, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
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
  depth.depthReadOnly = true;
  wgpu::RenderPassDescriptor desc{};
  desc.colorAttachmentCount = 2;
  desc.colorAttachments = colours;
  desc.depthStencilAttachment = &depth;
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "beams");
  pass.SetBindGroup(0, frame);
  pass.SetBindGroup(1, beams.group);
  // The pass runs every frame (its row in the table), drawing only what this frame has.
  if (beams.burn_count > 0) {
    pass.SetPipeline(pipelines.render(beams.p_burn));
    pass.Draw(beams.burn_count);
    ++rec.current().draws;
  }
  if (beams.seg_count > 0) {
    pass.SetPipeline(pipelines.render(beams.p_beam));
    pass.Draw(6, beams.seg_count);
    ++rec.current().draws;
  }
  pass.End();
}

}  // namespace ge::render
