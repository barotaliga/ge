// OFFSCREEN TARGETS AND READBACK: a frame drawn with no window (a lab, a test) and its pixels
// brought back to the CPU (docs/rendering.md § Lab hooks).
//
// A `Target` is a named colour texture a pass renders into and a copy can read. A `Readback`
// copies one target into a mappable buffer inside the frame's own command encoder, then maps
// it after the submit and hands the pixels over once the map completes: a frame or more later,
// never stalling the frame that asked. `read_now` is the blocking form, for tests and lab
// captures that have nothing else to do while they wait.
//
// Pixels come back as RGBA8, rows top to bottom with no padding: a target in RGBA8Unorm (or
// its sRGB twin) reads as is, BGRA8Unorm (a surface's usual format) is swizzled to RGBA.
//
// ge:summary Offscreen render targets and their readback to RGBA8 images (lab captures)
// ge:keywords gpu target offscreen render-target texture readback capture screenshot image
// ge:keywords rgba8 map-async copy-texture-to-buffer bytes-per-row create_target read_now
// ge:skill gpu
// ge:test tests/gpu_capture_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "gpu/device.hpp"

namespace ge::gpu {

// An RGBA8 picture, rows top to bottom, 4 bytes a pixel, no padding.
struct Image {
  uint32_t width = 0;
  uint32_t height = 0;
  std::vector<uint8_t> rgba;
};

struct Target {
  std::string name;  // the texture's label; what a capture is called
  wgpu::Texture texture;
  wgpu::TextureView view;
  wgpu::TextureFormat format = wgpu::TextureFormat::RGBA8Unorm;
  uint32_t width = 0;
  uint32_t height = 0;
};

// A colour target a pass can draw into, a shader can sample and a readback can copy.
Target create_target(Device& device, std::string_view name, uint32_t width, uint32_t height,
                     wgpu::TextureFormat format = wgpu::TextureFormat::RGBA8Unorm);

class Readback {
 public:
  // Record the copy of `target` into `encoder` (before it is finished and submitted).
  void copy(Device& device, wgpu::CommandEncoder& encoder, const Target& target);
  // After the submit that carried the copy: start mapping.
  void request(Device& device);
  // Block until the map completes (read_now's wait; never in a frame).
  void wait_mapped(Device& device);
  // Whether the pixels are here (processes the device's events; never blocks).
  bool ready(Device& device);
  // The pixels, once ready; unmaps so the readback can be reused.
  Image take();
  // The same into `img`, reusing its storage (a frame's capture allocates nothing once warm).
  void take_into(Image& img);

 private:
  wgpu::Buffer buffer_;
  uint32_t width_ = 0, height_ = 0, row_bytes_ = 0;
  wgpu::Future future_{};
  bool bgra_ = false;
  bool mapped_ = false;
  bool failed_ = false;
};

// Copy `target` back now, waiting for the GPU. Tests and lab captures only, never in a frame.
Image read_now(Device& device, const Target& target);

}  // namespace ge::gpu
