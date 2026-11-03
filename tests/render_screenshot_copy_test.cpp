// A window's screenshot, its GPU half (engine/render/screenshot_copy.hpp): a frame drawn with no
// screenshot asked is never copied; one asked is copied, handed to the writer with the frame's
// size and colours a frame or two later, and written by the engine's writer to a file the still
// reader takes back; the writer's line reaches the console; a copy destroyed with a picture still
// in flight waits for it (the map's callback holds its address). Skips without an adapter.
//
// ge:summary Tests: the screenshot's GPU half: copied only when asked, handed over, written
// ge:keywords test gpu screenshot screenshot-copy readback writer console frame
// ge:skill render replay testing
// ge:test tests/render_screenshot_copy_test.cpp
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "capture/screenshot.hpp"
#include "capture/still.hpp"
#include "check.hpp"
#include "console/console.hpp"
#include "gpu/target.hpp"
#include "gpu_support.hpp"
#include "render/screenshot_copy.hpp"

namespace fs = std::filesystem;

namespace {

constexpr uint32_t kW = 48, kH = 32;
// A frame's worth of time between the loops' frames: the GPU maps a copy back asynchronously, and
// a loop with no pause runs its frames before the first map completes. Up to two seconds in all.
constexpr int kFrames = 2000;
void next_frame() { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }

void clear(ge::gpu::Device& d, const ge::gpu::Target& t) {
  wgpu::RenderPassColorAttachment colour{};
  colour.view = t.view;
  colour.loadOp = wgpu::LoadOp::Clear;
  colour.storeOp = wgpu::StoreOp::Store;
  colour.clearValue = {200 / 255.0, 60 / 255.0, 30 / 255.0, 1.0};
  wgpu::RenderPassDescriptor pass{};
  pass.colorAttachmentCount = 1;
  pass.colorAttachments = &colour;
  wgpu::CommandEncoder encoder = d.device.CreateCommandEncoder();
  encoder.BeginRenderPass(&pass).End();
  wgpu::CommandBuffer commands = encoder.Finish();
  d.queue.Submit(1, &commands);
}

// A writer that keeps what it is handed.
struct Kept {
  bool asked = false;
  int written = 0;
  uint32_t width = 0, height = 0;
  std::vector<uint8_t> rgba;
  bool due() {
    const bool now = asked;
    asked = false;
    return now;
  }
  void request() { asked = true; }
  bool write(uint32_t w, uint32_t h, std::span<const uint8_t> px) {
    ++written;
    width = w;
    height = h;
    rgba.assign(px.begin(), px.end());
    return true;
  }
  std::vector<std::string> poll() { return {}; }
};

}  // namespace

TEST(a_frame_is_copied_only_when_asked_and_handed_over_whole) {
  auto d = ge::test::test_device();
  if (!d) return;
  const ge::gpu::Target t = ge::gpu::create_target(*d, "screenshot.test", kW, kH);
  ge::console::Console con;
  ge::render::ScreenshotCopy copy;
  Kept kept;
  for (int k = 0; k < 6; ++k) {
    clear(*d, t);
    copy.frame(*d, t, kept, con);
    d->instance.ProcessEvents();
  }
  CHECK(kept.written == 0);  // nothing asked, nothing copied
  kept.request();
  for (int k = 0; k < kFrames && kept.written == 0; ++k) {
    clear(*d, t);
    copy.frame(*d, t, kept, con);
    d->instance.ProcessEvents();
    next_frame();
  }
  CHECK(kept.written == 1);
  CHECK(kept.width == kW && kept.height == kH && kept.rgba.size() == size_t{kW} * kH * 4);
  if (kept.rgba.size() >= 4) CHECK(kept.rgba[0] >= 199 && kept.rgba[1] <= 61 && kept.rgba[2] <= 31);
  CHECK(d->errors.load() == 0);
}

TEST(a_copy_destroyed_in_flight_waits_for_its_picture) {
  auto d = ge::test::test_device();
  if (!d) return;
  const ge::gpu::Target t = ge::gpu::create_target(*d, "screenshot.quit", kW, kH);
  ge::console::Console con;
  Kept kept;
  {
    ge::render::ScreenshotCopy copy;
    kept.request();
    clear(*d, t);
    copy.frame(*d, t, kept, con);  // the copy started, its map not yet back
  }  // ASan holds this: the map's callback must not land on a freed readback
  d->instance.ProcessEvents();
  CHECK(d->errors.load() == 0);
}

TEST(the_engines_writer_takes_it_to_a_file_and_says_so) {
  auto d = ge::test::test_device();
  if (!d) return;
  const fs::path dir = fs::temp_directory_path() / "ge_gpu_screenshot_copy";
  std::error_code ec;
  fs::remove_all(dir, ec);
  const ge::gpu::Target t = ge::gpu::create_target(*d, "screenshot.file", kW, kH);
  ge::console::Console con;
  std::vector<std::string> printed;
  con.set_sink([&printed](std::string_view line) { printed.emplace_back(line); });
  {
    ge::capture::Screenshots shots(dir.string(), "test");
    ge::capture::add_screenshot_command(con, shots, "the test's folder");
    con.execute("screenshot");
    con.run();
    ge::render::ScreenshotCopy copy;
    for (int k = 0; k < kFrames && printed.empty(); ++k) {
      clear(*d, t);
      copy.frame(*d, t, shots, con);
      d->instance.ProcessEvents();
      next_frame();
    }
  }
  CHECK_MSG(printed.size() == 1 && printed[0].find("screenshot: ") == 0,
            printed.empty() ? "nothing printed" : printed[0]);
  std::string file;
  for (const auto& e : fs::directory_iterator(dir, ec)) file = e.path().string();
  CHECK(fs::path(file).extension() == ".jpg");  // the defaults
  std::ifstream in(file, std::ios::binary);
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
  std::string error;
  const auto back = ge::capture::decode_still(bytes, error);
  CHECK_MSG(back && back->width == static_cast<int>(kW) && back->height == static_cast<int>(kH),
            error);
  fs::remove_all(dir, ec);
}
