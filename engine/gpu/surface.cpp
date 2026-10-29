// The surface's creation from each kind of native handle, its format and present-mode choice,
// and the stale-surface recovery.
//
// ge:summary Surface creation per window system, format and present-mode choice, recovery
// ge:keywords gpu surface-source-xlib surface-source-wayland surface-source-metal-layer
// ge:keywords surface-source-windows-hwnd get-current-texture outdated lost
// ge:skill gpu
// ge:test tests/platform_window_test.cpp
#include "gpu/surface.hpp"

#include "core/output.hpp"

namespace ge::gpu {

namespace out = core::output;

bool Surface::create(Device& device, const platform::NativeWindow& w) {
  using Kind = platform::NativeWindow::Kind;
  wgpu::SurfaceDescriptor desc{};
  wgpu::SurfaceSourceXlibWindow xlib{};
  wgpu::SurfaceSourceWaylandSurface wayland{};
  wgpu::SurfaceSourceMetalLayer metal{};
  wgpu::SurfaceSourceWindowsHWND hwnd{};
  switch (w.kind) {
    case Kind::x11:
      xlib.display = w.display;
      xlib.window = w.x11_window;
      desc.nextInChain = &xlib;
      break;
    case Kind::wayland:
      wayland.display = w.display;
      wayland.surface = w.surface;
      desc.nextInChain = &wayland;
      break;
    case Kind::metal:
      metal.layer = w.metal_layer;
      desc.nextInChain = &metal;
      break;
    case Kind::win32:
      hwnd.hinstance = w.instance;
      hwnd.hwnd = w.surface;
      desc.nextInChain = &hwnd;
      break;
    case Kind::none:
      out::error(
          "gpu: a surface for a window with no native handles (Kind::none); the "
          "platform layer could not read them, see its error line; skill: platform.");
      return false;
  }
  desc.label = "ge.surface";
  surface_ = device.instance.CreateSurface(&desc);
  if (!surface_) {
    out::error("gpu: CreateSurface failed for the window's handles; skill: gpu.");
    return false;
  }
  wgpu::SurfaceCapabilities caps;
  surface_.GetCapabilities(device.adapter, &caps);
  format_ = caps.formatCount ? caps.formats[0] : wgpu::TextureFormat::BGRA8Unorm;
  for (size_t i = 0; i < caps.formatCount; ++i) {
    // Linear light is encoded by the post pass, so the surface takes its bytes as written.
    if (caps.formats[i] == wgpu::TextureFormat::BGRA8Unorm ||
        caps.formats[i] == wgpu::TextureFormat::RGBA8Unorm) {
      format_ = caps.formats[i];
      break;
    }
  }
  return true;
}

void Surface::configure(Device& device, uint32_t width, uint32_t height, bool vsync) {
  width_ = width;
  height_ = height;
  vsync_ = vsync;
  if (width == 0 || height == 0) return;  // minimised: nothing to present to
  mode_ = wgpu::PresentMode::Fifo;
  if (!vsync) {
    wgpu::SurfaceCapabilities caps;
    surface_.GetCapabilities(device.adapter, &caps);
    for (const wgpu::PresentMode want :
         {wgpu::PresentMode::Mailbox, wgpu::PresentMode::Immediate}) {
      bool has = false;
      for (size_t i = 0; i < caps.presentModeCount; ++i) has = has || caps.presentModes[i] == want;
      if (has) {
        mode_ = want;
        break;
      }
    }
  }
  wgpu::SurfaceConfiguration config{};
  config.device = device.device;
  config.format = format_;
  config.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
  config.width = width;
  config.height = height;
  config.presentMode = mode_;
  surface_.Configure(&config);
}

wgpu::TextureView Surface::acquire(Device& device) {
  current_ = nullptr;
  if (width_ == 0 || height_ == 0) return nullptr;
  wgpu::SurfaceTexture st;
  surface_.GetCurrentTexture(&st);
  switch (st.status) {
    case wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal:
    case wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal:
      current_ = st.texture;
      return st.texture.CreateView();
    case wgpu::SurfaceGetCurrentTextureStatus::Timeout:
      return nullptr;
    case wgpu::SurfaceGetCurrentTextureStatus::Outdated:
    case wgpu::SurfaceGetCurrentTextureStatus::Lost:
      configure(device, width_, height_, vsync_);
      return nullptr;
    default:
      out::error(
          "gpu: the surface's texture could not be acquired (an error status); the "
          "device's error line says why; skill: gpu.");
      return nullptr;
  }
}

void Surface::present() {
  surface_.Present();
  current_ = nullptr;
}

Target Surface::frame_target() const {
  Target t;
  t.name = "surface";
  t.texture = current_;
  t.format = format_;
  t.width = width_;
  t.height = height_;
  return t;
}

}  // namespace ge::gpu
