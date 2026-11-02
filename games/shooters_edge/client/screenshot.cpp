// The `screenshot` command and the frame's copy for it (screenshot.hpp).
//
// ge:summary The screenshot command bound, the frame copied and handed to the writer
// ge:keywords screenshot command readback frame_target poll
// ge:skill replay shooter
// ge:test tests/capture_screenshot_test.cpp
#include "shooters_edge/client/screenshot.hpp"

namespace shooters_edge::client {

std::filesystem::path screenshot_dir(const std::string& basedir, const std::string& game) {
  return std::filesystem::path(basedir.empty() ? "." : basedir) / game / "screenshots";
}

Shots::Shots(ge::console::Console& console, const std::string& basedir, const std::string& game)
    : writer(screenshot_dir(basedir, game).string(), "shooters_edge", &settings) {
  ge::capture::add_screenshot_cvars(console.cvars(), settings);
  console.add_command(
      "screenshot",
      "screenshot: the next frame shown written to <game folder>/screenshots/ in scr_format "
      "(bound to F12)",
      [this](ge::console::Console&, const ge::console::Args&) { writer.request(); });
}

void screenshot_frame(Shots& s, ShotCopy& copy, ge::gpu::Device& device,
                      const ge::gpu::Surface& surface, ge::console::Console& console) {
  // Every slot still in flight: the frame is asked for again rather than lost.
  if (s.writer.due() && !copy.ring.capture(device, surface.frame_target(), 0)) s.writer.request();
  copy.ring.poll(device, copy.picture, [&s](ge::gpu::Image& img, uint64_t) {
    s.writer.write(img.width, img.height, img.rgba);
  });
  for (const std::string& line : s.writer.poll()) console.print(line);
}

}  // namespace shooters_edge::client
