// FALL LINE'S APP: the window, the frame loop and the GPU, put together. Each frame: read the
// input, hand the run clock the frame's wall time (engine/loop/run_clock.hpp: focus lost
// pauses it), take the steps it grants (none yet: the simulation is M5), and draw. Until the
// frame core draws the mountain (M7) the frame is the GPU test scene, the sky gradient and the
// triangle, presented at vsync.
//
//   fall_line [--frames=N] [--size=WxH] [--no-vsync]
//
// --frames quits after N presented frames and prints the frame pace (a smoke run under a
// virtual display: xvfb-run -a build/release/games/fall_line/fall_line --frames=60).
// Escape or closing the window quits.
//
// ge:summary Fall Line's app: the window, the frame loop on the run clock, the GPU frame
// ge:keywords app main frame-loop window vsync present run-clock frame-pace fall-line
// ge:skill platform gpu
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include "core/output.hpp"
#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/surface.hpp"
#include "gpu/test_scene.hpp"
#include "loop/run_clock.hpp"
#include "platform/window.hpp"

namespace {

namespace out = ge::core::output;

void print_line(ge::core::output::Level level, std::string_view message, void*) {
  std::FILE* to = level == ge::core::output::Level::error ? stderr : stdout;
  std::fprintf(to, "%.*s\n", static_cast<int>(message.size()), message.data());
}

struct Args {
  long frames = 0;  // 0: until quit
  int width = 1280, height = 720;
  bool vsync = true;
};

bool parse(int argc, char** argv, Args& a) {
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (arg.rfind("--frames=", 0) == 0) {
      a.frames = std::strtol(argv[i] + 9, nullptr, 10);
    } else if (arg.rfind("--size=", 0) == 0) {
      if (std::sscanf(argv[i] + 7, "%dx%d", &a.width, &a.height) != 2) return false;
    } else if (arg == "--no-vsync") {
      a.vsync = false;
    } else {
      return false;
    }
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  out::set_output_sink(print_line);
  Args args;
  if (!parse(argc, argv, args)) {
    out::error(
        "fall_line: unknown flag. Usage: fall_line [--frames=N] [--size=WxH] "
        "[--no-vsync]");
    return 2;
  }
  auto window =
      ge::platform::open_window({.title = "Fall Line", .width = args.width, .height = args.height});
  if (!window) return 1;
  auto device = ge::gpu::create_device();
  if (!device) return 1;
  ge::gpu::Surface surface;
  if (!surface.create(*device, window->native())) return 1;
  int w = 0, h = 0;
  window->pixel_size(w, h);
  surface.configure(*device, static_cast<uint32_t>(w), static_cast<uint32_t>(h), args.vsync);

  ge::gpu::PipelineSet manifest;
  const uint32_t scene = ge::gpu::add_test_scene(*device, manifest, surface.format());
  if (!manifest.finish(*device)) return 1;
  ge::gpu::end_load(*device);

  ge::loop::RunClock clock = ge::loop::create_run_clock(120);
  ge::platform::InputSnapshot input;
  uint64_t last = ge::platform::now_ns();
  const uint64_t first = last;
  long presented = 0;
  long steps = 0;
  while (args.frames == 0 || presented < args.frames) {
    ge::platform::poll_input(*window, input);
    if (input.quit || input.tapped(ge::platform::Key::escape)) break;
    if (input.focus_lost) ge::loop::pause(clock);
    if (input.focus_gained) ge::loop::resume(clock);
    if (input.resized) {
      surface.configure(*device, static_cast<uint32_t>(input.width_px),
                        static_cast<uint32_t>(input.height_px), args.vsync);
    }
    const uint64_t now = ge::platform::now_ns();
    steps += ge::loop::frame(clock, static_cast<double>(now - last) * 1e-9);
    last = now;

    wgpu::TextureView target = surface.acquire(*device);
    if (!target) continue;
    wgpu::CommandEncoder encoder = device->device.CreateCommandEncoder();
    ge::gpu::draw_test_scene(encoder, manifest.render(scene), target);
    wgpu::CommandBuffer commands = encoder.Finish();
    device->queue.Submit(1, &commands);
    surface.present();
    ++presented;
  }
  const double seconds = static_cast<double>(ge::platform::now_ns() - first) * 1e-9;
  char line[160];
  std::snprintf(line, sizeof line,
                "fall_line: %ld frames in %.2f s (%.1f ms a frame), %ld steps, %u GPU errors",
                presented, seconds,
                presented ? seconds * 1e3 / static_cast<double>(presented) : 0.0, steps,
                device->errors.load());
  out::status(line);
  return device->errors.load() == 0 && !device->lost.load() ? 0 : 1;
}
