// LAB `frame-pace`: the real window's frame loop timed. It opens a window, presents the GPU
// test scene for N frames (at vsync unless asked not to) and reports how far apart the
// presents landed: the mean, the median, the 99th percentile and the worst interval, and how
// many steps the run clock (engine/loop/run_clock.hpp) took on those real intervals. Every
// number is wall-clock, so all of them are `measured` and the digest holds only the flags.
// In the container it runs under a virtual display (`xvfb-run -a`), which has no vsync: there
// it times the loop's cost, and pacing at vsync is a check on a real display (the Mac). With no
// display it notes that and passes. Built only with GE_PLATFORM and GE_GPU.
//
// ge:summary Lab: the window's frame loop timed (present intervals, percentiles, steps)
// ge:keywords lab frame-pace vsync present interval percentile hitch window frame-loop
// ge:keywords bench run-clock xvfb
// ge:skill platform labs
// ge:test tests/platform_window_test.cpp
#include <algorithm>
#include <string>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/surface.hpp"
#include "gpu/test_scene.hpp"
#include "lab/lab.hpp"
#include "loop/run_clock.hpp"
#include "platform/window.hpp"

namespace {

using ge::lab::Det;

void run(ge::lab::Lab& lab) {
  const int frames = static_cast<int>(lab.arg_num("frames"));
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  const bool vsync = !lab.arg_bool("no-vsync");
  if (frames < 2 || width <= 0 || height <= 0) {
    lab.fail("--frames must be at least 2 and the size positive", "Pass --frames=120.");
    return;
  }
  lab.metric("frames", frames, "count");
  if (!ge::platform::display_available()) {
    lab.note("No display: nothing timed. In the container, run it under xvfb-run -a.");
    return;
  }
  auto window =
      ge::platform::open_window({.title = "frame-pace", .width = width, .height = height});
  auto device = window ? ge::gpu::create_device({.label = "frame-pace"}) : nullptr;
  ge::gpu::Surface surface;
  if (!window || !device || !surface.create(*device, window->native())) {
    lab.note("No window or GPU device could be made: nothing timed (the lines above say why).");
    return;
  }
  int w = 0, h = 0;
  window->pixel_size(w, h);
  surface.configure(*device, static_cast<uint32_t>(w), static_cast<uint32_t>(h), vsync);
  ge::gpu::PipelineSet manifest;
  const uint32_t scene = ge::gpu::add_test_scene(*device, manifest, surface.format());
  if (!manifest.finish(*device)) {
    lab.fail("the test scene's pipeline did not build", "Read the gpu: lines above. Skill: gpu.");
    return;
  }
  ge::gpu::end_load(*device);

  ge::loop::RunClock clock = ge::loop::create_run_clock(120);
  ge::platform::InputSnapshot input;
  std::vector<double> gaps;
  gaps.reserve(static_cast<size_t>(frames));
  long steps = 0;
  int skipped = 0;
  uint64_t last = ge::platform::now_ns();
  for (int f = 0; f < frames; ++f) {
    ge::platform::poll_input(*window, input);
    wgpu::TextureView target = surface.acquire(*device);
    if (!target) {
      ++skipped;
      continue;
    }
    wgpu::CommandEncoder encoder = device->device.CreateCommandEncoder();
    ge::gpu::draw_test_scene(encoder, manifest.render(scene), target);
    wgpu::CommandBuffer commands = encoder.Finish();
    device->queue.Submit(1, &commands);
    surface.present();
    const uint64_t now = ge::platform::now_ns();
    const double gap = static_cast<double>(now - last) * 1e-9;
    last = now;
    steps += ge::loop::frame(clock, gap);
    if (f > 0) gaps.push_back(gap * 1e3);  // the first interval includes the setup
  }
  if (gaps.empty()) {
    lab.fail("no frame was presented", "Read the gpu: lines above. Skill: platform.");
    return;
  }
  std::vector<double> sorted = gaps;
  std::sort(sorted.begin(), sorted.end());
  double sum = 0;
  for (const double g : gaps) sum += g;
  const auto at = [&](double q) {
    return sorted[std::min(sorted.size() - 1, static_cast<size_t>(q * double(sorted.size())))];
  };
  const double mean = sum / static_cast<double>(gaps.size());
  lab.metric("interval.mean", mean, "ms", Det::measured);
  lab.metric("interval.p50", at(0.5), "ms", Det::measured);
  lab.metric("interval.p99", at(0.99), "ms", Det::measured);
  lab.metric("interval.max", sorted.back(), "ms", Det::measured);
  lab.metric("fps", mean > 0 ? 1e3 / mean : 0, "fps", Det::measured);
  lab.metric("steps-per-frame", static_cast<double>(steps) / frames, "steps", Det::measured);
  lab.metric("skipped-frames", skipped, "count", Det::measured);
  lab.metric("gpu-errors", device->errors.load(), "count", Det::measured);
  lab.note("adapter: " + device->report.name + "; " + (vsync ? "vsync (FIFO)" : "no vsync") +
           ". A virtual display has no vsync: there the intervals are the loop's cost.");
  if (device->errors.load() != 0) {
    lab.fail(std::to_string(device->errors.load()) + " GPU errors in the loop",
             "Read the gpu: lines above. Skill: gpu.");
  }
}

const ge::lab::LabSpec kSpec{
    "frame-pace",
    ge::lab::LabKind::bench,
    "engine",
    "engine/platform/window.hpp, engine/gpu/surface.hpp, the frame loop",
    "platform",
    "How evenly does the window's frame loop present, and how many steps does each frame "
    "take? (Pacing at vsync needs a real display: the Mac.)",
    {
        {"frames", "240", "frames to present"},
        {"width", "640", "the window's width"},
        {"height", "360", "the window's height"},
        {"no-vsync", "", "present as fast as the adapter allows", true},
    },
    "--frames=30 --width=160 --height=90",
    run,
};
GE_LAB(kSpec);

}  // namespace
