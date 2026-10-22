// A real window (engine/platform/window.hpp, engine/gpu/surface.hpp): it opens, its handles
// make a surface, and frames are drawn and presented at vsync with no GPU error. It needs a
// display, so it skips without one; CI's Linux x64 rows run it under a virtual X display
// (xvfb-run) on lavapipe with GE_GPU_REQUIRE=1, where a skip fails.
//
// ge:summary Tests for the window: open, surface, present at vsync, input poll
// ge:keywords test window surface present vsync xvfb display input poll
// ge:skill platform gpu testing
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "check.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/surface.hpp"
#include "gpu/test_scene.hpp"
#include "gpu_support.hpp"
#include "platform/window.hpp"

TEST(a_window_presents_frames) {
  if (!ge::platform::display_available()) {
    if (ge::test::env_on("GE_GPU_REQUIRE")) {
      ge::test::fail(__FILE__, __LINE__, "no display, and GE_GPU_REQUIRE=1: run under xvfb-run -a");
    } else {
      ge::test::skip("no display to open a window on (xvfb-run -a provides one)");
    }
    return;
  }
  // A virtual X server starved of CPU (CI runs the tests four at a time) now and then refuses a
  // connection ("Failed to connect to the X11 display server") right after the probe above
  // closed one; a few tries a moment apart ride that out, and a display that never accepts
  // still fails here.
  std::unique_ptr<ge::platform::Window> window;
  for (int attempt = 0; attempt < 5 && !window; ++attempt) {
    if (attempt > 0) std::this_thread::sleep_for(std::chrono::milliseconds(200));
    window =
        ge::platform::open_window({.title = "platform_window_test", .width = 320, .height = 200});
  }
  CHECK(window != nullptr);
  if (!window) return;
  CHECK(window->native().kind != ge::platform::NativeWindow::Kind::none);
  auto d = ge::test::test_device();
  if (!d) return;
  ge::gpu::Surface surface;
  CHECK(surface.create(*d, window->native()));
  int w = 0, h = 0;
  window->pixel_size(w, h);
  CHECK(w > 0 && h > 0);
  surface.configure(*d, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
  ge::gpu::PipelineSet manifest;
  const uint32_t scene = ge::gpu::add_test_scene(*d, manifest, surface.format());
  CHECK(manifest.finish(*d));
  ge::gpu::end_load(*d);

  ge::platform::InputSnapshot input;
  int presented = 0;
  const uint64_t start = ge::platform::now_ns();
  for (int frame = 0; frame < 20; ++frame) {
    ge::platform::poll_input(*window, input);
    wgpu::TextureView target = surface.acquire(*d);
    if (!target) continue;
    wgpu::CommandEncoder encoder = d->device.CreateCommandEncoder();
    ge::gpu::draw_test_scene(encoder, manifest.render(scene), target);
    wgpu::CommandBuffer commands = encoder.Finish();
    d->queue.Submit(1, &commands);
    surface.present();
    ++presented;
  }
  CHECK(ge::platform::now_ns() > start);
  CHECK(presented >= 18);  // a stale surface may cost a frame or two while it settles
  CHECK(input.width_px == w && input.height_px == h);
  CHECK(d->errors.load() == 0);
  CHECK(d->pipelines_after_load == 0);
}
