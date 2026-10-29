// The target's texture and the readback's padded copy, map and unpadding.
//
// ge:summary Offscreen target creation; readback copy, async map and row unpadding
// ge:keywords gpu readback padded-rows 256-byte-rows bgra-swizzle unmap
// ge:skill gpu
// ge:test tests/gpu_capture_test.cpp
#include "gpu/target.hpp"

#include <cstring>
#include <utility>

#include "core/output.hpp"

namespace ge::gpu {

namespace out = core::output;

Target create_target(Device& device, std::string_view name, uint32_t width, uint32_t height,
                     wgpu::TextureFormat format) {
  Target t;
  t.name = std::string(name);
  t.format = format;
  t.width = width;
  t.height = height;
  wgpu::TextureDescriptor desc{};
  desc.label = wgpu::StringView(t.name.data(), t.name.size());
  desc.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc |
               wgpu::TextureUsage::TextureBinding;
  desc.size = {width, height, 1};
  desc.format = format;
  t.texture = device.device.CreateTexture(&desc);
  t.view = t.texture.CreateView();
  return t;
}

void Readback::copy(Device& device, wgpu::CommandEncoder& encoder, const Target& target) {
  const bool rgba = target.format == wgpu::TextureFormat::RGBA8Unorm ||
                    target.format == wgpu::TextureFormat::RGBA8UnormSrgb;
  const bool bgra = target.format == wgpu::TextureFormat::BGRA8Unorm ||
                    target.format == wgpu::TextureFormat::BGRA8UnormSrgb;
  if (!rgba && !bgra) {
    out::error("gpu: readback of '" + target.name +
               "': only 8-bit RGBA or BGRA targets read back to an Image. Read a float or "
               "depth target through a pass that writes it to RGBA8; skill: gpu.");
    failed_ = true;
    return;
  }
  // A copy's rows are 256-byte aligned; take() drops the padding.
  const uint32_t row = (target.width * 4 + 255) / 256 * 256;
  if (!buffer_ || row != row_bytes_ || target.height != height_) {
    const std::string label = target.name + ".readback";
    wgpu::BufferDescriptor desc{};
    desc.label = wgpu::StringView(label.data(), label.size());
    desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
    desc.size = uint64_t{row} * target.height;
    buffer_ = device.device.CreateBuffer(&desc);
  }
  width_ = target.width;
  height_ = target.height;
  row_bytes_ = row;
  bgra_ = bgra;
  failed_ = false;
  mapped_ = false;
  wgpu::TexelCopyTextureInfo src{};
  src.texture = target.texture;
  wgpu::TexelCopyBufferInfo dst{};
  dst.buffer = buffer_;
  dst.layout.bytesPerRow = row;
  dst.layout.rowsPerImage = target.height;
  const wgpu::Extent3D size{target.width, target.height, 1};
  encoder.CopyTextureToBuffer(&src, &dst, &size);
}

void Readback::request(Device&) {
  if (failed_ || !buffer_) return;
  future_ = buffer_.MapAsync(
      wgpu::MapMode::Read, 0, buffer_.GetSize(), wgpu::CallbackMode::AllowProcessEvents,
      [this](wgpu::MapAsyncStatus status, wgpu::StringView message) {
        if (status == wgpu::MapAsyncStatus::Success) {
          mapped_ = true;
        } else {
          failed_ = true;
          out::error("gpu: readback map failed: " + to_string(message) + "; skill: gpu.");
        }
      });
}

bool Readback::ready(Device& device) {
  if (!mapped_ && !failed_) device.instance.ProcessEvents();
  return mapped_ || failed_;
}

void Readback::wait_mapped(Device& device) {
  if (future_.id != 0 && !mapped_ && !failed_) wait(device, future_);
}

Image Readback::take() {
  Image img;
  take_into(img);
  return img;
}

void Readback::take_into(Image& img) {
  if (!mapped_) {
    img.width = img.height = 0;
    img.rgba.clear();
    return;
  }
  img.width = width_;
  img.height = height_;
  img.rgba.resize(size_t{width_} * height_ * 4);
  const auto* src = static_cast<const uint8_t*>(buffer_.GetConstMappedRange());
  for (uint32_t y = 0; y < height_; ++y) {
    uint8_t* d = &img.rgba[size_t{y} * width_ * 4];
    std::memcpy(d, src + size_t{y} * row_bytes_, size_t{width_} * 4);
    if (bgra_) {
      for (size_t x = 0; x < width_; ++x) std::swap(d[x * 4], d[x * 4 + 2]);
    }
  }
  buffer_.Unmap();
  mapped_ = false;
}

Image read_now(Device& device, const Target& target) {
  Readback rb;
  wgpu::CommandEncoder encoder = device.device.CreateCommandEncoder();
  rb.copy(device, encoder, target);
  wgpu::CommandBuffer commands = encoder.Finish();
  device.queue.Submit(1, &commands);
  rb.request(device);
  rb.wait_mapped(device);
  return rb.take();
}

}  // namespace ge::gpu
