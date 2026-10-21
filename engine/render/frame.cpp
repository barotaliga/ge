// The frame's uniforms filled from a FrameView, its targets made, the pass table checked, and
// the layout, group and buffer helpers.
//
// ge:summary Frame uniforms, targets, the pass table's check and the binding helpers
// ge:keywords render uniforms targets pass-table bind-group layout buffer
// ge:skill render
// ge:test tests/render_frame_test.cpp
#include "render/frame.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace ge::render {

void fill_uniforms(FrameUniforms& u, const FrameView& view, uint32_t width, uint32_t height) {
  std::memset(&u, 0, sizeof u);
  const CameraMatrices cam =
      camera_matrices(view, static_cast<double>(width) / static_cast<double>(height));
  std::memcpy(u.view_proj, cam.view_proj.m, sizeof u.view_proj);
  std::memcpy(u.inv_view_proj, cam.inv_view_proj.m, sizeof u.inv_view_proj);
  for (int i = 0; i < 3; ++i) u.eye[i] = static_cast<float>(view.eye[i]);
  u.eye[3] = static_cast<float>((view.altitude_m + view.eye[1]) * 1e-3);
  double s[3];
  sun_direction(view, s);
  for (int i = 0; i < 3; ++i) u.sun[i] = static_cast<float>(s[i]);
  u.sun[3] = static_cast<float>(view.sun_illuminance);
  u.size[0] = static_cast<float>(width);
  u.size[1] = static_cast<float>(height);
  u.size[2] = 1.0f / static_cast<float>(width);
  u.size[3] = 1.0f / static_cast<float>(height);
  u.time[0] = static_cast<float>(view.time_s);
  u.time[1] = static_cast<float>(view.frame);
  u.world[0] = static_cast<float>(view.altitude_m * 1e-3);
  std::memcpy(u.planes, cam.planes, sizeof cam.planes);
}

FrameTargets create_targets(gpu::Device& device, std::string_view label, uint32_t width,
                            uint32_t height) {
  FrameTargets t;
  t.width = width;
  t.height = height;
  auto make = [&](std::string_view name, wgpu::TextureFormat format, wgpu::TextureUsage usage) {
    const std::string l = std::string(label) + "." + std::string(name);
    wgpu::TextureDescriptor desc{};
    desc.label = wgpu::StringView(l.data(), l.size());
    desc.usage = usage;
    desc.size = {width, height, 1};
    desc.format = format;
    return device.device.CreateTexture(&desc);
  };
  t.hdr = make("hdr", kHdrFormat,
               wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding);
  t.depth = make("depth", kDepthFormat,
                 wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding);
  t.hdr_view = t.hdr.CreateView();
  t.depth_view = t.depth.CreateView();
  return t;
}

namespace {

// The words of a space-separated list.
std::vector<std::string_view> words(std::string_view list) {
  std::vector<std::string_view> out;
  size_t i = 0;
  while (i < list.size()) {
    while (i < list.size() && list[i] == ' ') ++i;
    const size_t j = list.find(' ', i);
    const size_t end = j == std::string_view::npos ? list.size() : j;
    if (end > i) out.push_back(list.substr(i, end - i));
    i = end;
  }
  return out;
}

}  // namespace

std::string check_pass_table(std::span<const PassRow> rows, std::string_view inputs) {
  std::vector<std::string_view> written = words(inputs);
  std::string faults;
  std::vector<std::string_view> names;
  for (const PassRow& row : rows) {
    if (std::find(names.begin(), names.end(), row.name) != names.end()) {
      faults += "render: pass '" + std::string(row.name) +
                "' is listed twice in the pass table; give each row its own name (skill: "
                "render, § A pass).\n";
    }
    names.push_back(row.name);
    for (std::string_view r : words(row.reads)) {
      if (std::find(written.begin(), written.end(), r) == written.end()) {
        faults += "render: pass '" + std::string(row.name) + "' reads '" + std::string(r) +
                  "' before any pass writes it; move the writer above it in the pass table, or "
                  "list the target as made at load (skill: render, § A pass).\n";
      }
    }
    for (std::string_view w : words(row.writes)) written.push_back(w);
  }
  if (names.size() > static_cast<size_t>(kMaxPasses)) {
    faults += "render: the pass table has " + std::to_string(names.size()) +
              " passes, over kMaxPasses (" + std::to_string(kMaxPasses) +
              "); raise it in render/frame.hpp.\n";
  }
  return faults;
}

