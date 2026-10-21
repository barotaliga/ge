// FALL LINE'S APP: the window, the frame loop and the GPU, put together. Each frame: read the
// input, hand the run clock the frame's wall time (engine/loop/run_clock.hpp: focus lost
// pauses it), take the steps it grants (none yet: the simulation is M5), and draw the map's
// frame (frame.hpp) from a named view (views.hpp) at vsync. Until the run exists the camera
// stands still at that view.
//
//   fall_line [--frames=N] [--size=WxH] [--no-vsync] [--seed=N] [--view=NAME] [--hour=H]
//
// --frames quits after N presented frames and prints the frame pace (a smoke run under a
// virtual display: xvfb-run -a build/release/games/fall_line/fall_line --frames=60).
// Escape or closing the window quits.
//
// ge:summary Fall Line's app: the window, the frame loop on the run clock, the GPU frame
// ge:keywords app main frame-loop window vsync present run-clock frame-pace fall-line
// ge:skill platform gpu render
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include "core/output.hpp"
#include "fall_line/app/frame.hpp"
#include "fall_line/app/views.hpp"
#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/surface.hpp"
#include "loop/run_clock.hpp"
#include "mountain/mapgen/generate.hpp"
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
  double seed = 38;
  std::string view = "start";
  double hour = -1;  // the map's own
};

bool parse(int argc, char** argv, Args& a) {
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (arg.rfind("--frames=", 0) == 0) {
      a.frames = std::strtol(argv[i] + 9, nullptr, 10);
    } else if (arg.rfind("--size=", 0) == 0) {
      char* x = nullptr;
      a.width = static_cast<int>(std::strtol(argv[i] + 7, &x, 10));
      if (!x || *x != 'x') return false;
      a.height = static_cast<int>(std::strtol(x + 1, nullptr, 10));
      if (a.width <= 0 || a.height <= 0) return false;
    } else if (arg == "--no-vsync") {
      a.vsync = false;
    } else if (arg.rfind("--seed=", 0) == 0) {
      a.seed = std::strtod(argv[i] + 7, nullptr);
    } else if (arg.rfind("--view=", 0) == 0) {
      a.view = std::string(arg.substr(7));
    } else if (arg.rfind("--hour=", 0) == 0) {
      a.hour = std::strtod(argv[i] + 7, nullptr);
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
        "fall_line: unknown flag. Usage: fall_line [--frames=N] [--size=WxH] [--no-vsync] "
        "[--seed=N] [--view=start|piste|lookback|vista|aerial] [--hour=H]; "
        "next: rerun with those flags only.");
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

  ge::mountain::mapgen::GenerateOptions opts;
  opts.version = fall_line::app::kMapVersion;
  const ge::mountain::mapgen::Level level = ge::mountain::mapgen::generate_level(args.seed, opts);
  ge::render::FrameView view;
  fall_line::app::day_view(level, args.hour, view);
  if (!fall_line::app::make_view(level, args.view, view)) {
    out::error("fall_line: no view named '" + args.view + "'; see games/fall_line/app/views.hpp.");
    return 2;
  }
  auto frame = fall_line::app::create_frame(*device, level, static_cast<uint32_t>(w),
                                            static_cast<uint32_t>(h), surface.format());
  if (!frame) return 1;

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
      fall_line::app::resize(*device, *frame, static_cast<uint32_t>(input.width_px),
                             static_cast<uint32_t>(input.height_px));
    }
    const uint64_t now = ge::platform::now_ns();
    steps += ge::loop::frame(clock, static_cast<double>(now - last) * 1e-9);
    last = now;

    wgpu::TextureView target = surface.acquire(*device);
    if (!target) continue;
    view.time_s = static_cast<double>(now - first) * 1e-9;
    view.frame = static_cast<uint64_t>(presented);
    fall_line::app::draw(*device, *frame, view, target);
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
