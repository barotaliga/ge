// THE WINDOW'S SURFACE: what a frame is presented to. Made from the platform's
// `NativeWindow` handles (engine/platform/window.hpp; no SDL here), configured to the
// window's pixel size, and presented at vsync (FIFO, which every adapter has) unless asked
// otherwise. A surface that went stale (resized, minimised, its swapchain lost) is
// reconfigured by `acquire`, which then skips that frame. Between `acquire` and `present` the
// frame's texture is a `Target` a readback can copy (`frame_target`; the live capture's ring,
// gpu/readback_ring.hpp): the surface is configured with CopySrc for it.
//
// ge:summary The window surface: made from native handles, configured, acquired, presented
// ge:keywords gpu surface swapchain present vsync fifo mailbox configure acquire resize
// ge:keywords native-window x11 wayland metal-layer hwnd surface-format
// ge:skill gpu platform
// ge:test tests/platform_window_test.cpp
#pragma once

#include <cstdint>

#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "platform/window.hpp"

namespace ge::gpu {

class Surface {
 public:
  // Make the surface for `window`. False after an error line when the handles are unusable.
  bool create(Device& device, const platform::NativeWindow& window);
  // (Re)configure to a pixel size. `vsync` false prefers mailbox, then immediate.
  void configure(Device& device, uint32_t width, uint32_t height, bool vsync = true);

  // This frame's texture to draw into; null when the frame must be skipped (the surface was
  // stale and has been reconfigured, or the window has no area).
  wgpu::TextureView acquire(Device& device);
  void present();
  // The acquired frame's texture as a target (a null texture outside acquire..present).
  Target frame_target() const;

  wgpu::TextureFormat format() const { return format_; }
  uint32_t width() const { return width_; }
  uint32_t height() const { return height_; }

 private:
  wgpu::Surface surface_;
  wgpu::Texture current_;  // the acquired frame's texture, until present
  wgpu::TextureFormat format_ = wgpu::TextureFormat::Undefined;
  wgpu::PresentMode mode_ = wgpu::PresentMode::Fifo;
  uint32_t width_ = 0, height_ = 0;
  bool vsync_ = true;
};

}  // namespace ge::gpu
