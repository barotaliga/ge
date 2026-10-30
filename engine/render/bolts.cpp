// The lightning's channel: its buffers, its pipeline, the frame's upload and the pass.
//
// ge:summary The lightning channel's pass definitions: buffers, pipeline, upload, draw
// ge:keywords render lightning bolts channel ribbon prepare_bolts encode_bolts
// ge:skill render
// ge:test tests/render_bolts_test.cpp
#include "render/bolts.hpp"

#include <algorithm>

#include "render/bolts.wgsl.hpp"
#include "render/night.hpp"

namespace ge::render {

namespace {

using BU = wgpu::BufferUsage;

// bolts.wgsl's BoltParams.
struct Params {
  float info[4];  // the segments, a pixel's angle (rad), the light's scale (pre-exposed over lux)
};

}  // namespace

void create_bolts(gpu::Device& device, gpu::PipelineSet& pipelines, Bolts& bolts,
                  const wgpu::BindGroupLayout& frame, uint32_t capacity) {
  bolts.capacity = std::max(capacity, 1u);
  bolts.segments = make_buffer(device, "bolts.segments",
                               uint64_t{bolts.capacity} * sizeof(BoltSegment), BU::Storage);
  bolts.params = make_buffer(device, "bolts.params", sizeof(Params), BU::Uniform);
  const auto vf = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
  bolts.layout = make_layout(device, "bolts", {Slot::uniform, Slot::storage_read}, vf);
  bolts.group = make_group(device, "bolts", bolts.layout, {bolts.params, bolts.segments});

  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::bolts, "bolts");
  // Added to the picture; into the motion target, the reactive channel's largest.
  wgpu::BlendState add{};
  add.color = {wgpu::BlendOperation::Add, wgpu::BlendFactor::One, wgpu::BlendFactor::One};
  add.alpha = add.color;
  wgpu::BlendState most{};
  most.color = {wgpu::BlendOperation::Max, wgpu::BlendFactor::One, wgpu::BlendFactor::One};
  most.alpha = most.color;
  wgpu::ColorTargetState colours[2]{};
  colours[0].format = kHdrFormat;
  colours[0].blend = &add;
  colours[1].format = kMotionFormat;
  colours[1].blend = &most;
  colours[1].writeMask = wgpu::ColorWriteMask::Blue;
  wgpu::FragmentState fragment{};
  fragment.module = module;
  fragment.entryPoint = "fs_main";
  fragment.targetCount = 2;
  fragment.targets = colours;
  wgpu::DepthStencilState depth{};
  depth.format = kDepthFormat;
  depth.depthWriteEnabled = false;
  depth.depthCompare = wgpu::CompareFunction::Greater;  // reversed-z: nearer is greater
  wgpu::RenderPipelineDescriptor desc{};
  desc.label = "bolts.draw";
  desc.layout = make_pipeline_layout(device, "bolts", {frame, bolts.layout});
  desc.vertex.module = module;
  desc.vertex.entryPoint = "vs_main";
  desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  desc.depthStencil = &depth;
  desc.fragment = &fragment;
  bolts.p_draw = pipelines.add_render(device, desc);
}

void prepare_bolts(gpu::Device& device, Bolts& bolts, std::span<const BoltSegment> segments,
                   const FrameView& view, uint32_t render_height) {
  bolts.count = static_cast<uint32_t>(std::min<size_t>(segments.size(), bolts.capacity));
  if (bolts.count > 0) {
    device.queue.WriteBuffer(bolts.segments, 0, segments.data(),
                             uint64_t{bolts.count} * sizeof(BoltSegment));
  }
  Params p{};
  p.info[0] = static_cast<float>(bolts.count);
  p.info[1] = static_cast<float>(view.fov_y / std::max(render_height, 1u));
  p.info[2] = static_cast<float>(pre_exposure(view) / kSunLux);
  device.queue.WriteBuffer(bolts.params, 0, &p, sizeof p);
}

void encode_bolts(const Bolts& bolts, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
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
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "bolts");
  if (bolts.count > 0) {
    pass.SetPipeline(pipelines.render(bolts.p_draw));
    pass.SetBindGroup(0, frame);
    pass.SetBindGroup(1, bolts.group);
    pass.Draw(6, bolts.count);
    ++rec.current().draws;
  }
  pass.End();
}

}  // namespace ge::render
