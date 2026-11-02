// A WINDOW'S SCREENSHOT, ITS GPU HALF (docs/replays.md § Screenshots): the frame the window shows
// copied back to the CPU with no stall and handed to the screenshot writer
// (capture/screenshot.hpp), so a game's frame loop makes one call for it.
//
// Once a frame, after the frame is drawn into the surface's texture and before it is presented,
// `frame` asks the writer whether a screenshot is due; when one is, the frame is copied into a ring
// of two readbacks (gpu/readback_ring.hpp: its own command buffer, after the frame's work), and a
// copy that has come back (a frame or two later) is handed to the writer. Both slots in flight:
// the screenshot is asked for again on the next frame rather than lost. The writer's lines (a file
// written, or why not) are printed to the console. Make it after the GPU device, so its buffers
// go before the device does; the writer and its `screenshot` command are made before the console
// runs the command line (capture/screenshot.hpp). Destroyed with a copy still in flight (the
// window closed right after the key), it waits for that copy first: its map's callback holds the
// readback's address, so the device must outlive it, as declaring it after the device makes so.
// That last picture is dropped; the writer has already stopped taking them.
//
// The writer is a template argument (anything with `due`, `request`, `write` and `poll`, as
// capture::Screenshots has), so the frame core takes no link on the capture library; the game
// links both.
//
// ge:summary A window's screenshot: the shown frame copied back with no stall, handed to the writer
// ge:keywords screenshot frame readback ring surface frame_target present no-stall writer
// ge:keywords screenshot-copy window f12
// ge:skill render replay
// ge:test tests/render_screenshot_copy_test.cpp
#pragma once

#include <cstdint>
#include <string>

#include "console/console.hpp"
#include "gpu/readback_ring.hpp"
#include "gpu/target.hpp"

namespace ge::render {

class ScreenshotCopy {
 public:
  ScreenshotCopy() = default;
  ScreenshotCopy(const ScreenshotCopy&) = delete;
  ScreenshotCopy& operator=(const ScreenshotCopy&) = delete;
  ~ScreenshotCopy() {
    if (device_ != nullptr) ring_.drain(*device_, picture_, [](gpu::Image&, uint64_t) {});
  }

  // `shown`: the frame as drawn (`Surface::frame_target()` between acquire and present).
  template <class Writer>
  void frame(gpu::Device& device, const gpu::Target& shown, Writer& writer,
             console::Console& console) {
    device_ = &device;
    if (writer.due() && !ring_.capture(device, shown, 0)) writer.request();
    ring_.poll(device, picture_, [&writer](gpu::Image& img, uint64_t) {
      writer.write(img.width, img.height, img.rgba);
    });
    for (const std::string& line : writer.poll()) console.print(line);
  }

 private:
  gpu::ReadbackRing ring_{2};
  gpu::Image picture_;             // the copy that came back, its storage reused
  gpu::Device* device_ = nullptr;  // the last frame's: what a copy still in flight is waited on
};

}  // namespace ge::render