uint32_t FrameStats::draws() const {
  uint32_t n = 0;
  for (int i = 0; i < count; ++i) n += passes[static_cast<size_t>(i)].draws;
  return n;
}

uint32_t FrameStats::dispatches() const {
  uint32_t n = 0;
  for (int i = 0; i < count; ++i) n += passes[static_cast<size_t>(i)].dispatches;
  return n;
}

void FrameRecorder::init(gpu::Device& device, std::string_view label, int passes) {
  timed = timer.init(device, label, static_cast<uint32_t>(passes));
}

void FrameRecorder::begin_frame(gpu::Device& device) {
  if (timed) timer.ready(device);
  for (int i = 0; i < stats.count; ++i) {
    stats.passes[static_cast<size_t>(i)].gpu_ms = timer.ms(static_cast<uint32_t>(i));
  }
  stats.count = 0;
}

static PassStat& open_stat(FrameStats& stats, std::string_view name) {
  if (stats.count >= kMaxPasses) stats.count = kMaxPasses - 1;  // check_pass_table reports it
  PassStat& p = stats.passes[static_cast<size_t>(stats.count++)];
  p.name = name;
  p.draws = 0;
  p.dispatches = 0;
  return p;
}

wgpu::ComputePassEncoder FrameRecorder::compute(wgpu::CommandEncoder& encoder,
                                                std::string_view name) {
  const uint32_t i = static_cast<uint32_t>(stats.count);
  open_stat(stats, name);
  wgpu::ComputePassDescriptor desc{};
  desc.label = wgpu::StringView(name.data(), name.size());
  desc.timestampWrites = timed ? timer.writes(i) : nullptr;
  return encoder.BeginComputePass(&desc);
}

wgpu::RenderPassEncoder FrameRecorder::render(wgpu::CommandEncoder& encoder,
                                              wgpu::RenderPassDescriptor desc,
                                              std::string_view name) {
  const uint32_t i = static_cast<uint32_t>(stats.count);
  open_stat(stats, name);
  desc.label = wgpu::StringView(name.data(), name.size());
  desc.timestampWrites = timed ? timer.writes(i) : nullptr;
  return encoder.BeginRenderPass(&desc);
}

void FrameRecorder::end_frame(wgpu::CommandEncoder& encoder) {
  if (timed) timer.resolve(encoder);
}

void FrameRecorder::after_submit(gpu::Device& device) {
  if (timed) timer.request(device);
}

void FrameRecorder::wait(gpu::Device& device) {
  if (!timed) return;
  timer.wait(device);
  for (int i = 0; i < stats.count; ++i) {
    stats.passes[static_cast<size_t>(i)].gpu_ms = timer.ms(static_cast<uint32_t>(i));
  }
}

wgpu::BindGroupLayout make_layout(gpu::Device& device, std::string_view label,
                                  std::initializer_list<Slot> slots, wgpu::ShaderStage visibility) {
  std::vector<wgpu::BindGroupLayoutEntry> entries;
  uint32_t i = 0;
  for (Slot s : slots) {
    wgpu::BindGroupLayoutEntry e{};
    e.binding = i++;
    e.visibility = visibility;
    switch (s) {
      case Slot::uniform:
        e.buffer.type = wgpu::BufferBindingType::Uniform;
        break;
      case Slot::storage_read:
        e.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
        break;
      case Slot::storage:
        e.buffer.type = wgpu::BufferBindingType::Storage;
        // A writable storage buffer is not allowed in a vertex shader.
        e.visibility = visibility & ~wgpu::ShaderStage::Vertex;
        break;
      case Slot::sampler:
        e.sampler.type = wgpu::SamplerBindingType::Filtering;
        break;
      case Slot::texture:
        e.texture.sampleType = wgpu::TextureSampleType::Float;
        e.texture.viewDimension = wgpu::TextureViewDimension::e2D;
        break;
      case Slot::texture_raw:
        e.texture.sampleType = wgpu::TextureSampleType::UnfilterableFloat;
        e.texture.viewDimension = wgpu::TextureViewDimension::e2D;
        break;
      case Slot::texture_3d:
        e.texture.sampleType = wgpu::TextureSampleType::Float;
        e.texture.viewDimension = wgpu::TextureViewDimension::e3D;
        break;
      case Slot::store_2d:
      case Slot::store_3d:
        e.storageTexture.access = wgpu::StorageTextureAccess::WriteOnly;
        e.storageTexture.format = wgpu::TextureFormat::RGBA16Float;
        e.storageTexture.viewDimension =
            s == Slot::store_2d ? wgpu::TextureViewDimension::e2D : wgpu::TextureViewDimension::e3D;
        e.visibility = visibility & ~wgpu::ShaderStage::Vertex;
        break;
    }
    entries.push_back(e);
  }
  wgpu::BindGroupLayoutDescriptor desc{};
  desc.label = wgpu::StringView(label.data(), label.size());
  desc.entryCount = entries.size();
  desc.entries = entries.data();
  return device.device.CreateBindGroupLayout(&desc);
}

wgpu::PipelineLayout make_pipeline_layout(gpu::Device& device, std::string_view label,
                                          std::initializer_list<wgpu::BindGroupLayout> groups) {
  std::vector<wgpu::BindGroupLayout> list(groups.begin(), groups.end());
  wgpu::PipelineLayoutDescriptor desc{};
  desc.label = wgpu::StringView(label.data(), label.size());
  desc.bindGroupLayoutCount = list.size();
  desc.bindGroupLayouts = list.data();
  return device.device.CreatePipelineLayout(&desc);
}

wgpu::BindGroup make_group(gpu::Device& device, std::string_view label,
                           const wgpu::BindGroupLayout& layout, std::initializer_list<Bind> binds) {
  std::vector<wgpu::BindGroupEntry> entries;
  uint32_t i = 0;
  for (const Bind& b : binds) {
    wgpu::BindGroupEntry e{};
    e.binding = i++;
    e.buffer = b.buffer;
    e.textureView = b.view;
    e.sampler = b.sampler;
    entries.push_back(e);
  }
  wgpu::BindGroupDescriptor desc{};
  desc.label = wgpu::StringView(label.data(), label.size());
  desc.layout = layout;
  desc.entryCount = entries.size();
  desc.entries = entries.data();
  return device.device.CreateBindGroup(&desc);
}

wgpu::Buffer make_buffer(gpu::Device& device, std::string_view label, uint64_t size,
                         wgpu::BufferUsage usage) {
  wgpu::BufferDescriptor desc{};
  desc.label = wgpu::StringView(label.data(), label.size());
  desc.size = (size + 3) / 4 * 4;
  desc.usage = usage | wgpu::BufferUsage::CopyDst;
  return device.device.CreateBuffer(&desc);
}

}  // namespace ge::render

namespace ge::render {

wgpu::Texture make_texture(gpu::Device& device, std::string_view label, uint32_t width,
                           uint32_t height, uint32_t depth, wgpu::TextureFormat format,
                           wgpu::TextureUsage usage, uint32_t mips) {
  wgpu::TextureDescriptor desc{};
  desc.label = wgpu::StringView(label.data(), label.size());
  desc.usage = usage;
  desc.dimension = depth > 1 ? wgpu::TextureDimension::e3D : wgpu::TextureDimension::e2D;
  desc.size = {width, height, depth};
  desc.format = format;
  desc.mipLevelCount = mips;
  return device.device.CreateTexture(&desc);
}

}  // namespace ge::render
